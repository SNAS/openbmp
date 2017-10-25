/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef CLIENT_THREAD_H_
#define CLIENT_THREAD_H_

#include "MsgBusImpl_kafka.h"
#include "BMPListener.h"
#include "Logger.h"
#include "Config.h"
#include <thread>

#define CLIENT_WRITE_BUFFER_BLOCK_SIZE    8192        // Number of bytes to write to BMP reader from buffer

struct ThreadMgmt {
    pthread_t thr;
    BMPListener::ClientInfo client;
    Config *cfg;
    Logger *log;
    bool running;                       // true if running, zero if not running
    bool baselineTimeout;		        // true if past the baseline time of the router
};

struct ClientThreadInfo {
    msgBus_kafka *mbus;
    BMPListener::ClientInfo *client;
    Logger *log;

    std::thread *bmp_reader_thread;
    int bmp_write_end_sock;

    bool closing;                      // Indicates if client is closing normally (set when socket is disconnected)

};

/**
 * Client thread function
 *
 * Thread function that is called when starting a new thread.
 * The DB/mysql is initialized for each thread.
 *
 * @param [in]  arg     Pointer to the BMPServer ClientInfo
 */
void *ClientThread(void *arg);



#endif /* CLIENT_THREAD_H_ */
