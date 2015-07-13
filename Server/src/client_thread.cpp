/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <sys/socket.h>

#include <cstdlib>
#include <cstring>
#include <thread>

#include "client_thread.h"
#include "BMPReader.h"
#include "Logger.h"


#include <cxxabi.h>
#include <poll.h>


/**
 * Client thread cancel
 * @param arg       Pointer to ClientThreadInfo struct
 */
void ClientThread_cancel(void *arg) {
    ClientThreadInfo *cInfo = static_cast<ClientThreadInfo *>(arg);
    Logger *logger = cInfo->log;

    LOG_INFO("Thread terminating due to cancel request.");

    LOG_INFO("Closing client connection to %s:%s", cInfo->client->c_ipv4, cInfo->client->c_port);
    shutdown(cInfo->client->c_sock, SHUT_RDWR);
    close (cInfo->client->c_sock);

    close (cInfo->client->pipe_sock);
    close (cInfo->bmp_write_end_sock);
    cInfo->bmp_reader_thread->join();

    delete cInfo->bmp_reader_thread;

    delete cInfo->mysql;
    cInfo->mysql = NULL;
}

/**
 * Client thread function
 *
 * Thread function that is called when starting a new thread.
 * The DB/mysql is initialized for each thread.
 *
 * @param [in]  arg     Pointer to the BMPServer ClientInfo
 */
