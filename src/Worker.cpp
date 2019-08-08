#include <iostream>
#include <arpa/inet.h>
#include <parsebgp.h>
#include "Worker.h"

Worker::Worker() {
    // get config instance
    config = Config::get_config();
    // set default status to waiting
    status = WORKER_STATUS_WAITING;
    // set debug flag
    debug = (config->debug_worker | config->debug_all);
    // get logger instance
    logger = Logger::get_logger();
    topic_builder = TopicBuilder();
}

double Worker::rib_dump_rate() {
    std::cout << "rib dump rate is not implemented yet." << std::endl;
}

void Worker::start(int obmp_server_tcp_socket, bool is_ipv4_socket) {
    // start SockBuffer
    sock_buffer.start(obmp_server_tcp_socket, is_ipv4_socket);

    // initialize router information
    router_ip = sock_buffer.get_router_ip();

    // get read fd
    reader_fd = sock_buffer.get_reader_fd();

    // change worker status to RUNNING
    status = WORKER_STATUS_RUNNING;

    /* worker now consumes bmp data from pipe socket (read_fd) to parse bmp msgs. */
    work_thread = thread(&Worker::work, this);

    if (debug) LOG_INFO("a worker started.");
}

// set running flag to false
void Worker::stop() {
    status = WORKER_STATUS_STOPPED;
    /* the worker has been notified to stop working, time to clean up. */

    if (debug) LOG_INFO("a worker stopped.");
    // join worker
    work_thread.join();
}

// return if the worker is running
bool Worker::is_running() {
    return status == WORKER_STATUS_RUNNING;
}

bool Worker::is_waiting() {
    return status == WORKER_STATUS_WAITING;
}

bool Worker::has_stopped() {
    return status == WORKER_STATUS_STOPPED;
}

void Worker::work() {
    /* each iteration populates:
     * 1) a raw bmp message with its length.
     * 2) variables required to build a corresponding openbmp kafka topic
     *  and a custom binary header that encapsulates the raw bmp message.
     */
    /* CASE: INIT msg
     *  should happen once normally unless the bmp router changes its information
     *  this msg can contain router sysname, sysdesc. */
    /* From rfc7854:
     *  "The initiation message consists of the common BMP header followed by
     *  two or more Information TLVs (Section 4.4) containing information
     *  about the monitored router.  The sysDescr and sysName Information
     *  TLVs MUST be sent, any others are optional." */

    /*
     * CASE: TERM msg
     * close tcp socket and stop working
     */

    /* CASE: if msg is contains PEER header (which is all other cases really.)
     *  peer header can contain distinguisher id, ip addr, asn, and bgp id.
     *  if {{peer_group}} is used by bmp_raw topic in the config file,
     *  we will check if there is a peer_group match for this peer. */

    while (status == WORKER_STATUS_RUNNING) {
        parsebgp_error_t err = parser.parse(get_unread_buffer(), get_bmp_data_unread_len());
        if (err == PARSEBGP_OK) {
            // parser returns the length of the raw bmp message
            int raw_bmp_msg_len = parser.get_raw_bmp_msg_len();
            // get parsed msg
            parsebgp_bmp_msg_t *parsed_bmp_msg = parser.get_parsed_bmp_msg();

            // TODO: handle init and term bmp msgs
            if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_INIT_MSG) {
                LOG_INFO("received init msg.");
                router_init = true;
            } else if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_TERM_MSG) {
                LOG_INFO("received term msg.");
                router_init = false;
                status = WORKER_STATUS_STOPPED;
            }

            int mapping[] = {-1, AF_INET, AF_INET6};
            char ip_buf[INET6_ADDRSTRLEN] = "[invalid IP]";
            if (parsed_bmp_msg->peer_hdr.afi)
                inet_ntop(mapping[parsed_bmp_msg->peer_hdr.afi],
                        parsed_bmp_msg->peer_hdr.addr, ip_buf, INET6_ADDRSTRLEN);
            string peer_ip(ip_buf);
            topic_builder.get_raw_bmp_topic_string(router_ip, peer_ip, parsed_bmp_msg->peer_hdr.asn);
            /* TODO:
             *  1. topicbuilder builds the right topic string
             *  2. encapsulator builds encapsulated message by using the raw bmp message.
             *  3. message bus sends the encapsulated message to the right topic. */

            // update buffer pointers
            update_buffer(raw_bmp_msg_len);
        } else if (err == PARSEBGP_PARTIAL_MSG) {
            // recv data from socket 1 byte at a time until a bmp init msg is received.
            // this allows us to receive the entire init msg faster (as soon as the router is connected).
            /* TODO: may be a bug here, recv() will continue to block until, e.g. all 1500 bytes are received.
             *          which means if the ringbuffer cannot fulfill the last 1500 bytes before it terminates,
             *          the worker stucks at recv();  unless recv() returns a smaller amount of bytes
             *          when it detects the connection has been closed.*/
            refill_buffer(router_init ? 1500 : 1);
        } else {
            LOG_INFO("stopping the worker, something serious happened -- %d", err);
            // set worker status to stopped so the main thread can clean up.
            status = WORKER_STATUS_STOPPED;
            break;
        }
    }

    // worker stopped for some reason
    // stop sock_buffer to disconnect with the connected bmp router
    sock_buffer.stop();
}

// recv_len affects how soon we need to refill the buffer;
// the larger the value, the less frequent the refills or memmove calls.
void Worker::refill_buffer(int recv_len) {
    // move unread buffer to the beginning of bmp_data_buffer
    memmove(bmp_data_buffer, get_unread_buffer(), get_bmp_data_unread_len());
    // clear read len
    bmp_data_read_len = 0;

    // make sure no buffer overflow
    if ((get_bmp_data_unread_len() + recv_len) < BMP_MSG_BUF_SIZE) {
        int received_bytes = 0;
        // append data to the buffer
        received_bytes = recv(reader_fd,get_unread_buffer() + get_bmp_data_unread_len(),
                recv_len,MSG_WAITALL);
        if (received_bytes <= 0) {
            LOG_INFO("bad connection");
            // set worker status to stopped. the main thread will clean up later.
            status = WORKER_STATUS_STOPPED;
        }
        bmp_data_unread_len += received_bytes;
    }

}

void Worker::update_buffer(int parsed_bmp_msg_len) {
    bmp_data_read_len += parsed_bmp_msg_len;
    bmp_data_unread_len -= parsed_bmp_msg_len;
    assert(bmp_data_read_len >= 0);
    assert(bmp_data_unread_len >= 0);
}

int Worker::get_bmp_data_unread_len() {
    return bmp_data_unread_len;
}

uint8_t *Worker::get_unread_buffer() {
    return bmp_data_buffer + bmp_data_read_len;
}

