//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_SOCKBUFFER_H
#define OPENBMP_SOCKBUFFER_H

#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "Logger.h"
#include "Config.h"

/*
 * SockBuffer saves data from an TCP socket,
 * and pushes to a different socket.
 */
class SockBuffer {
public:
    // constructor
    SockBuffer();

    // runs sockbuffer and handles thread creation.
    void start(int obmp_server_sock, bool is_ipv4_connection);
    // stop buffering, close the connection to bmp router.
    void stop();

private:
    // debug mode
    bool debug;

    Config *config;
    Logger *logger;

    /*
     * TCP socket connects to a BMP router
     * SockBuffer pushes bmp data to one pipe socket (write_fd)
     * Worker's work() reads bmp data from the other pipe socket (read_fd)
     */

    // tcp socket that connects to the bmp router of this worker.
    // router tcp fd; sock buffer reads bmp data from this file descriptor
    int router_tcp_fd;
    // pollfd that checks tcp socket that connects to the bmp router of a worker
    pollfd pfd_tcp;
    // bmp router address info
    sockaddr_storage bmp_router_addr;

    // creates a pipe sock (PF_LOCAL) between the worker and datastore.
    int worker_to_data_store_sock_pair_fd[2];
    // worker will read from reader_fd.
    int reader_fd;
    // datastore will push data to writer_fd.
    int writer_fd;
    // pollfd that checks pipe socket that writes tcp data back to worker
    pollfd pfd_pipe;

    // ring buffer related variables
    unsigned char* ring_buffer;
    int ring_buffer_size;
    int bytes_read = 0;
    int read_position = 0;
    int write_position = 0;
    bool wrap_state = false;

    unsigned char *sock_buf_read_ptr;
    unsigned char *sock_buf_write_ptr;

    // function to establish connection with a bmp router
    void establish_router_connection(int obmp_server_sock, bool is_ipv4_connection);

    // start to buffer bmp msgs
    // the thread will call save_data() and push_data() to buffer bmp data
    void create_sock_buffer_thread();

    // functions to read and push bmp data
    void save_data();
    void push_data();

};


#endif //OPENBMP_RINGBUFFER_H