void *ClientThread(void *arg) {
    // Setup the args
    ThreadMgmt *thr = static_cast<ThreadMgmt *>(arg);
    Logger *logger = thr->log;

    // Setup the client thread info struct
    ClientThreadInfo cInfo;
    cInfo.mysql = NULL;
    cInfo.client = &thr->client;
    cInfo.log = thr->log;

    int sock_fds[2];
    pollfd pfd;
    unsigned char *sock_buf = NULL;

    /*
     * Setup the cleanup routine for when the thread is canceled.
     *  A thread is only canceled if openbmpd is terminated.
     */
    pthread_cleanup_push(ClientThread_cancel, &cInfo);

    try {
        // connect to mysql
        cInfo.mysql = new mysqlBMP(logger, thr->cfg->dbURL,thr->cfg->username, thr->cfg->password, thr->cfg->dbName);

        if (thr->cfg->debug_mysql)
            cInfo.mysql->enableDebug();

        BMPReader rBMP(logger, thr->cfg);
        LOG_INFO("Thread started to monitor BMP from router %s using socket %d buffer in bytes = %u",
                cInfo.client->c_ipv4, cInfo.client->c_sock, thr->cfg->bmp_buffer_size);

        // Buffer client socket using pipe
        socketpair(PF_LOCAL, SOCK_STREAM, 0, sock_fds);
        cInfo.bmp_write_end_sock = sock_fds[1];
        cInfo.client->pipe_sock = sock_fds[0];

        /*
         * Create and start the reader thread to monitor the pipe fd (read end)
         */
        bool bmp_run = true;
        //cInfo.bmp_reader_thread = new std::thread([&] {rBMP.readerThreadLoop(bmp_run,cInfo.client,
        cInfo.bmp_reader_thread = new std::thread(&BMPReader::readerThreadLoop, &rBMP, std::ref(bmp_run), cInfo.client,
                                                                             (DbInterface *)cInfo.mysql );

        // Variables to handle circular buffer
        sock_buf = new unsigned char[thr->cfg->bmp_buffer_size];
        int bytes_read = 0;
        int write_buf_pos = 0;
        int read_buf_pos = 0;
        int bytes_in_buf = 0;
        bool wrap_state = false;
        unsigned char *sock_buf_read_ptr = sock_buf;
        unsigned char *sock_buf_write_ptr = sock_buf;

        /*
         * monitor and buffer the client socket
         */
        while (bmp_run) {

            if ((wrap_state and (write_buf_pos + 1) < read_buf_pos) or
                    (not wrap_state and write_buf_pos < thr->cfg->bmp_buffer_size)) {

                pfd.fd = cInfo.client->c_sock;
                pfd.events = POLLIN | POLLHUP | POLLERR;
                pfd.revents = 0;

                // Attempt to read from socket
                if (poll(&pfd, 1, 5)) {
                    if (pfd.revents & POLLHUP or pfd.revents & POLLERR) {
                        bytes_read = 0;                     // Indicate to close the connection

                    } else {
                            if (not wrap_state)     // write is ahead of read in terms of buffer pointer
                                bytes_read = read(cInfo.client->c_sock, sock_buf_write_ptr,
                                                  thr->cfg->bmp_buffer_size - write_buf_pos);

                            else if (read_buf_pos > write_buf_pos) // read is ahead of write in terms of buffer pointer
                                bytes_read = read(cInfo.client->c_sock, sock_buf_write_ptr,
                                                  read_buf_pos - write_buf_pos - 1);
                    }

                    if (bytes_read <= 0) {
                        close(sock_fds[0]);
                        close(sock_fds[1]);
                        close(cInfo.client->c_sock);

                        bmp_run = false;
                        cInfo.bmp_reader_thread->join();
                        delete cInfo.bmp_reader_thread;
                        break;
                    }
                    else {
                        sock_buf_write_ptr += bytes_read;
                        write_buf_pos += bytes_read;
                    }

                }

            } else if (write_buf_pos >= thr->cfg->bmp_buffer_size) { // if reached end of buffer space
                // Reached end of buffer, wrap to start
                write_buf_pos = 0;
                sock_buf_write_ptr = sock_buf;
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

            if ((not wrap_state and read_buf_pos < write_buf_pos) or
                    (wrap_state and read_buf_pos < thr->cfg->bmp_buffer_size)) {

                pfd.fd = cInfo.bmp_write_end_sock;
                pfd.events = POLLOUT | POLLHUP | POLLERR;
                pfd.revents = 0;

                // Attempt to write buffer to bmp reader
                if (poll(&pfd, 1, 10)) {

                    if (pfd.revents & POLLHUP or pfd.revents & POLLERR) {
                        close(sock_fds[0]);
                        close(sock_fds[1]);
                        close(cInfo.client->c_sock);

                        bmp_run = false;
                        cInfo.bmp_reader_thread->join();
                        delete cInfo.bmp_reader_thread;
                        break;
                    }

                    if (not wrap_state) // Write buffer is a head of read in terms of buffer pointer
                        bytes_read = write(cInfo.bmp_write_end_sock, sock_buf_read_ptr,
                                           (write_buf_pos - read_buf_pos) > CLIENT_WRITE_BUFFER_BLOCK_SIZE ?
                                           CLIENT_WRITE_BUFFER_BLOCK_SIZE : (write_buf_pos - read_buf_pos));

                    else // Read buffer is ahead of write in terms of buffer pointer
                        bytes_read = write(cInfo.bmp_write_end_sock, sock_buf_read_ptr,
                                           (thr->cfg->bmp_buffer_size - read_buf_pos) > CLIENT_WRITE_BUFFER_BLOCK_SIZE ?
                                           CLIENT_WRITE_BUFFER_BLOCK_SIZE : (thr->cfg->bmp_buffer_size - read_buf_pos));

                    if (bytes_read > 0) {
                        sock_buf_read_ptr += bytes_read;
                        read_buf_pos += bytes_read;
                    }
                }
            }
            else if (read_buf_pos >= thr->cfg->bmp_buffer_size) {
                read_buf_pos = 0;
                sock_buf_read_ptr = sock_buf;
                wrap_state = false;
                //LOG_INFO("read buffer wrapped");
            }
        }

        LOG_INFO("%s: Thread for sock [%d] ended normally", cInfo.client->c_ipv4, cInfo.client->c_sock);

    } catch (char const *str) {
        LOG_INFO("%s: %s - Thread for sock [%d] ended", cInfo.client->c_ipv4, str, cInfo.client->c_sock);
        close(sock_fds[0]);
        close(sock_fds[1]);

    } catch (abi::__forced_unwind&) {
        close(sock_fds[0]);
        close(sock_fds[1]);
        throw;

    } catch (...) {
        LOG_INFO("%s: Thread for sock [%d] ended abnormally: ", cInfo.client->c_ipv4, cInfo.client->c_sock);
        close(sock_fds[0]);
        close(sock_fds[1]);


    }

    if (sock_buf != NULL)
        delete [] sock_buf;

    pthread_cleanup_pop(0);

    // Delete mysql
    if (cInfo.mysql != NULL)
       delete cInfo.mysql;

    // close the socket
    shutdown(cInfo.client->c_sock, SHUT_RDWR);
    close (cInfo.client->c_sock);

    // Indicate that we are no longer running
    thr->running = false;

    // Exit the thread
    pthread_exit(NULL);

    return NULL;
}
