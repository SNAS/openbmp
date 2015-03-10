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
    int         sock;                           ///< Listening socket
    sockaddr_in svr_addr;                       ///< Server address

public:

/*    enum ADDR_TYPES {
        ADDR_IPV4, ADDR_IPV6, DNS
    };
*/

    /**
     * Structure for client information - received connection
     */
    class ClientInfo {
    public:
        sockaddr_in c_addr;                 ///< client address info
        int         c_sock;                 ///< Active client socket connection
        char        c_port[6];              ///< Client source port
        char        c_ipv4[16];             ///< Client IPv4 source address
    };

    /**
     * Class constructor
     *
     *  \param [in] logPtr  Pointer to existing Logger for app logging
     *  \param [in] config  Pointer to the loaded configuration
     *
     */
    BMPListener(Logger *logPtr, Cfg_Options *config);

    virtual ~BMPListener();

    /**
     * Accept new/pending connections
     *
     * Will accept new connections and wait if one is not currently ready.
     *
     * \param [out]  c  Client information reference to where the client info will be stored
     */
    void accept_connection(ClientInfo &c);

    // Debug methods
    void enableDebug();
    void disableDebug();


public:
    Logger      *logger;                    ///< Logging class pointer

private:
    Cfg_Options *cfg;                       ///< Config pointer
    bool        debug;                      ///< debug flag to indicate debugging

    /**
     * Opens server IPv4 listening socket
     */
    void open_socket_v4();
};

#endif /* BMPListener_H_ */
