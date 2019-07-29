//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_DATASTORE_H
#define OPENBMP_DATASTORE_H

#include <poll.h>

/*
 * DataStore saves data from an TCP socket,
 * and pushes to a different socket.
 */
class DataStore {
public:
    DataStore(int in_sock, int out_sock);

    void save_data();

    void push_data();

private:
    // a blocking ring buffer
    unsigned char* ring_buffer;
    int ring_buffer_size;

    int tcp_fd;   // tcp connection
    int pipe_fd;  // write-end socket that pushes data to worker
    pollfd pfd_tcp; // checks tcp socket that connects to the bmp router of a worker
    pollfd pfd_pipe; // checks pipe socket that writes tcp data back to worker

    int bytes_read = 0;
    int read_position = 0;
    int write_position = 0;
    bool wrap_state = false;

    unsigned char *sock_buf_read_ptr;
    unsigned char *sock_buf_write_ptr;

};


#endif //OPENBMP_RINGBUFFER_H
