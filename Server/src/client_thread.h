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

#include "DbImpl_mysql.h"
#include "BMPListener.h"
#include "Logger.h"
#include "Config.h"
#include <thread>

struct ThreadMgmt {
    pthread_t thr;
    BMPListener::ClientInfo client;
    Cfg_Options *cfg;
    Logger *log;
    bool running;                       // true if running, zero if not running
};

struct ClientThreadInfo {
    mysqlBMP *mysql;
    BMPListener::ClientInfo *client;
    Logger *log;

    std::thread *bmp_reader_thread;
    int bmp_write_end_sock;

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
