#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#include "Encapsulator.h"

class OpenBMP;  // forward declaration

class Worker {
public:
    Worker();
    void start();
    void stop();
    bool is_running();
    double rib_dump_rate();
private:
    Encapsulator encapsulator;

    bool running;

    int tcp_fd;

    int sock_fds[2];
    int reader_fd;
    int writer_fd;
};

#endif //OPENBMP_WORKER_H

