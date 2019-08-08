//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_SOCKBUFFER_H
#define OPENBMP_SOCKBUFFER_H

#include <poll.h>
#include <thread>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fstream>
#include <iostream>

#include "Logger.h"
#include "Config.h"

using namespace std;

/*
 * SockBuffer saves data from an TCP socket,
 * and pushes to a different socket.
 */
class SockBuffer {
public:
    // constructor
    SockBuffer();

    // to connect with bmp router and handle bufferer thread creation
    void start(int obmp_server_sock, bool is_ipv4_connection);
    // stop buffering, close the connection to bmp router.
    void stop();

    // worker calls this function to get reader_fd
    int get_reader_fd();
    string get_router_ip();

private:
    // debug mode
    bool debug = false;
    // running status
    bool running = false;

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
    int local_sock_pair[2];
    // worker will read from reader_fd.
    int reader_fd;
    // sockbuffer will push data to writer_fd.
    int writer_fd;
    // pollfd that checks local socket that writes tcp data back to worker
    pollfd pfd_local;

    // ring buffer related variables
    unsigned char* ring_buffer;
    int ring_buffer_size;
    int bytes_read = 0;
    int read_position = 0;
    int write_position = 0;
    bool wrap_state = false;
    unsigned char *sock_buf_read_ptr;
    unsigned char *sock_buf_write_ptr;

    // thread-related variables
    thread buffer_thread;

    // plain text router info
    string router_ip;
    string router_port;

    // function to establish connection with a bmp router
    void connect_bmp_router(int obmp_server_sock, bool is_ipv4_connection);

    // start to buffer bmp msgs
    // the thread will call save_data() and push_data() to store and push bmp data
    void sock_bufferer();

    // functions to read and push bmp data
    void save_data(); // reads from the sock connected with router
    void push_data(); // pushes to the sock connected with worker

};


#endif //OPENBMP_RINGBUFFER_H
