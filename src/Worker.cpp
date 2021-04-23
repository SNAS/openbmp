/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <iostream>
#include <netinet/in.h>
#include <sys/time.h>
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
    // get msg bus instance
    msg_bus = MessageBus::get_message_bus();
}

bool Worker::has_rib_dump_started() {
    return router_rib_dump_started;
}

double Worker::rib_dump_rate() {
    std::cout << "rib dump rate is not implemented yet." << std::endl;
    return .0;
}

void Worker::start(int obmp_server_tcp_socket, bool is_ipv4_socket) {
    // start SockBuffer
    sock_buffer.start(obmp_server_tcp_socket, is_ipv4_socket);

    // initialize router information
    is_router_ip_ipv4 = is_ipv4_socket;
    router_ip = sock_buffer.get_router_ip();
    sock_buffer.get_router_ip_raw(router_ip_raw);
    router_hostname = Utility::resolve_ip(router_ip);
    topic_builder = new TopicBuilder(router_ip, router_hostname);
    router_group = topic_builder->get_router_group();

    encapsulator = new Encapsulator(router_ip_raw, is_router_ip_ipv4, router_group);

    // get read fd
    reader_fd = sock_buffer.get_reader_fd();

    // change worker status to RUNNING
    status = WORKER_STATUS_RUNNING;

    /* worker now consumes bmp data from pipe socket (read_fd) to parse bmp msgs. */
    work_thread = thread(&Worker::work, this);

    if (debug) DEBUG("a worker started.");
}

void Worker::stop() {
    // worker stopped for some reason
    // stop sock_buffer to disconnect with the connected bmp router
    sock_buffer.stop();
    /* the worker has been notified to stop working, time to clean up. */
    status = WORKER_STATUS_STOPPED;
    if (debug) DEBUG("a worker stopped.");
    // join worker
    work_thread.join();
    delete topic_builder;
    delete encapsulator;
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
     * 2) variables required to get_encap_msg a corresponding openbmp kafka topic
     *  and a custom binary header that encapsulates the raw bmp message.
     */

    /* A way to tell that the worker has received a INIT msg from a router
     *  but is waiting for RIB dump.
     *  There is a typically 30 second window before a router start to dump RIB table,
     *  if a collector has many router connections at once,
     *  the cpu usage monitor alone cannot help to determine
     *  if the collector should accept the connections.
     *
     *  ASSUMPTION:
     *  once the count value is >= 2, it means the router started to dump its RIB
     */
    int bmp_msg_count = 0;

