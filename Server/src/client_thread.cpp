/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
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
    Logger *logger = cInfo->log;

    LOG_INFO("Thread terminating due to cancel request.");

    LOG_INFO("Closing client connection to %s:%s", cInfo->client->c_ipv4, cInfo->client->c_port);
    shutdown(cInfo->client->c_sock, SHUT_RDWR);
    close (cInfo->client->c_sock);

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
        LOG_INFO("Thread started to monitor BMP from router %s using socket %d",
                cInfo.client->c_ipv4, cInfo.client->c_sock);

        bool run = true;
        while (run) {
            run = rBMP.ReadIncomingMsg(cInfo.client, (DbInterface *)cInfo.mysql);
        }
        LOG_INFO("%s: Thread for sock [%d] ended normally", cInfo.client->c_ipv4, cInfo.client->c_sock);

    } catch (char const *str) {
        LOG_INFO("%s: %s - Thread for sock [%d] ended", cInfo.client->c_ipv4, str, cInfo.client->c_sock);
    }

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
