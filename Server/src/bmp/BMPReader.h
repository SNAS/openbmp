/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef BMPREADER_H_
#define BMPREADER_H_

#include "BMPListener.h"
#include "BMPReader.h"
#include "AddPathDataContainer.h"
#include "MsgBusInterface.hpp"
#include "Logger.h"
#include "Config.h"

#include <map>
#include <memory>

/**
 * \class   BMPReader
 *
 * \brief   Server class for the BMP instance
 * \details Maintains received connections and data from those connections.
 */
class BMPReader {

public:
    /**
     * Persistent peer information structure
     *
     *   OPEN and other updates can add/change persistent peer information.
     */
    struct peer_info {
        bool sent_four_octet_asn;                               ///< Indicates if 4 (true) or 2 (false) octet ASN is being used (sent cap)
        bool recv_four_octet_asn;                               ///< Indicates if 4 (true) or 2 (false) octet ASN is being used (recv cap)
        bool using_2_octet_asn;                                 ///< Indicates if peer is using two octet ASN format or not (true=2 octet, false=4 octet)
        bool checked_asn_octet_length;                          ///< Indicates if the ASN octet length has been checked or not
        shared_ptr<AddPathDataContainer> add_path_capability;   ///< Stores data about Add Path capability
        string peer_group;                                      ///< Peer group name of defined
    };


    /**
     * Class constructor
     *
     *  \param [in] logPtr  Pointer to existing Logger for app logging
     *  \param [in] config  Pointer to the loaded configuration
     *
     */
    BMPReader(Logger *logPtr, Config *config);

    virtual ~BMPReader();

    /**
     * Read messages from BMP stream
     *
     * BMP routers send BMP/BGP messages, this method reads and parses those.
     *
     * \param [in]  client      Client information pointer
     * \param [in]  mbus_ptr     The database pointer referencer - DB should be already initialized
     * \return true if more to read, false if the connection is done/closed
     */
    bool ReadIncomingMsg(BMPListener::ClientInfo *client, MsgBusInterface *mbus_ptr);

    /**
     * Read messages from BMP stream in a loop
     *
     * \param [in]  run         Reference to bool to indicate if loop should continue or not
     * \param [in]  client      Client information pointer
     * \param [in]  mbus_ptr     The database pointer referencer - DB should be already initialized
     *
     * \return true if more to read, false if the connection is done/closed
     *
     * \throw (char const *str) message indicate error
     */
    void readerThreadLoop(bool &run, BMPListener::ClientInfo *client, MsgBusInterface *mbus_ptr);

    /**
     * disconnect/close bmp stream
     *
     * Closes the BMP stream and disconnects router as needed
     *
     * \param [in]  client      Client information pointer
     * \param [in]  mbus_ptr     The database pointer referencer - DB should be already initialized
     * \param [in]  reason_code The reason code for closing the stream/feed
     * \param [in]  reason_text String detailing the reason for close
     *
     */
    void disconnect(BMPListener::ClientInfo *client, MsgBusInterface *mbus_ptr, int reason_code, char const *reason_text);

    // Debug methods
    void enableDebug();
    void disableDebug();


public:
    Logger      *logger;                    ///< Logging class pointer

private:
    Config      *cfg;                       ///< Config pointer
    bool        debug;                      ///< debug flag to indicate debugging
    u_char      router_hash_id[16];         ///< Router hash ID


    /**
     * Persistent peer info map, Key is the peer_hash_id.
     */
    std::map<std::string, peer_info> peer_info_map;
    typedef std::map<std::string, peer_info>::iterator peer_info_map_iter;

};

#endif /* BMPReader_H_ */
