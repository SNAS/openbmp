/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <sys/types.h>
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

    log = logPtr;

    if (cfg->debug_bmp)
        enableDebug();
}

/**
 * Destructor
 */
BMPReader::~BMPReader() {

}

/**
 * Read messages from BMP stream
 *
 * BMP routers send BMP/BGP messages, this method reads and parses those.
 *
 * \param [in]  client      Client information pointer
 * \param [in]  dbi_ptr     The database pointer referencer - DB should be already initialized
 */
void BMPReader::ReadIncomingMsg(BMPListener::ClientInfo *client, DbInterface *dbi_ptr) {
    parseBGP *pBGP;                                 // Pointer to BGP parser

    // Data storage structures
    DbInterface::tbl_bgp_peer p_entry;

    // Initialize the parser for BMP messages
    parseBMP *pBMP = new parseBMP(log, &p_entry);    // handler for BMP messages

    if (cfg->debug_bmp) {
        enableDebug();
        pBMP->enableDebug();
    }

    char bmp_type = 0;
    DbInterface::tbl_router r_entry;

    // Setup the router record table object
    r_entry.isConnected = 1;
    memcpy(r_entry.src_addr, client->c_ipv4, sizeof(client->c_ipv4));

    try {
        bmp_type = pBMP->handleMessage(client->c_sock);

        /*
         * Now that we have parsed the BMP message...
         *  add record to the database
         */

        dbi_ptr->add_Router(r_entry);         // add the router entry

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
                // Read the reason code
                char reason;
                if (read(client->c_sock, &reason, 1) == 1) {
                    LOG_NOTICE("sock=%d : %s: BGP peer down notification with reason code: %d",
                            client->c_sock, p_entry.peer_addr, reason);

                    DbInterface::tbl_peer_down_event down_event = {0};

                    // Initialize the down_event struct
                    down_event.bmp_reason = reason;
                    memcpy(down_event.peer_hash_id, p_entry.hash_id, sizeof(p_entry.hash_id));

                    // Prepare the BGP parser
                    pBGP = new parseBGP(log, dbi_ptr, &p_entry);
                    if (cfg->debug_bgp)
                       pBGP->enableDebug();

                    // Check if the reason indicates we have a BGP message that follows
                    switch (reason) {
                        case 1 : { // Local system close with BGP notify
                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Local close by (%s) for peer (%s) : ", r_entry.src_addr,
                                    p_entry.peer_addr);
                            pBGP->handleMessage(client->c_sock, &down_event);
                            break;
                        }
                        case 2 : // Local system close, no bgp notify
                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Local (%s) closed peer (%s) session: No BGP notify message.", r_entry.src_addr,
                                    p_entry.peer_addr);
                            break;

                        case 3 : { // remote system close with bgp notify
                            snprintf(down_event.error_text, sizeof(down_event.error_text),
                                    "Remote peer (%s) closed local (%s) session: ", r_entry.src_addr,
                                    p_entry.peer_addr);

                            pBGP->handleMessage(client->c_sock, &down_event);
                            break;
                        }
                    }

                    delete pBGP;            // Free the bgp parser after each use.

                    // Add event to the database
                    dbi_ptr->add_PeerDownEvent(down_event);

                } else {
                    LOG_ERR("Error with client socket %d", client->c_sock);
                    throw "BMPReader: Unable to read from client socket";
                }
                break;
            }

            case parseBMP::TYPE_PEER_UP : // Peer up type
                // TODO: Check the peer up message before parsing the OPEN message
                LOG_INFO("%s: PEER UP Received and not supported by this daemon yet", client->c_ipv4);
                break;

            case parseBMP::TYPE_ROUTE_MON : { // Route monitoring type
                /*
                 * Read and parse the the BGP message from the client.
                 *     parseBGP will update mysql directly
                 */
                pBGP = new parseBGP(log, dbi_ptr, &p_entry);
                if (cfg->debug_bgp)
                    pBGP->enableDebug();

                pBGP->handleMessage(client->c_sock);
                delete pBGP;
                break;
            }

            case parseBMP::TYPE_STATS_REPORT : { // Stats Report
                pBMP->handleStatsReport(dbi_ptr, client->c_sock);
                break;
            }

            case parseBMP::TYPE_INIT_MSG : { // Initiation Message
                LOG_INFO("%s: Init message received with length of %u", client->c_ipv4, pBMP->getBMPLength());
                pBMP->handleInitMsg(r_entry, dbi_ptr, client->c_sock);
                break;
            }

        }

    } catch (const char *str) {
        // Record that the connection is no longer active

        // Mark the router as disconnected and update the error to be a local disconnect (no term message received)
        r_entry.term_reason_code = 65535;
        snprintf(r_entry.term_reason_text, sizeof(r_entry.term_reason_text), "%s", str);
        dbi_ptr->disconnect_Router(r_entry);

        delete pBMP;                    // Make sure to free the resource
        throw str;
    }

    // Free the bmp parser
    delete pBMP;
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
