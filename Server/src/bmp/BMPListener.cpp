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

#include <poll.h>
#include "parseBgpLib.h"
#include <MsgBusInterface.hpp>

#include "BMPListener.h"
#include "md5.h"

using namespace std;

/**
 * Class constructor
 *
 *  \param [in] logPtr  Pointer to existing Logger for app logging
 *  \param [in] config  Pointer to the loaded configuration
 *
 */
BMPListener::BMPListener(Logger *logPtr, Config *config) {
    sock = 0;
    sockv6 = 0;
    debug = false;

    // Update pointer to the config
    cfg = config;

    logger = logPtr;

    if (cfg->debug_bmp)
        enableDebug();

    svr_addr.sin_family      = PF_INET;
    svr_addr.sin_port        = htons(cfg->bmp_port);
    svr_addr.sin_addr.s_addr = INADDR_ANY;

    svr_addrv6.sin6_family   = AF_INET6;
    svr_addrv6.sin6_port     = htons(cfg->bmp_port);
    svr_addrv6.sin6_scope_id = 0;
    svr_addrv6.sin6_addr     = in6addr_any;

    // Open listening sockets
    open_socket(cfg->svr_ipv4, cfg->svr_ipv6);
}

/**
 * Destructor
 */
BMPListener::~BMPListener() {
    if (sock > 0)
        close(sock);
    if (sockv6 > 0)
        close(sockv6);

    delete cfg;
}

/**
 * Opens server (v4 or 6) listening socket(s)
 *
 * \param [in] ipv4     True to open v4 socket
 * \param [in] ipv6     True to open v6 socket
 */
