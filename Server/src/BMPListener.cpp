/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <iostream>
#include <cerrno>
#include <string>

#include "BMPListener.h"

using namespace std;

/**
 * Class constructor
 *
 *  \param [in] logPtr  Pointer to existing Logger for app logging
 *  \param [in] config  Pointer to the loaded configuration
 *
 */
BMPListener::BMPListener(Logger *logPtr, Cfg_Options *config) {
    sock = 0;
    debug = false;

    // Update pointer to the config
    cfg = config;

    logger = logPtr;

    if (cfg->debug_bmp)
        enableDebug();

    svr_addr.sin_port = htons(atoi(cfg->bmp_port));
    svr_addr.sin_addr.s_addr = INADDR_ANY;

    // Open the v4 socket
    open_socket_v4();
}

/**
 * Destructor
 */
BMPListener::~BMPListener() {
    if (sock > 0)
        close (sock);

    delete cfg;
}

/**
 * Opens server IPv4 listening socket
 */
void BMPListener::open_socket_v4() {
   svr_addr.sin_family = PF_INET;

   // Open the socket
   if ( (sock=socket(PF_INET, SOCK_STREAM, 0))  <= 0) {
       throw "ERROR: Cannot open a socket.";
   }

   // Set socket options
   int on = 1;
   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
       close(sock);
       throw "ERROR: Failed to set socket option SO_REUSEADDR";
   }

   // Bind to the address/port
    if (::bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr)) < 0) {
       close(sock);
       throw "ERROR: Cannot bind to address and port";
   }

   // Wait for incoming connections
   listen(sock, 20);
}

/**
 * Accept new/pending connections
 *
 * Will accept new connections and wait if one is not currently ready.
 *
 * \param [out]  c  Client information reference to where the client info will be stored
 */
void BMPListener::accept_connection(ClientInfo &c) {
   socklen_t c_addr_len = sizeof(c.c_addr);         // the client info length

   // Accept the pending client request, or block till one exists
   if ( (c.c_sock = accept(sock, (struct sockaddr *) &c.c_addr, &c_addr_len)) < 0) {
       string error = "Server accept connection: ";
       if (errno != EINTR)
           error += strerror(errno);
       else
           error += "Exiting normally per user request to stop server";

       throw error.c_str();
   }

   // Update returned class to have address and port of client in text form.
   snprintf(c.c_ipv4, sizeof(c.c_ipv4), "%s", inet_ntoa(c.c_addr.sin_addr));
   snprintf(c.c_port, sizeof(c.c_port), "%hu", ntohs(c.c_addr.sin_port));

   // Enable TCP Keepalives
   int on = 1;
   if(setsockopt(c.c_sock, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
       LOG_NOTICE("%s: sock=%d: Unable to enable tcp keepalives", c.c_ipv4, c.c_sock);
   }
}


/*
 * Enable/Disable debug
 */
void BMPListener::enableDebug() {
    debug = true;
}

void BMPListener::disableDebug() {
    debug = false;
}
