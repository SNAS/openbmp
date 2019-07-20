#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

class OpenBMP;  // forward declaration
#include "Encapsulator.h"
#include "OpenBMP.h"

class Worker {
public:
    Worker(OpenBMP* obmp);
    void start();
    void stop();
    double rib_dump_rate();
private:
    OpenBMP* obmp;
    Encapsulator encapsulator;
    int tcp_fd;
    int reader_fd;
    int writer_fd;
};

#endif //OPENBMP_WORKER_H

