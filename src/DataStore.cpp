//
// Created by Lumin Shi on 2019-07-29.
//

#include "DataStore.h"
#include "Config.h"

#define CLIENT_WRITE_BUFFER_BLOCK_SIZE    8192        // Number of bytes to write to BMP reader from buffer

DataStore::DataStore(int in_sock, int out_sock) {
    auto config = Config::get_config();
    ring_buffer_size = config->bmp_ring_buffer_size;
    ring_buffer = new unsigned char[ring_buffer_size];

    // init tcp pfd
    tcp_fd = in_sock;
    pfd_tcp.fd = tcp_fd;
    pfd_tcp.events = POLLIN | POLLHUP | POLLERR;
    pfd_tcp.revents = 0;

    // init pipe pfd
    pipe_fd = out_sock;
    pfd_pipe.fd = pipe_fd;
    pfd_pipe.events = POLLIN | POLLHUP | POLLERR;
    pfd_pipe.revents = 0;

}

void DataStore::save_data() {
    if ((wrap_state and (write_position + 1) < read_position) or
        (not wrap_state and write_position < ring_buffer_size)) {

        // Attempt to read from socket
        if (poll(&pfd_tcp, 1, 5)) {
            if (pfd_tcp.revents & POLLHUP or pfd_tcp.revents & POLLERR) {
                bytes_read = 0;                     // Indicate to close the connection

            } else {
                if (not wrap_state)     // write is ahead of read in terms of buffer pointer
                    bytes_read = read(tcp_fd, sock_buf_write_ptr,
                                      ring_buffer_size - write_position);

                else if (read_position > write_position) // read is ahead of write in terms of buffer pointer
                    bytes_read = read(tcp_fd, sock_buf_write_ptr,
                                      read_position - write_position - 1);
            }

            if (bytes_read <= 0) {
                throw "bad tcp connection.";
//                close(sock_fds[0]);
//                close(sock_fds[1]);
//                close(cInfo.client->c_sock);
            }
            else {
                sock_buf_write_ptr += bytes_read;
                write_position += bytes_read;
            }

        }

    } else if (write_position >= ring_buffer_size) { // if reached end of buffer space
        // Reached end of buffer, wrap to start
        write_position = 0;
        sock_buf_write_ptr = ring_buffer;
        wrap_state = true;
        //LOG_INFO("write buffer wrapped");
    }

    /** DEBUG ONLY

    else {
        LOG_INFO("%s: buffer stall, waiting for read to catch up  w=%u r=%u",  cInfo.client->c_ipv4,
                 write_buf_pos, read_buf_pos);
    }

    if (write_buf_pos != read_buf_pos)
        LOG_INFO("%s: CHECK: state=%d w=%u r=%u",  cInfo.client->c_ipv4, wrap_state, write_buf_pos, read_buf_pos);
    **/
}

void DataStore::push_data() {
    if ((not wrap_state and read_position < write_position) or
        (wrap_state and read_position < ring_buffer_size)) {

        // Attempt to write buffer to bmp reader
        if (poll(&pfd_pipe, 1, 10)) {

            if (pfd_pipe.revents & POLLHUP or pfd_pipe.revents & POLLERR) {
                throw "bad pipe connection.";
            }

            if (not wrap_state) // Write buffer is a head of read in terms of buffer pointer
                bytes_read = write(pipe_fd, sock_buf_read_ptr,
                                   (write_position - read_position) > CLIENT_WRITE_BUFFER_BLOCK_SIZE ?
                                   CLIENT_WRITE_BUFFER_BLOCK_SIZE : (write_position - read_position));

            else // Read buffer is ahead of write in terms of buffer pointer
                bytes_read = write(pipe_fd, sock_buf_read_ptr,
                                   (ring_buffer_size - read_position) > CLIENT_WRITE_BUFFER_BLOCK_SIZE ?
                                   CLIENT_WRITE_BUFFER_BLOCK_SIZE : (ring_buffer_size - read_position));

            if (bytes_read > 0) {
                sock_buf_read_ptr += bytes_read;
                read_position += bytes_read;
            }
        }
    }
    else if (read_position >= ring_buffer_size) {
        read_position = 0;
        sock_buf_read_ptr = ring_buffer;
        wrap_state = false;
        //LOG_INFO("read buffer wrapped");
    }

}
