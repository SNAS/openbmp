#include <iostream>
#include <unistd.h>
#include <cstring>
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
    // start SockBuffer, set read_fd
    sock_buffer.start(obmp_server_tcp_socket, is_ipv4_socket);

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

    // stop sock_buffer, it will also disconnect with the connected router.
    sock_buffer.stop();

    // join worker
    work_thread.join();
    if (debug) LOG_INFO("a worker stopped.");
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
    // variables to save a raw bmp message
    uint8_t bmp_msg_buffer[BMP_MSG_BUF_SIZE];
    int bmp_msg_len;

    while (status == WORKER_STATUS_RUNNING) {
        // TODO: grab enough byte to feed libparsebgp, it returns number of bytes it still needs.

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


        // TODO
        sleep(1);
    }
}
