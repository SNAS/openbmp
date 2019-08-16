#ifndef OPENBMP_OPENBMP_H
#define OPENBMP_OPENBMP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Config.h"
#include "Worker.h"
#include "Logger.h"
#include "MessageBus.h"

#include <vector>

class Worker; // forward declaration to void the cross referencing issue between OpenBMP.h and Worker.h

class OpenBMP {
public:
    OpenBMP();

    Config *config;
    Logger *logger;
    MessageBus *message_bus;
    std::vector<Worker*> workers;

    /**********************************
     *  public functions of openbmp
     **********************************/
    void start();  // start the server

    void stop(); // stop openbmp server

private:
    bool running; // collector running status.
    bool debug; // whether the openbmp main thread should print debug messages.

    // a cpu utilization monitor thread will update this value
    double cpu_util = 0;
    thread cpu_mon_thread;

    /*******************************************************
     *  openbmp server connection-related variables
     *******************************************************/
    int sock;  // collector listening socket (v4)
    int sock_v6;  // collector listening socket (v6)
    sockaddr_in  collector_addr{};  // collector v4 address
    sockaddr_in6 collector_addr_v6{};  // collector v6 address

    /****************************************************
     * Functions to accept new bmp connections
    *****************************************************/
    // create tcp socket so the collector can accept connection from bmp routers.
    void open_server_socket(bool ipv4, bool ipv6);

    // checks for any bmp connection, if it finds one, it hands over the connection to a worker.
    void find_bmp_connection(Worker *worker);

    // checks if the collector can still handle more bmp connections.
    bool can_accept_bmp_connection();

    // a thread that updates cpu usage
    void cpu_usage_monitor();

    // used by can_accept_bmp_connection()
    bool did_not_affect_rib_dump_rate();

    // remove workers that has stopped working.
    void remove_dead_workers();

    // return the number of workers that are waiting for their RIB dumps
    int get_rib_dump_waiting_worker_num();

};


#endif //OPENBMP_OPENBMP_H
