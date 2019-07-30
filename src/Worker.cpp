#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <netinet/in.h>
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

    // dummy value init for bmp router connection type
    is_ipv4_connection = true;

    // creates a pipe sock (PF_LOCAL) between the worker and datastore
    socketpair(PF_LOCAL, SOCK_STREAM, 0, worker_to_data_store_sock_pair_fd);

    // worker will read from reader_fd
    reader_fd = worker_to_data_store_sock_pair_fd[0];

    // datastore will push data to writer_fd
    writer_fd = worker_to_data_store_sock_pair_fd[1];
}

double Worker::rib_dump_rate() {
    std::cout << "rib dump rate is not implemented yet." << std::endl;
}

void Worker::start(int active_tcp_socket, bool is_ipv4_socket) {
    // save the type of tcp socket
    this->is_ipv4_connection = is_ipv4_socket;
    // establish tcp connection with bmp router
    establish_connection_with_bmp_router(active_tcp_socket);

    status = WORKER_STATUS_RUNNING;
    // create data_store thread

    // worker now consumes pipe socket to parse bmp msgs
    while (status == WORKER_STATUS_RUNNING) {
        // TODO
        sleep(1);
    }
}

// set running flag to false
void Worker::stop() {
    status = WORKER_STATUS_STOPPED;
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

void Worker::establish_connection_with_bmp_router(int active_tcp_socket) {
    // accept the pending client request, or block till one exists
    socklen_t bmp_router_addr_len = sizeof(bmp_router_addr);    // the bmp info length

    if ((bmp_router_tcp_fd = accept(active_tcp_socket,
                                    (struct sockaddr *) &bmp_router_addr, &bmp_router_addr_len)) < 0) {

        std::string error = "Server accept connection: ";
        if (errno != EINTR)
            error += strerror(errno);
        else
            error += "Exiting normally per user request to stop server";

        throw error.c_str();
    }


    if (debug) {
        char c_ip[46];
        char c_port[6];
        if (is_ipv4_connection){
            sockaddr_in *bmp_router_addr_v4 = (sockaddr_in *) &bmp_router_addr;
            inet_ntop(AF_INET,  &bmp_router_addr_v4->sin_addr, c_ip, sizeof(c_ip));
            snprintf(c_port, sizeof(c_port), "%hu", ntohs(bmp_router_addr_v4->sin_port));
        } else {
            sockaddr_in6 *bmp_router_addr_v6 = (sockaddr_in6 *) &bmp_router_addr;
            inet_ntop(AF_INET6,  &bmp_router_addr_v6->sin6_addr, c_ip, sizeof(c_ip));
            snprintf(c_port, sizeof(c_port), "%hu", ntohs(bmp_router_addr_v6->sin6_port));
        }
        cout << "bmp router ip: "<< c_ip << " port: "<< c_port << endl;
    }

    // enable TCP keepalive
    int on = 1;
    if (setsockopt(bmp_router_tcp_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
        LOG_NOTICE("%s: sock=%d: Unable to enable tcp keepalive");
    }

}

