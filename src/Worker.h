#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#include "Encapsulator.h"
#include "Logger.h"
#include "Config.h"
#include "TopicBuilder.h"
#include "SockBuffer.h"

class OpenBMP;  // forward declaration

#define WORKER_STATUS_WAITING 1
#define WORKER_STATUS_RUNNING 2
#define WORKER_STATUS_STOPPED 3
#define BMP_MSG_BUF_SIZE 68000

class Worker {
public:
    Worker();
    // worker start function starts with a socket with pending bmp request
    void start(int obmp_server_tcp_socket, bool is_ipv4_socket);
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
    /*************************************
     * Worker's dependencies
     *************************************/
    Encapsulator encapsulator;
    TopicBuilder topic_builder;
    SockBuffer sock_buffer;
    Config *config;
    Logger *logger;
    // libparsebgp bmp_parser;  // TODO: we need to add the to-be-updated libparsebgp here.

    // debug flag
    bool debug;
    // worker status: WORKER_STATUS_WAITING | WORKER_STATUS_RUNNING | WORKER_STATUS_STOPPED
    int status;

    // Worker's read fd
    //  sockbuffer saves bmp router's data to its ringbuffer,
    //  it then pushes bmp data via a pipe socket.
    //  this is the other end of the socket.
    int read_fd;

    /**********************************
     * Worker's helper functions
     **********************************/
    // to process bmp messages
    void work();
};

#endif //OPENBMP_WORKER_H

