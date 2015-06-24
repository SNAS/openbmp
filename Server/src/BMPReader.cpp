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
#include <unistd.h>

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cerrno>

#include "BMPListener.h"
#include "BMPReader.h"
#include "parseBMP.h"
#include "parseBGP.h"
#include "DbInterface.hpp"


using namespace std;

/**
 * Class constructor
 *
 *  \param [in] logPtr  Pointer to existing Logger for app logging
 *  \param [in] config  Pointer to the loaded configuration
 *
 */
BMPReader::BMPReader(Logger *logPtr, Cfg_Options *config) {
    debug = false;

    cfg = config;

    logger = logPtr;

    if (cfg->debug_bmp)
        enableDebug();
}

/**
 * Destructor
 */
BMPReader::~BMPReader() {

}


/**
 * Read messages from BMP stream in a loop
 *
 * \param [in]  run         Reference to bool to indicate if loop should continue or not
 * \param [in]  client      Client information pointer
 * \param [in]  dbi_ptr     The database pointer referencer - DB should be already initialized
 *
 * \return true if more to read, false if the connection is done/closed
 *
 * \throw (char const *str) message indicate error
 */
void BMPReader::readerThreadLoop(bool &run, BMPListener::ClientInfo *client, DbInterface *dbi_ptr) {
    while (run) {

        try {
            if (not ReadIncomingMsg(client, dbi_ptr))
                break;

        } catch (char const *str) {
            run = false;
            break;
        }
    }
}

/**
 * Read messages from BMP stream
 *
 * BMP routers send BMP/BGP messages, this method reads and parses those.
 *
 * \param [in]  client      Client information pointer
 * \param [in]  dbi_ptr     The database pointer referencer - DB should be already initialized
 *
 * \return true if more to read, false if the connection is done/closed
 *
 * \throw (char const *str) message indicate error
 */
