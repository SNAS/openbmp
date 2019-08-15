#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#include <thread>
#include <sys/socket.h>
#include "Encapsulator.h"
#include "Logger.h"
#include "Config.h"
#include "Constant.h"
#include "TopicBuilder.h"
#include "SockBuffer.h"
#include "MessageBus.h"
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

    // return if rib dump phase has started.
    bool has_rib_dump_started();

private:
    /*************************************
     * Worker's dependencies
     *************************************/
    Config *config;
    Logger *logger;
    Encapsulator *encapsulator;
    TopicBuilder *topic_builder;
    MessageBus *msg_bus;
    SockBuffer sock_buffer;
    Parser parser;  // libparsebgp wrapper

    // debug flag
    bool debug;
    // worker status: WORKER_STATUS_WAITING | WORKER_STATUS_RUNNING | WORKER_STATUS_STOPPED
    int status;
    // whether the worker has received a init msg
    bool router_init = false;
    // set to true when the worker receives none init msgs after the init msg.
    bool router_rib_dump_started = false;

    // worker will read bmp data from reader_fd,
    //  and the bmp data is sent by SockBuffer
    //  as it receives the bmp data from its connected bmp router
    int reader_fd;

    // work() thread
    //  this thread process bmp data from sockbuffer and send bmp msgs to message bus
    thread work_thread;

    // variables to save raw bmp data
    uint8_t bmp_data_buffer[BMP_MSG_BUF_SIZE];
    int bmp_data_unread_len = 0;
    int bmp_data_read_len = 0;

    // router related information
    bool is_router_ip_ipv4 = true;
    string router_ip;
    uint8_t router_ip_raw[16];
    string router_hostname;
    string router_group;

    /**********************************
     * Worker's helper functions
     **********************************/
    // processes bmp data and send encapsulated raw bmp msgs to the msg bus
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

