/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef BMPLISTENER_H_
#define BMPLISTENER_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>

#include "Logger.h"
#include "Config.h"

using namespace std;

/**
 * \class   BMPListener
 *
 * \brief   Server class for the BMP instance
 * \details Maintains received connections and data from those connections.
 */
class BMPListener {
    int          sock;                           ///< Listening socket (ipv4)
    int          sockv6;                         ///< IPv6 listening socket
    sockaddr_in  svr_addr;                       ///< Server v4 address
    sockaddr_in6 svr_addrv6;                     ///< Server v6 address

public:
    /**
     * Structure for client information - received connection
     */
    class ClientInfo {
    public:
        u_char      hash_id[16];            ///< Hash ID for router (is the unique ID)
        sockaddr_storage c_addr;            ///< client address info
        sockaddr_storage s_addr;            ///< Server/collector address info
        int         c_sock;                 ///< Active client socket connection
        int         pipe_sock;              ///< Piped socket for client stream (buffered) - zero if not buffered
        char        c_port[6];              ///< Client source port
        char        c_ip[46];               ///< Client IP source address
        char        s_port[6];              ///< Server/collector port
        char        s_ip[46];               ///< Server/collector IP - printed form
    };

    /**
     * Class constructor
     *
     *  \param [in] logPtr  Pointer to existing Logger for app logging
     *  \param [in] config  Pointer to the loaded configuration
     *
     */
    BMPListener(Logger *logPtr, Config *config);

    virtual ~BMPListener();

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
    bool wait_and_accept_connection(ClientInfo &c, int timeout);

/**
     * Generate BMP router HASH
     *
     * \param [in,out] client   Refernce to client info used to generate the hash.
     *
     * \return client.hash_id will be updated with the generated hash
     */
    void hashRouter(ClientInfo &client);

    // Debug methods
    void enableDebug();
    void disableDebug();


public:
    Logger      *logger;                    ///< Logging class pointer

private:
    Config      *cfg;                       ///< Config pointer
    bool        debug;                      ///< debug flag to indicate debugging

    /**
     * Opens server (v4 or 6) listening socket(s)
     *
     * \param [in] ipv4     True to open v4 socket
     * \param [in] ipv6     True to open v6 socket
     */
    void open_socket(bool ipv4, bool ipv6);

    /**
     * Accept new/pending connections
     *
     * Will accept new connections and wait if one is not currently ready.
     * Supports IPv4 and IPv6 sockets
     *
     * \param [out]  c  Client information reference to where the client info will be stored
     * \param [in]   isIPv4  True to indicate if IPv4, false if IPv6
     */
    void accept_connection(ClientInfo &c, bool isIPv4);

};

#endif /* BMPListener_H_ */
