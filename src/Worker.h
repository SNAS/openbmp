#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#include "Encapsulator.h"

class OpenBMP;  // forward declaration

class Worker {
public:
    Worker(OpenBMP* obmp);
    void start();
    void stop();
    double rib_dump_rate();
private:
    OpenBMP* obmp_main;
    Encapsulator encapsulator;
    int tcp_fd;
    int reader_fd;
    int writer_fd;
};

#endif //OPENBMP_WORKER_H

