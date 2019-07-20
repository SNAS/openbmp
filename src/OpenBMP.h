#ifndef OPENBMP_OPENBMP_H
#define OPENBMP_OPENBMP_H

class Worker; // forward declaration
#include "Config.h"
#include "Worker.h"
#include "Logger.h"
#include "MessageBus.h"

#include <list>

using namespace std;

class OpenBMP {
public:
    OpenBMP();
    Config config;
    MessageBus message_bus;
    list<Worker> workers;

    void start();
    void stop();
    int get_num_of_active_connections();

private:
    /****************************************************/
    /* Functions to accept new bmp connection() */
    /****************************************************/
    void accept_bmp_connection();
    void can_accept_bmp_connection();
    bool below_max_cpu_utilization_threshold();
    bool did_not_affect_rib_dump_rate();
    void create_worker(OpenBMP* obmp);
    void remove_dead_workers();

};


#endif //OPENBMP_OPENBMP_H
