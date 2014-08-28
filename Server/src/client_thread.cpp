/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <sys/socket.h>

#include "client_thread.h"
#include "BMPReader.h"
#include "Logger.h"

/**
 * Client thread cancel
 * @param arg       Pointer to ClientThreadInfo struct
 */
void ClientThread_cancel(void *arg) {
    ClientThreadInfo *cInfo = static_cast<ClientThreadInfo *>(arg);
    Logger *log = cInfo->log;

    LOG_INFO("Thread terminating due to cancel request.");

    LOG_INFO("Closing client connection to %s:%s", cInfo->client->c_ipv4, cInfo->client->c_port);
    shutdown(cInfo->client->c_sock, SHUT_RDWR);
    close (cInfo->client->c_sock);

    delete cInfo->mysql;
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
    pthread_t myid = pthread_self();

    // Setup the args
    ThreadMgmt *thr = static_cast<ThreadMgmt *>(arg);
    Logger *log = thr->log;

    // Setup the client thread info struct
    ClientThreadInfo cInfo;
    cInfo.client = &thr->client;
    cInfo.log = thr->log;

    // connect to mysql
    cInfo.mysql = new mysqlBMP(log, thr->cfg->dbURL,thr->cfg->username, thr->cfg->password, thr->cfg->dbName);

    /*
     * Setup the cleanup routine for when the thread is canceled.
     *  A thread is only canceled if openbmpd is terminated.
     */
    pthread_cleanup_push(ClientThread_cancel, &cInfo);

    if (thr->cfg->debug_mysql)
        cInfo.mysql->enableDebug();

    try {
        BMPReader rBMP(log, thr->cfg);
        LOG_INFO("Thread started to monitor BMP from router %s using socket %d",
                cInfo.client->c_ipv4, cInfo.client->c_sock);

        while (1) {
            rBMP.ReadIncomingMsg(cInfo.client, (DbInterface *)cInfo.mysql);
        }

    } catch (char const *str) {
        LOG_INFO("%s: Thread for sock [%d] ended", str, cInfo.client->c_sock);
    }

    pthread_cleanup_pop(0);

    // Delete mysql
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
