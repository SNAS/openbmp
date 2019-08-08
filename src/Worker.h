#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H


#include <thread>
#include <sys/socket.h>
#include "Encapsulator.h"
#include "Logger.h"
#include "Config.h"
#include "Constants.h"
#include "TopicBuilder.h"
#include "SockBuffer.h"
#include "Parser.h"

using namespace std;

class OpenBMP;  // forward declaration

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
    Parser parser;  // libparsebgp wrapper
    Config *config;
    Logger *logger;

    // debug flag
    bool debug;
    // worker status: WORKER_STATUS_WAITING | WORKER_STATUS_RUNNING | WORKER_STATUS_STOPPED
    int status;
    bool router_init = false;

    // worker will read from reader_fd.
    int reader_fd;

    // Work thread
    thread work_thread;

    // variables to save raw bmp data
    uint8_t bmp_data_buffer[BMP_MSG_BUF_SIZE];
    int bmp_data_unread_len = 0;
    int bmp_data_read_len = 0;

    // router related information
    string router_ip;

    /**********************************
     * Worker's helper functions
     **********************************/
    // to process bmp messages
    void work();
    // bmp_data_buffer related functions
    uint8_t *get_unread_buffer();

    // get the length of unread bytes in the bmp data buffer
    int get_bmp_data_unread_len();

    // update buffer values
    void update_buffer(int parsed_bmp_msg_len);

    // save more data from sockbuffer
    void refill_buffer(int recv_len);

};

#endif //OPENBMP_WORKER_H

