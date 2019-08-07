#include <iostream>
#include <arpa/inet.h>
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
}

double Worker::rib_dump_rate() {
    std::cout << "rib dump rate is not implemented yet." << std::endl;
}

void Worker::start(int obmp_server_tcp_socket, bool is_ipv4_socket) {
    // start SockBuffer
    sock_buffer.start(obmp_server_tcp_socket, is_ipv4_socket);
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
            int read_len = parser.get_parsed_len();
            update_buffer(read_len);
        } else if (err == PARSEBGP_PARTIAL_MSG) {
            refill_buffer();
        } else {
            LOG_INFO("stopping the worker, something serious happened -- %d", err);
            // set worker status to stopped so the main thread can clean up.
            status = WORKER_STATUS_STOPPED;
            break;
        }
    }

    // worker stopped for some reason.
    // stop sock_buffer, it disconnects with the connected router.
    sock_buffer.stop();
}

void Worker::refill_buffer() {
    // this value affects how soon we need to refill the buffer;
    // the larger the value, the less frequent the refills.
    int avg_bmp_msg_len = 1500;
    // move unread buffer to the beginning of bmp_data_buffer
    memmove(bmp_data_buffer, get_unread_buffer(), get_bmp_data_unread_len());
    // clear read len
    bmp_data_read_len = 0;

    // make sure no buffer overflow
    if ((get_bmp_data_unread_len() + avg_bmp_msg_len) < BMP_MSG_BUF_SIZE) {
        int received_bytes = 0;
        // append data to the buffer
        received_bytes = recv(reader_fd,get_unread_buffer() + get_bmp_data_unread_len(),
                avg_bmp_msg_len,MSG_WAITALL);
        if (received_bytes <= 0) {
            LOG_INFO("bad connection");
            // set worker status to stopped so the main thread can clean up.
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
