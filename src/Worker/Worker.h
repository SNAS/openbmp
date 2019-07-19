#ifndef OPENBMP_WORKER_H
#define OPENBMP_WORKER_H

#endif //OPENBMP_WORKER_H

#include "../OpenBMP.h"
#include "Encapsulator.h"

class Worker {
public:
    Worker(OpenBMP obmp, int tcp_fd);
    void start();
    void stop();
    double rib_dump_rate();
private:
    OpenBMP obmp;
    Encapsulator encapsulator;
    int tcp_fd;
    int reader_fd;
    int writer_fd;
};