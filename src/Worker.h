#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#include "Encapsulator.h"
#include "Logger.h"
#include "Config.h"

class OpenBMP;  // forward declaration

#define WORKER_STATUS_WAITING 1
#define WORKER_STATUS_RUNNING 2
#define WORKER_STATUS_STOPPED 3

class Worker {
public:
    Worker();
    // worker start function starts with a socket with pending bmp request
    void start(int active_tcp_socket, bool is_ipv4_socket);
    // stop function set worker status to STOPPED
    void stop();
    // return if worker is processing bmp data
    bool is_running();
    // return if worker is waiting for bmp router
    bool is_waiting();
    // return if worker has been stopped for whatever reason
    bool has_stopped();
    double rib_dump_rate();
private:
    Encapsulator encapsulator;
    Config *config;
    Logger *logger;

    bool debug; // debug flag

    // worker status: WORKER_STATUS_WAITING | WORKER_STATUS_RUNNING | WORKER_STATUS_STOPPED
    int status;

    sockaddr_storage bmp_router_addr; // bmp router address info

    // creates a pipe sock (PF_LOCAL) between the worker and datastore.
    int worker_to_data_store_sock_pair_fd[2];
    // worker will read from reader_fd.
    int reader_fd;
    // datastore will push data to writer_fd.
    int writer_fd;

    // tcp socket that connects to the bmp router of this worker.
    // datastore reads messages from this socket.
    int bmp_router_tcp_fd;
    // remember if this socket is ip v4 or v6 socket.
    bool is_ipv4_connection;

    // called by Worker::start(), it establish connection requested by a bmp router
    // (hopefully is requested by a bmp router)
    void establish_connection_with_bmp_router(int active_tcp_socket);
};

#endif //OPENBMP_WORKER_H

