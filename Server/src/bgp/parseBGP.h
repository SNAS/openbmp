/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef PARSEBGP_H_
#define PARSEBGP_H_

#include <vector>
#include <list>
#include <BMPReader.h>
#include <parseBgpLib.h>
#include "MsgBusInterface.hpp"
#include "Logger.h"
#include "bgp_common.h"
#include "parseBGP.h"


using namespace std;

/**
 * \class   parseBGP
 *
 * \brief   Parser for BGP messages
 * \details This class can be used as needed to parse a complete BGP message. This
 *          class will read directly from the socket to read the BGP message.
 */
class parseBGP {
public:
    /**
     * Below defines the common BGP header per RFC4271
     */
    enum BGP_MSG_TYPES { BGP_MSG_OPEN=1, BGP_MSG_UPDATE, BGP_MSG_NOTIFICATION, BGP_MSG_KEEPALIVE,
        BGP_MSG_ROUTE_REFRESH
    };

    struct common_bgp_hdr {
        /**
         * 16-octet field is included for compatibility
         * All ones (required).
         */
        unsigned char    marker[16];

        /**
         * Total length of the message, including the header in octets
         *
         * min length is 19, max is 4096
         */
        unsigned short   len;

        /**
         * type code of the message
         *
         * 1 - OPEN
         * 2 - UPDATE
         * 3 - NOTIFICATION
         * 4 - KEEPALIVE
         * 5 - ROUTE-REFRESH
         */
        unsigned char    type;
    } __attribute__ ((__packed__));


    /**
     * Constructor for class -
     *
     * \details
     *    This class parses the BGP message and updates DB.  The
     *    'mysql_ptr' must be a pointer reference to an open mysql connection.
     *    'peer_entry' must be a pointer to the peer_entry table structure that
     *    has already been populated.
     *
     * \param [in]     logPtr      Pointer to existing Logger for app logging
     * \param [in]     mbus_ptr     Pointer to exiting dB implementation
     * \param [in,out] peer_entry  Pointer to peer entry
     * \param [in]     routerAddr  The router IP address - used for logging
     * \param [in,out] peer_info   Persistent peer information
     */
    parseBGP(Logger *logPtr, MsgBusInterface *mbus_ptr, MsgBusInterface::obj_bgp_peer *peer_entry, string routerAddr,
             BMPReader::peer_info *peer_info);

    virtual ~parseBGP();

    /**
     * handle BGP update message and store in DB
     *
     * \details Parses the bgp update message and store it in the DB.
     *
     * \param [in]     data             Pointer to the raw BGP message header
     * \param [in]     size             length of the data buffer (used to prevent overrun)

     *
     * \returns True if error, false if no error.
     */
    bool handleUpdate(u_char *data, size_t size);

    /**
     * handle BGP notify event - updates the down event with parsed data
     *
     * \details
     *  The notify message does not directly add to Db, so the calling
     *  method/function must handle that.
     *
     * \param [in]     data             Pointer to the raw BGP message header
     * \param [in]     size             length of the data buffer (used to prevent overrun)
     * \param [out]    down_event       Reference to the down event/notification storage buffer
     *
     * \returns True if error, false if no error.
     */
    bool handleDownEvent(u_char *data, size_t size, MsgBusInterface::obj_peer_down_event &down_event);

    /**
     * Handles the up event by parsing the BGP open messages - Up event will be updated
     *
     * \details
     *  This method will read the expected sent and receive open messages.
     *
     * \param [in]     data             Pointer to the raw BGP message header
     * \param [in]     size             length of the data buffer (used to prevent overrun)
     *
     * \returns True if error, false if no error.
     */
    bool handleUpEvent(u_char *data, size_t size, MsgBusInterface::obj_peer_up_event *up_event);

    /*
     * Debug methods
     */
    void enableDebug();
    void disableDebug();


private:
    /**
     * data_bytes_remaining is a counter that starts at the message size and then is
     * decremented as the message is read.
     *
     * This is used to ensure no buffer overruns on bgp data buffer
     */
    unsigned int data_bytes_remaining;

    /**
     * BGP data buffer for the raw BGP message to be parsed
     */
    unsigned char *data;                             ///< Pointer to the data buffer for the raw BGP message

    common_bgp_hdr common_hdr;                       ///< Current/last pased bgp common header

    MsgBusInterface::obj_bgp_peer    *p_entry;       ///< peer table entry - will be updated with BMP info

    MsgBusInterface *mbus_ptr;                       ///< Pointer to open DB implementation
    string                           router_addr;    ///< Router IP address - used for logging
    BMPReader::peer_info             *p_info;        ///< Persistent Peer information

    bool            debug;                           ///< debug flag to indicate debugging
    Logger          *logger;                         ///< Logging class pointer

    /**
     * Parses the BGP common header
     *
     * \details
     *      This method will parse the bgp common header and will upload the global
     *      c_hdr structure, instance data pointer, and remaining bytes of message.
     *      The return value of this method will be the BGP message type.
     *
     * \param [in]      data            Pointer to the raw BGP message header
     * \param [in]      size            length of the data buffer (used to prevent overrun)
     *
     * \returns BGP message type
     */
    u_char parseBgpHeader(u_char *data, size_t size);

    /**
     * Update the Database with the parsed updated data
     *
     * \details This method will update the database based on the supplied parsed update data
     *
     * \param  parsed_data          Reference to the parsed update data
     */
    void UpdateDB(parse_bgp_lib::parseBgpLib::parsed_update &update);

};

#endif /* PARSEBGP_H_ */