//    FILE *fp = fopen("/tmp/bmp_dump/dump_frr_router_from_worker.txt", "w");

    while (status == WORKER_STATUS_RUNNING) {
        parsebgp_error_t err = parser.parse(get_unread_buffer(), get_bmp_data_unread_len());

        /*
        if (get_bmp_data_unread_len() > 0) {
            fwrite(get_unread_buffer(), 1, get_bmp_data_unread_len(), fp);
            status = WORKER_STATUS_STOPPED;
            break;
        }
         */

        if (err == PARSEBGP_OK) {

            // increase bmp_msg_count up to 2 (an arbitrary decision)
            if (router_init & (bmp_msg_count >= 2))
                // if true, it means rib dump has started.
                router_rib_dump_started = true;
            else {
                bmp_msg_count++;
            }

            // parser returns the length of the raw bmp message
            int raw_bmp_msg_len = parser.get_raw_bmp_msg_len();
            // get parsed msg
            parsebgp_bmp_msg_t *parsed_bmp_msg = parser.get_parsed_bmp_msg();

            // 1. topicbuilder builds the right topic string (done)
            string peer_ip;
            parser.get_peer_ip(peer_ip);
            string topic_string = topic_builder->get_raw_bmp_topic_string(peer_ip, parser.get_peer_asn());

            // 2. encapsulator builds encapsulated message by using the raw bmp message.
            timeval cap_time;
            gettimeofday(&cap_time, nullptr);
            encapsulator->build_encap_bmp_msg(get_unread_buffer(), raw_bmp_msg_len, cap_time);
            uint8_t* encap_msg = encapsulator->get_encap_bmp_msg();
            int encap_msg_size = encapsulator->get_encap_bmp_msg_size();
            void * encap_key = encapsulator->get_router_hash_id();
            size_t encap_key_len = 16; /* size of MD5 hash */

            // 3. message bus sends the encapsulated message to the right topic. */
            int64_t msg_time = (int64_t) ((int64_t) cap_time.tv_sec * 1000 + (int64_t) cap_time.tv_usec / 1000);
            msg_bus->send(topic_string, encap_msg, encap_msg_size, encap_key, encap_key_len, msg_time);

            // we now handle bmp init and term msg
            if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_INIT_MSG) {
                /* CASE: INIT msg
                 *  should happen once normally unless the bmp router changes its information
                 *  this msg can contain router sysname, sysdesc. */
                LOG_INFO("received init msg.");
                router_init = true;
            } else if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_TERM_MSG) {
                /*
                 * CASE: TERM msg
                 * close tcp socket and stop working
                 */
                LOG_INFO("received term msg.");
                router_init = false;
                status = WORKER_STATUS_STOPPED;
            } else if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_PEER_UP) {
                /*
                 * CASE: TERM msg
                 * close tcp socket and stop working
                 */
                LOG_INFO("received peer up   message from router: %s for peer: %s",router_ip.c_str(), peer_ip.c_str());
            } else if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_PEER_DOWN) {
                /*
                 * CASE: TERM msg
                 * close tcp socket and stop working
                 */
                LOG_INFO("received peer down message from router: %s for peer: %s",router_ip.c_str(), peer_ip.c_str());
            } else if (parsed_bmp_msg->type == PARSEBGP_BMP_TYPE_STATS_REPORT) {
                /*
                 * CASE: TERM msg
                 * close tcp socket and stop working
                 */
                LOG_INFO("received stats report from router: %s",router_ip.c_str());
            }
            //It's not sufficient ro only refill the buffer if a PARTIAL_MSG is found. A valid message at the the end 
            //of the buffer can lead to an overflow without triggering a PARTIAL_MSG. When the buffer gets
            //below 64 bytes, refill
            if(bmp_data_unread_len <= 64){
                LOG_DEBUG("low unread buffer: %d refilling", bmp_data_unread_len);
                refill_buffer(WORKER_BUF_REFILL_SIZE);
            } 
            // update buffer pointers
            update_buffer(raw_bmp_msg_len);
        } else if (err == PARSEBGP_PARTIAL_MSG) {
            // recv data from socket 1 byte at a time until a bmp init msg is received.
            // this allows us to receive the entire init msg faster (as soon as the router is connected).
            /* TODO: may be a bug here? recv() will continue to block until, e.g. all 1500 bytes are received.
             *          which means if the ringbuffer cannot fulfill the last 1500 bytes before it terminates,
             *          the worker stucks at recv();  unless recv() returns a smaller amount of bytes
             *          when it detects the connection has been closed.*/
            refill_buffer(router_init ? WORKER_BUF_REFILL_SIZE : 1);
//            refill_buffer(WORKER_BUF_REFILL_SIZE);
        } else if (err == PARSEBGP_INVALID_MSG) {
            // Handle invalid messages without stopping the worker.
            LOG_ERR("Invalid BMP messages of type: %d. Skipping");
            int raw_bmp_msg_len = parser.get_raw_bmp_msg_len();
            update_buffer(raw_bmp_msg_len);
        } else {
            LOG_INFO("stopping the worker, something serious happened -- %d", err);
            // set worker status to stopped so the main thread can clean up.
            status = WORKER_STATUS_STOPPED;
            break;
        }
    }

    // close dump file
//    fclose(fp);

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
            LOG_INFO("bad connection from router: %d, stopping worker", router_ip.c_str());
            // set worker status to stopped. the main thread will clean up later.
            status = WORKER_STATUS_STOPPED;
        }
        bmp_data_unread_len += received_bytes;
    } else {
        LOG_ERR("Buffer overflow in worker! router: %d", router_ip.c_str());
        status = WORKER_STATUS_STOPPED;
    }

}

void Worker::update_buffer(int parsed_bmp_msg_len) {
    bmp_data_read_len += parsed_bmp_msg_len;
    bmp_data_unread_len -= parsed_bmp_msg_len;
    if(bmp_data_read_len < 0){
        LOG_ERR("Assertion error. bmp_data_read_len is invalid (%d)", bmp_data_read_len);
    }
    assert(bmp_data_unread_len >= 0);
    if(bmp_data_unread_len < 0){
        LOG_ERR("Assertion error. bmp_data_unread_len is invalid (%d)", bmp_data_read_len);
    }
    assert(bmp_data_unread_len >= 0);
}

int Worker::get_bmp_data_unread_len() {
    return bmp_data_unread_len;
}

uint8_t *Worker::get_unread_buffer() {
    return bmp_data_buffer + bmp_data_read_len;
}