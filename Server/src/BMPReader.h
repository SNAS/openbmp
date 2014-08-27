/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef BMPREADER_H_
#define BMPREADER_H_

#include "BMPListener.h"
#include "DbInterface.hpp"
#include "Logger.h"
#include "Config.h"

using namespace std;

/**
 * \class   BMPReader
 *
 * \brief   Server class for the BMP instance
 * \details Maintains received connections and data from those connections.
 */
class BMPReader {

public:

/*    enum ADDR_TYPES {
        ADDR_IPV4, ADDR_IPV6, DNS
    };
*/

    /**
     * Class constructor
     *
     *  \param [in] logPtr  Pointer to existing Logger for app logging
     *  \param [in] config  Pointer to the loaded configuration
     *
     */
    BMPReader(Logger *logPtr, Cfg_Options *config);

    virtual ~BMPReader();

    /**
     * Read messages from BMP stream
     *
     * BMP routers send BMP/BGP messages, this method reads and parses those.
     *
     * \param [in]  client      Client information pointer
     * \param [in]  dbi_ptr     The database pointer referencer - DB should be already initialized
     */
    void ReadIncomingMsg(BMPListener::ClientInfo *client, DbInterface *dbi_ptr);

    // Debug methods
    void enableDebug();
    void disableDebug();


public:
    Logger      *log;                       ///< Logging class pointer

private:
    Cfg_Options *cfg;                       ///< Config pointer
    bool        debug;                      ///< debug flag to indicate debugging
};

#endif /* BMPReader_H_ */
