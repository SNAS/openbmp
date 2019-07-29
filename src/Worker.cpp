#include <iostream>
#include <sys/socket.h>
#include "Worker.h"

Worker::Worker() {
    running = false;

    // Buffer client socket using pipe
    socketpair(PF_LOCAL, SOCK_STREAM, 0, sock_fds);
    reader_fd = sock_fds[0];
    writer_fd = sock_fds[1];
}

double Worker::rib_dump_rate() {}

void Worker::start() {
    running = true;
    while (running) {
        // create data_store thread

    }
}

// set running flag to false
void Worker::stop() {
    running = false;
}

// return if the worker is running
bool Worker::is_running() {
    return running;
}