bool BMPReader::ReadIncomingMsg(BMPListener::ClientInfo *client, DbInterface *dbi_ptr) {
    bool rval = true;
    parseBGP *pBGP;                                 // Pointer to BGP parser

    int read_fd = client->pipe_sock > 0 ? client->pipe_sock : client->c_sock;

    // Data storage structures
    DbInterface::tbl_bgp_peer p_entry;

    // Initialize the parser for BMP messages
    parseBMP *pBMP = new parseBMP(logger, &p_entry);    // handler for BMP messages

    if (cfg->debug_bmp) {
        enableDebug();
        pBMP->enableDebug();
    }

    char bmp_type = 0;

    DbInterface::tbl_router r_entry;
    bzero(&r_entry, sizeof(r_entry));
    memcpy(r_entry.hash_id, router_hash_id, sizeof(r_entry.hash_id));

    // Setup the router record table object
    r_entry.isConnected = 1;
    memcpy(r_entry.src_addr, client->c_ipv4, sizeof(client->c_ipv4));

    try {
        bmp_type = pBMP->handleMessage(read_fd);

        /*
         * Now that we have parsed the BMP message...
         *  add record to the database
         */

        dbi_ptr->add_Router(r_entry);         // add the router entry
        memcpy(router_hash_id, r_entry.hash_id, sizeof(router_hash_id));            // Cache the hash ID

        // only process the peering info if the message includes it
        if (bmp_type < 4) {
            // Update p_entry hash_id now that add_Router updated it.
            memcpy(p_entry.router_hash_id, r_entry.hash_id, sizeof(r_entry.hash_id));

            dbi_ptr->add_Peer(p_entry);           // add the peer entry
        }

        /*
         * At this point we only have the BMP header message, what happens next depends
         *      on the BMP message type.
         */
        switch (bmp_type) {
            case parseBMP::TYPE_PEER_DOWN : { // Peer down type

                DbInterface::tbl_peer_down_event down_event = {};

                if (pBMP->parsePeerDownEventHdr(read_fd,down_event)) {
                    pBMP->bufferBMPMessage(read_fd);


                    // Prepare the BGP parser
                    pBGP = new parseBGP(logger, dbi_ptr, &p_entry, (char *)r_entry.src_addr,
                                        &peer_info_map[string(reinterpret_cast<char*>(p_entry.hash_id))]);

                    if (cfg->debug_bgp)
                       pBGP->enableDebug();

                    // Check if the reason indicates we have a BGP message that follows
                    switch (down_event.bmp_reason) {
                        case 1 : { // Local system close with BGP notify
                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Local close by (%s) for peer (%s) : ", r_entry.src_addr,
                                    p_entry.peer_addr);
                            pBGP->handleDownEvent(pBMP->bmp_data, pBMP->bmp_data_len, down_event);
                            break;
                        }
                        case 2 : // Local system close, no bgp notify
                        {
                            // Read two byte code corresponding to the FSM event
                            uint16_t fsm_event = 0 ;
                            memcpy(&fsm_event, pBMP->bmp_data, 2);
                            bgp::SWAP_BYTES(&fsm_event);

                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Local (%s) closed peer (%s) session: fsm_event=%d, No BGP notify message.",
                                    r_entry.src_addr,p_entry.peer_addr, fsm_event);
                            break;
                        }
                        case 3 : { // remote system close with bgp notify
                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Remote peer (%s) closed local (%s) session: ", r_entry.src_addr,
                                    p_entry.peer_addr);

                            pBGP->handleDownEvent(pBMP->bmp_data, pBMP->bmp_data_len, down_event);
                            break;
                        }
                    }

                    delete pBGP;            // Free the bgp parser after each use.

                    // Add event to the database
                    dbi_ptr->add_PeerDownEvent(down_event);

                } else {
                    LOG_ERR("Error with client socket %d", read_fd);
                    // Make sure to free the resource
                    throw "BMPReader: Unable to read from client socket";
                }
                break;
            }

            case parseBMP::TYPE_PEER_UP : // Peer up type
            {
                DbInterface::tbl_peer_up_event up_event = {};
                peer_info pInfo = {};

                if (pBMP->parsePeerUpEventHdr(read_fd, up_event)) {
                    LOG_INFO("%s: PEER UP Received, local addr=%s:%hu remote addr=%s:%hu", client->c_ipv4,
                            up_event.local_ip, up_event.local_port, p_entry.peer_addr, up_event.remote_port);

                    pBMP->bufferBMPMessage(read_fd);

                    // Prepare the BGP parser
                    pBGP = new parseBGP(logger, dbi_ptr, &p_entry, (char *)r_entry.src_addr,
                                        &peer_info_map[string(reinterpret_cast<char*>(p_entry.hash_id))]);

                    if (cfg->debug_bgp)
                       pBGP->enableDebug();

                    // Parse the BGP sent/received open messages
                    pBGP->handleUpEvent(pBMP->bmp_data, pBMP->bmp_data_len, &up_event);

                    // Free the bgp parser
                    delete pBGP;

                    // Add the up event to the DB
                    dbi_ptr->add_PeerUpEvent(up_event);

                } else {
                    LOG_NOTICE("%s: PEER UP Received but failed to parse the BMP header.", client->c_ipv4);
                }
                break;
            }

            case parseBMP::TYPE_ROUTE_MON : { // Route monitoring type
                pBMP->bufferBMPMessage(read_fd);

                /*
                 * Read and parse the the BGP message from the client.
                 *     parseBGP will update mysql directly
                 */
                pBGP = new parseBGP(logger, dbi_ptr, &p_entry, (char *)r_entry.src_addr,
                                    &peer_info_map[string(reinterpret_cast<char*>(p_entry.hash_id))]);
                if (cfg->debug_bgp)
                    pBGP->enableDebug();

                pBGP->handleUpdate(pBMP->bmp_data, pBMP->bmp_data_len);
                delete pBGP;

                break;
            }

            case parseBMP::TYPE_STATS_REPORT : { // Stats Report
                DbInterface::tbl_stats_report stats = {};
                if (! pBMP->handleStatsReport(read_fd, stats))
                    // Add to mysql
                    dbi_ptr->add_StatReport(stats);

                break;
            }

            case parseBMP::TYPE_INIT_MSG : { // Initiation Message
                LOG_INFO("%s: Init message received with length of %u", client->c_ipv4, pBMP->getBMPLength());
                pBMP->handleInitMsg(read_fd, r_entry);

                // Update the router entry with the details
                dbi_ptr->update_Router(r_entry);
                break;
            }

            case parseBMP::TYPE_TERM_MSG : { // Termination Message
                LOG_INFO("%s: Term message received with length of %u", client->c_ipv4, pBMP->getBMPLength());


                pBMP->handleTermMsg(read_fd, r_entry);

                LOG_INFO("Proceeding to disconnect router");
                dbi_ptr->disconnect_Router(r_entry);
                close(client->c_sock);

                rval = false;                           // Indicate connection is closed
                break;
            }

        }

    } catch (char const *str) {
        // Mark the router as disconnected and update the error to be a local disconnect (no term message received)
        LOG_INFO("%s: Caught: %s", client->c_ipv4, str);
        disconnect(client, dbi_ptr, 65534, str);

        delete pBMP;                    // Make sure to free the resource
        throw str;
    }

    // Free the bmp parser
    delete pBMP;

    return rval;
}

/**
 * disconnect/close bmp stream
 *
 * Closes the BMP stream and disconnects router as needed
 *
 * \param [in]  client      Client information pointer
 * \param [in]  dbi_ptr     The database pointer referencer - DB should be already initialized
 * \param [in]  reason_code The reason code for closing the stream/feed
 * \param [in]  reason_text String detailing the reason for close
 *
 */
void BMPReader::disconnect(BMPListener::ClientInfo *client, DbInterface *dbi_ptr, int reason_code, char const *reason_text) {

    DbInterface::tbl_router r_entry;
    bzero(&r_entry, sizeof(r_entry));
    memcpy(r_entry.hash_id, router_hash_id, sizeof(r_entry.hash_id));

    // Mark the router as disconnected and update the error to be a local disconnect (no term message received)
    LOG_INFO("%s: Disconnecting router", client->c_ipv4);

    r_entry.term_reason_code = reason_code;
    if (reason_text != NULL)
        snprintf(r_entry.term_reason_text, sizeof(r_entry.term_reason_text), "%s", reason_text);

    dbi_ptr->disconnect_Router(r_entry);

    close(client->c_sock);
}

/*
 * Enable/Disable debug
 */
void BMPReader::enableDebug() {
    debug = true;
}

void BMPReader::disableDebug() {
    debug = false;
}