void BMPListener::open_socket(bool ipv4, bool ipv6) {
    int on = 1;

    if (ipv4) {
        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            throw "ERROR: Cannot open IPv4 socket.";
        }

        // Set socket options
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            close(sock);
            throw "ERROR: Failed to set IPv4 socket option SO_REUSEADDR";
        }

        // Bind to the address/port
        if (::bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr)) < 0) {
            close(sock);
            throw "ERROR: Cannot bind to IPv4 address and port";
        }

        // listen for incoming connections
        listen(sock, 10);
    }

    if (ipv6) {
        if ((sockv6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
            throw "ERROR: Cannot open IPv6 socket.";
        }

        // Set socket options
        if (setsockopt(sockv6, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            close(sockv6);
            throw "ERROR: Failed to set IPv6 socket option SO_REUSEADDR";
        }

        if (setsockopt(sockv6, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
            close(sockv6);
            throw "ERROR: Failed to set IPv6 socket option IPV6_V6ONLY";
        }

        // Bind to the address/port
        if (::bind(sockv6, (struct sockaddr *) &svr_addrv6, sizeof(svr_addrv6)) < 0) {
            perror("bind to ipv6");
            close(sockv6);
            throw "ERROR: Cannot bind to IPv6 address and port";
        }

        // listen for incoming connections
        listen(sockv6, 10);
    }
}

/**
 * Wait and Accept new/pending connections
 *
 * Will accept both IPv4 and IPv6 (if configured), but only one will be accepted
 * per poll/wait period.  Must run this method in a loop fashion to accept all
 * pending connections.
 *
 * \param [out] c           Ref to client info - this will be updated based on accepted connection
 * \param [in]  timeout     Timeout in ms to wait for
 *
 * \return  True if accepted a connection, false if not (timed out waiting)
 */
bool BMPListener::wait_and_accept_connection(ClientInfo &c, int timeout) {
    pollfd pfd[4];
    int fds_cnt = 0;
    int cur_sock = 0;
    bool close_sock = false;

    if (sock > 0) {
        pfd[fds_cnt].fd = sock;
        pfd[fds_cnt].events = POLLIN | POLLHUP | POLLERR;
        pfd[fds_cnt].revents = 0;
        fds_cnt++;
    }

    if (sockv6 > 0) {
        pfd[fds_cnt].fd = sockv6;
        pfd[fds_cnt].events = POLLIN | POLLHUP | POLLERR;
        pfd[fds_cnt].revents = 0;
        fds_cnt++;
    }

    // Check if the listening socket has a new connection
    if (poll(pfd, fds_cnt + 1, timeout)) {

        for (int i = 0; i <= fds_cnt; i++) {
            if (pfd[i].revents & POLLHUP or pfd[i].revents & POLLERR) {
                LOG_WARN("sock=%d: received POLLHUP/POLLHERR while accepting", pfd[i].fd);
                cur_sock = pfd[i].fd;
                close_sock = true;
                break;

            } else if (pfd[i].revents & POLLIN) {
                cur_sock = pfd[i].fd;
                break;
            }
        }
    }

    if (cur_sock > 0) {
        if (close_sock) {
            if (cur_sock == sock)
                close(sock);
            else if (cur_sock == sockv6)
                close(sockv6);
        }

        else {
            if (cur_sock == sock)  // IPv4
                accept_connection(c, true);

            else // IPv6
                accept_connection(c, false);

            return true;
        }
    }

    return false;
}



/**
 * Accept new/pending connections
 *
 * Will accept new connections and wait if one is not currently ready.
 * Supports IPv4 and IPv6 sockets
 *
 * \param [out]  c       Client information reference to where the client info will be stored
 * \param [in]   isIPv4  True to indicate if IPv4, false if IPv6
 */
void BMPListener::accept_connection(ClientInfo &c, bool isIPv4) {
    socklen_t c_addr_len = sizeof(c.c_addr);         // the client info length
    socklen_t s_addr_len = sizeof(c.s_addr);         // the client info length

    int sock = isIPv4 ? this->sock : this->sockv6;

    sockaddr_in *v4_addr = (sockaddr_in *) &c.c_addr;
    sockaddr_in6 *v6_addr = (sockaddr_in6 *) &c.c_addr;

    uint8_t addr_fam = isIPv4 ? PF_INET : PF_INET6;

    bzero(c.c_ip, sizeof(c.s_ip));
    bzero(c.c_ip, sizeof(c.c_ip));

    // Accept the pending client request, or block till one exists
    if ((c.c_sock = accept(sock, (struct sockaddr *) &c.c_addr, &c_addr_len)) < 0) {
        string error = "Server accept connection: ";
        if (errno != EINTR)
            error += strerror(errno);
        else
            error += "Exiting normally per user request to stop server";

        throw error.c_str();
    }

    // Update returned class to have address and port of client in text form.
    if (isIPv4) {
        inet_ntop(AF_INET, &v4_addr->sin_addr, c.c_ip, sizeof(c.c_ip));
        snprintf(c.c_port, sizeof(c.c_port), "%hu", ntohs(v4_addr->sin_port));
    } else {
        inet_ntop(AF_INET6,  &v6_addr->sin6_addr, c.c_ip, sizeof(c.c_ip));
        snprintf(c.c_port, sizeof(c.c_port), "%hu", ntohs(v6_addr->sin6_port));
    }

    // Get the server source address and port
    v4_addr = (sockaddr_in *) &c.s_addr;
    v6_addr = (sockaddr_in6 *) &c.s_addr;

    if (!getsockname(c.c_sock, (struct sockaddr *) &c.s_addr, &s_addr_len)) {
        if (isIPv4) {
            inet_ntop(AF_INET, &v4_addr->sin_addr, c.s_ip, sizeof(c.s_ip));
            snprintf(c.s_port, sizeof(c.s_port), "%hu", ntohs(v4_addr->sin_port));
        } else {
            inet_ntop(AF_INET, &v6_addr->sin6_addr, c.s_ip, sizeof(c.s_ip));
            snprintf(c.s_port, sizeof(c.s_port), "%hu", ntohs(v6_addr->sin6_port));
        }

    } else {
        LOG_ERR("sock=%d: Unable to get the socket name/local address information", c.c_sock);


    }

    // Enable TCP Keepalives
    int on = 1;
    if (setsockopt(c.c_sock, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
        LOG_NOTICE("%s: sock=%d: Unable to enable tcp keepalives", c.c_ip, c.c_sock);
    }

    hashRouter(c);
}


/**
 * Generate BMP router HASH
 *
 * \param [in,out] client   Reference to client info used to generate the hash.
 *
 * \return client.hash_id will be updated with the generated hash
 */
void BMPListener::hashRouter(ClientInfo &client) {
    string c_hash_str;
    MsgBusInterface::hash_toStr(cfg->c_hash_id, c_hash_str);

    MD5 hash;
    hash.update((unsigned char *)client.c_ip, strlen(client.c_ip));
    hash.update((unsigned char *)c_hash_str.c_str(), c_hash_str.length());
    hash.finalize();

    // Save the hash
    unsigned char *hash_bin = hash.raw_digest();
    memcpy(client.hash_id, hash_bin, 16);
    delete[] hash_bin;
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
