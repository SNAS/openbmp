#include <iostream>
#include "Worker.h"

Worker::Worker(OpenBMP* obmp) {
    obmp_main = obmp;
}

double Worker::rib_dump_rate() {}
void Worker::start() {}
void Worker::stop() {}

