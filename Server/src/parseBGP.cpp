/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "parseBGP.h"

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <vector>
#include <list>
#include <arpa/inet.h>

#include "DbInterface.hpp"

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
 * \param [in]     dbi_ptr     Pointer to exiting dB implementation
 * \param [in,out] peer_entry  Pointer to peer entry
 */
parseBGP::parseBGP(Logger *logPtr, DbInterface *dbi_ptr, DbInterface::tbl_bgp_peer *peer_entry) {
    debug = false;

    log = logPtr;

    // Set our mysql pointer
    this->dbi_ptr = dbi_ptr;

    // Set our peer entry
    p_entry = peer_entry;

    // Set the default ASN size
    peer_asn_len = 4;

    // Initialize the structure vars that will need to be deleted if used by destructor
    upd_hdr.withdrawn_routes = NULL;
    upd_hdr.path_attrs = NULL;
    upd_hdr.nlri_routes = NULL;
}

/**
 * Desctructor
 */
parseBGP::~parseBGP() {

    // free the path attributes
    if (upd_hdr.path_attrs != NULL) {
        // Delete the attribute data from each element since we had to dynamically
        //   allocate the value size based on the length
        for (size_t i=0; i < upd_hdr.path_attrs->size(); i++) {
            if (upd_hdr.path_attrs->at(i).data != NULL)
               delete [] upd_hdr.path_attrs->at(i).data;
        }

        upd_hdr.path_attrs->clear();        // Clear all elements
        delete upd_hdr.path_attrs;          // delete the vector itself
    }

    // Free the nlri prefix vector
    if (upd_hdr.nlri_routes != NULL) {
        upd_hdr.nlri_routes->clear();       // Clear all elements
        delete upd_hdr.nlri_routes;         // delete the vector itself
    }

    // Free the withdrawn routes vector
    if (upd_hdr.withdrawn_routes != NULL) {
        upd_hdr.withdrawn_routes->clear();  // Clear all elements
        delete upd_hdr.withdrawn_routes;    // delete the vector itself
    }
}

/**
 * Handle the incoming BGP message directly from the open socket.
 *
 * \details
 *   This function will read and parse the BGP message from the socket.
 *
 * \param [in]     sock             Socket to read BGP message from
 */
void parseBGP::handleMessage(int sock) {
    // Get the common header, which is present in every BGP message
    if ( (recv(sock, &c_hdr, BGP_MSG_HDR_LEN, MSG_WAITALL)) != BGP_MSG_HDR_LEN)
       throw "ERROR: Cannot read the BGP common header from socket.";

    // Fix the length
    BGP_SWAP_BYTES(c_hdr.len);

    // Make sure we have a header length, otherwise nothing to do here
    if (c_hdr.len <= 0)
        return;

    // Update the bgp message offset counter so we know how much more we have to read
    bgp_bytes_remaining = c_hdr.len - BGP_MSG_HDR_LEN;

    SELF_DEBUG("sock=%d : BGP hdr len = %u, type = %d", c_hdr.len, c_hdr.type, sock);

    // Process the message based on the type
    switch (c_hdr.type) {
        case BGP_MSG_UPDATE : // Update Message
            parseUpdateMsg(sock);
            break;

        case BGP_MSG_NOTIFICATION: // Notification message
            parseNotifyMsg(sock);
            break;

        default :
            // Some messages we don't support, skip ahead passed this unsupported type
            char byte;
            while (bgp_bytes_remaining-- > 0)
                read(sock, &byte, 1);

            //throw "WARNING: Unsupported BGP message type";
            LOG_WARN("Unsupported BGP message type, sock=%d type=%d", sock, c_hdr.type);
            break;
    }

}

/**
 * Same as handleMessage, except it enables the notify method to update the
 *   calling memory for the table data.
 *
 * \details
 *  The notify message does not directly add to Db, so the calling
 *  method/function must handle that.
 *
 * \param [in]     sock             Socket to read BGP message from
 * \param [in,out] peer_down_entry  Updated with details from notification msg
 */
void parseBGP::handleMessage(int sock, DbInterface::tbl_peer_down_event *peer_down_entry) {
    // Set the pointer
    notify_tbl = peer_down_entry;

    // Process the BGP message normally
    handleMessage(sock);

    // Reset the pointer
    //notify_tbl = NULL;

}

/**
 * Parses the update message
 *
 * \details
 *      Reads the update message from socket and parses it.  The parsed output will
 *      be added to the DB.
 *
 * \param [in]     sock     Socket to read BGP message from
 */
void parseBGP::parseUpdateMsg(int sock) {
    char buf[4096];                         // Working buffer for conversions

    // Get the withdrawn length to see if we have any routes to withdrawn
    if ( (recv(sock, &upd_hdr.withdrawn_len, 2 /* 2 bytes */, MSG_WAITALL)) != 2)
       throw "ERROR: parseUpdateMsg: Cannot read the withdrawn routes length.";

    // convert to host byte order
    BGP_SWAP_BYTES(upd_hdr.withdrawn_len);

    // Update the remaining bytes counter
    bgp_bytes_remaining -= 2;

    // Initialize header items
    upd_hdr.nlri_routes = NULL;
    upd_hdr.withdrawn_routes = NULL;

    // ======================================
    // Process withdrawn routes if we have them
    // --------------------------------------
    if (upd_hdr.withdrawn_len > 0) {
        SELF_DEBUG("sock=%d : We have withdraw routes, length of %u", sock, upd_hdr.withdrawn_len);

        // Process the withdrawn routes
        processWithdrawnRoutes(sock);
        bgp_bytes_remaining -= upd_hdr.withdrawn_len;
    }

    // ======================================
    // Process path attributes
    // --------------------------------------
    if ( (recv(sock, &upd_hdr.pathAttr_len, 2 /* 2 bytes */, MSG_WAITALL)) != 2)
       throw "ERROR: parseUpdateMsg: Cannot read the path attributes length.";

    // Fix the byte order
    BGP_SWAP_BYTES(upd_hdr.pathAttr_len);

    // Update the remaining bytes counter
    bgp_bytes_remaining -= 2;

    // we have path attributes, lets read those
    if (upd_hdr.pathAttr_len > 0) {
        SELF_DEBUG("sock=%d : Path attributes length is %d", sock, upd_hdr.pathAttr_len);

        // Get the path attributes from message
        getPathAttr(sock);
        processPathAttrs();                 // process and add to mysql the path attributes


    // ======================================
    // Process the NLRI's
    // --------------------------------------

        // The length of the NLRI should match the bytes remaining
        unsigned int nlri_len = c_hdr.len - 23 - upd_hdr.withdrawn_len - upd_hdr.pathAttr_len;

        if (nlri_len != bgp_bytes_remaining)
            throw "ERROR: parseUpdateMsg: NLRI length doesn't match the remaining bytes in update msg.";

        SELF_DEBUG("sock=%d : BGP NLRI Length %u/%u", sock, nlri_len, bgp_bytes_remaining);

        // If we have IPv6, we will not see NLRI's
        if (nlri_len > 0) {
            processNLRI(sock, nlri_len);
        }

    } else if (upd_hdr.withdrawn_routes == NULL) {
        // error that this is an invalid BGP update message since we have no
        //   withdrawn routes and no path attributes for the required NLRI's
        while (bgp_bytes_remaining-- > 0)
            read(sock, &buf[0], 1);

    } else {
        upd_hdr.path_attrs = NULL;              // No path attributes
    }
}

/**
 * Parses a notification message
 *
 * \details
 *      Reads the notification message from the socket.  The parsed output
 *      will be added to the DB.
 *
 * \param [in]     sock     Socket to read BGP message from
 */
void parseBGP::parseNotifyMsg(int sock) {
    char buf[255] = {0};                      // Misc working buffer

    if ( (read(sock, &notify_tbl->bgp_err_code, 1)) != 1)
        throw "ERROR: Could not read the bgp error code in the notify message.";
    if ( (read(sock, &notify_tbl->bgp_err_subcode, 1)) != 1)
        throw "ERROR: Could not read the bgp sub error code in the notify message.";

    // Update the remaining bytes counter
    bgp_bytes_remaining -= 2;

    // Update the error text to be meaningful
    switch (notify_tbl->bgp_err_code) {
        case NOTIFY_MSG_HDR_ERR : {
            if (notify_tbl->bgp_err_subcode == MSG_HDR_BAD_MSG_LEN)
                snprintf(buf, sizeof(buf), "Bad message header length");
            else
                snprintf(buf, sizeof(buf), "Bad message header type");
            break;
        }

        case NOTIFY_OPEN_MSG_ERR : {
            switch (notify_tbl->bgp_err_subcode) {
                case OPEN_BAD_BGP_ID :
                    snprintf(buf, sizeof(buf), "Bad BGP ID");
                    break;
                case OPEN_BAD_PEER_AS :
                    snprintf(buf, sizeof(buf), "Incorrect peer AS");
                    break;
                case OPEN_UNACCEPTABLE_HOLD_TIME :
                    snprintf(buf, sizeof(buf), "Unacceptable hold time");
                    break;
                case OPEN_UNSUPPORTED_OPT_PARAM :
                    snprintf(buf, sizeof(buf), "Unsupported optional parameter");
                    break;
                case OPEN_UNSUPPORTED_VER :
                    snprintf(buf, sizeof(buf), "Unsupported BGP version");
                    break;
                default :
                    snprintf(buf, sizeof(buf), "Open message error - unknown subcode [%d]",
                            notify_tbl->bgp_err_subcode);
                    break;
            }
            break;
        }

        case NOTIFY_UPDATE_MSG_ERR : {
            switch (notify_tbl->bgp_err_subcode) {
                case UPDATE_ATTR_FLAGS_ERROR :
                    snprintf(buf, sizeof(buf), "Update attribute flags error");
                    break;
                case UPDATE_ATTR_LEN_ERROR :
                    snprintf(buf, sizeof(buf), "Update attribute lenght error");
                    break;
                case UPDATE_INVALID_NET_FIELD :
                    snprintf(buf, sizeof(buf), "Invalid network field");
                    break;
                case UPDATE_INVALID_NEXT_HOP_ATTR :
                    snprintf(buf, sizeof(buf), "Invalid next hop address/attribute");
                    break;
                case UPDATE_MALFORMED_AS_PATH :
                    snprintf(buf, sizeof(buf), "Malformed AS_PATH");
                    break;
                case UPDATE_MALFORMED_ATTR_LIST :
                    snprintf(buf, sizeof(buf), "Malformed attribute list");
                    break;
                case UPDATE_MISSING_WELL_KNOWN_ATTR :
                    snprintf(buf, sizeof(buf), "Missing well known attribute");
                    break;
                case UPDATE_OPT_ATTR_ERROR :
                    snprintf(buf, sizeof(buf), "Update optional attribute error");
                    break;
                case UPDATE_UNRECOGNIZED_WELL_KNOWN_ATTR :
                    snprintf(buf, sizeof(buf), "Unrecognized well known attribute");
                    break;
                default :
                    snprintf(buf, sizeof(buf), "Update message error - unknown subcode [%d]",
                             notify_tbl->bgp_err_subcode);
                    break;
            }
            break;
        }

        case NOTIFY_HOLD_TIMER_EXPIRED : {
            snprintf(buf, sizeof(buf), "Hold timer expired");
            break;
        }

        case NOTIFY_FSM_ERR : {
            snprintf(buf, sizeof(buf), "FSM error");
            break;
        }

        case NOTIFY_CEASE : {
            switch (notify_tbl->bgp_err_subcode) {
                case CEASE_MAX_PREFIXES :
                    snprintf(buf, sizeof(buf), "Maximum number of prefixes reached");
                    break;
                case CEASE_ADMIN_SHUT :
                    snprintf(buf, sizeof(buf), "Administrative shutdown");
                    break;
                case CEASE_PEER_DECONFIG :
                    snprintf(buf, sizeof(buf), "Peer de-configured");
                    break;
                case CEASE_ADMIN_RESET :
                    snprintf(buf, sizeof(buf), "Administratively reset");
                    break;
                case CEASE_CONN_REJECT :
                    snprintf(buf, sizeof(buf), "Connection rejected");
                    break;
                case CEASE_OTHER_CONFIG_CHG :
                    snprintf(buf, sizeof(buf), "Other configuration change");
                    break;
                case CEASE_CONN_COLLISION :
                    snprintf(buf, sizeof(buf), "Connection collision resolution");
                    break;
                case CEASE_OUT_OF_RESOURCES :
                    snprintf(buf, sizeof(buf), "Maximum number of prefixes reached");
                    break;
                default :
                    snprintf(buf, sizeof(buf), "Unknown cease code, subcode [%d]",
                                        notify_tbl->bgp_err_subcode);
                    break;
            }
            break;
        }

        default : {
            sprintf(buf, "Unkown notification type [%d]", notify_tbl->bgp_err_code);
            break;
        }
    }


    // Update the entry record
    strncat(notify_tbl->error_text, buf,
            sizeof(notify_tbl->error_text) - strlen(notify_tbl->error_text) - 1);

    // Skip to end of BGP message since we don't parse the variable data right now
    if (bgp_bytes_remaining > 0) {
        while (bgp_bytes_remaining-- > 0)
            read(sock, &buf[0], 1);
    }

}

/**
 * Process the Withdrawn routes field in the BGP message
 *
 * \details
 *      Will handle the withdrawn field by calling other methods to parse
 *      and add data to the DB.
 *
 * \param [in]  sock         Socket to read BGP message from
 */
void parseBGP::processWithdrawnRoutes(int sock) {
    // Allocate withdrawn routes buffer space
    upd_hdr.withdrawn_routes = new vector<prefix_2tuple>(0);

    // Read in the prefixes
    getPrefixes_v4(sock, upd_hdr.withdrawn_routes, upd_hdr.withdrawn_len);

    // Store the prefixes in MySQL
    DbInterface::tbl_rib rib_entry;
    vector<DbInterface::tbl_rib> rib_list;

    // Update prefixes in MySQL
    for (size_t i=0; i < upd_hdr.withdrawn_routes->size(); i++) {
        snprintf(rib_entry.prefix, sizeof(rib_entry.prefix), "%d.%d.%d.%d",
                upd_hdr.withdrawn_routes->at(i).prefix_v4[0],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[1],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[2],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[3]);

        // Add rib_entry to list
        // Not currently set for withdrawn routes, since it's read afterwards
        bzero(rib_entry.path_attr_hash_id, sizeof(rib_entry.path_attr_hash_id));
        memcpy(rib_entry.peer_hash_id, p_entry->hash_id, sizeof(p_entry->hash_id));
        rib_entry.prefix_len = upd_hdr.withdrawn_routes->at(i).len;

        rib_list.insert(rib_list.end(), rib_entry);

        SELF_DEBUG("sock=%d : %d: prefix %d.%d.%d.%d/%d", sock, (int)i,
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[0],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[1],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[2],
                 upd_hdr.withdrawn_routes->at(i).prefix_v4[3],
                 (int)upd_hdr.withdrawn_routes->at(i).len);
   }

    // Add to mysql
    dbi_ptr->delete_Rib(rib_list);

    // Free the rib_list
    rib_list.clear();
}

/**
 * Process the NLRI field in the BGP message
 *
 * \details
 *      Will handle the NLRI field by calling other methods to parse
 *      and add data to the DB.
 *
 * \param [in]  sock         Socket to read BGP message from
 * \param [in]  nlri_len     Length of the NRLI field
 */
void parseBGP::processNLRI(int sock, unsigned short nlri_len) {

    // Allocate path attributes buffer space
    upd_hdr.nlri_routes = new vector<prefix_2tuple>(0);

    // Read in the prefixes
    getPrefixes_v4(sock, upd_hdr.nlri_routes, nlri_len);

    // Store the prefixes in MySQL
    DbInterface::tbl_rib rib_entry;
    vector<DbInterface::tbl_rib> rib_list;

    // Loop through the vector and store each route
    for (size_t i=0; i < upd_hdr.nlri_routes->size(); i++) {
        inet_ntop(AF_INET, upd_hdr.nlri_routes->at(i).prefix_v4, rib_entry.prefix, sizeof(rib_entry.prefix));

        // Add rib_entry to list
        memcpy(rib_entry.path_attr_hash_id, path_hash_id, sizeof(path_hash_id));
        memcpy(rib_entry.peer_hash_id, p_entry->hash_id, sizeof(p_entry->hash_id));
        rib_entry.prefix_len = upd_hdr.nlri_routes->at(i).len;
        rib_entry.timestamp_secs = p_entry->timestamp_secs;

        rib_list.insert(rib_list.end(), rib_entry);

        SELF_DEBUG("sock=%d : %d: prefix %d.%d.%d.%d/%d", sock, (int)i,
                    upd_hdr.nlri_routes->at(i).prefix_v4[0],
                    upd_hdr.nlri_routes->at(i).prefix_v4[1],
                    upd_hdr.nlri_routes->at(i).prefix_v4[2],
                    upd_hdr.nlri_routes->at(i).prefix_v4[3],
                    upd_hdr.nlri_routes->at(i).len);
    }

    // Add to mysql
    dbi_ptr->add_Rib(rib_list);

    // Free the rib_list
    rib_list.clear();
}

/**
 * Process the MP_REACH NLRI field data in the BGP message
 *
 * \details
 *      Will handle processing the mp_reach nlri data by calling other methods
 *      to parse the data so it can be added to the DB.
 *
 * \param [out]  nlri         Reference to storage where the NLRI data will be added
 * \param [in]   nlri_len     Length of the NLRI data
 * \param [in]   data         RAW mp_reach NLRI data buffer
 */
void parseBGP::process_MP_REACH_NLRI(mp_reach_nlri &nlri, unsigned short nlri_len, unsigned char *data) {

    // Read address family
    memcpy(&nlri.afi, data, 2); data += 2;
    BGP_SWAP_BYTES(nlri.afi);       // fix the byte order since this is multi-byte

    // TODO : Add code to detect the SAFI type
    // read the subsequent afi
    memcpy(&nlri.safi, data++, 1);

    // read the next hop length
    memcpy(&nlri.nh_len, data++, 1);

    // Read the next_hop address
    if (nlri.nh_len <= 16)
        memcpy(nlri.next_hop, data, nlri.nh_len);

    data += nlri.nh_len;                // Move the pointer past next hop

    // read the reserved byte - save it just in case
    memcpy(&nlri.reserved, data++, 1);

    SELF_DEBUG("afi=%d safi=%d nh_len=%d reserved=%d",
                nlri.afi, nlri.safi, nlri.nh_len, nlri.reserved);
    SELF_DEBUG("next-hop address first %x end %x",
                nlri.next_hop[0], nlri.next_hop[15]);

    // Update the nlri_len since we read some data
    nlri_len -= 5 + nlri.nh_len;

    // The remaining bytes are prefixes
    if (nlri_len) {
        getPrefixes_v6(&nlri.prefixes, nlri_len, data);
    }
}

/**
 * Add MP_REACH NLRI data to the DB
 *
 * \param [out]  nlri         Reference to the NLRI data will be added to the DB
 */
void parseBGP::MP_REACH_NLRI_toDB(mp_reach_nlri &nlri) {

    // Store the prefixes in MySQL
    DbInterface::tbl_rib         rib_entry;
    vector<DbInterface::tbl_rib> rib_list;

    // Loop through the vector and store each route
    for (size_t i=0; i < nlri.prefixes.size(); i++) {
        inet_ntop(AF_INET6, nlri.prefixes[i].prefix_v6, rib_entry.prefix, sizeof(rib_entry.prefix));

        // Add rib_entry to list
        memcpy(rib_entry.path_attr_hash_id, path_hash_id, sizeof(path_hash_id));
        memcpy(rib_entry.peer_hash_id, p_entry->hash_id, sizeof(p_entry->hash_id));
        rib_entry.prefix_len = nlri.prefixes[i].len;

        rib_list.insert(rib_list.end(), rib_entry);

        SELF_DEBUG("%d: prefix %s/%d", (int)i,
                rib_entry.prefix, nlri.prefixes[i].len);
    }

    // Add to mysql
    dbi_ptr->add_Rib(rib_list);

    // Free the rib_list
    rib_list.clear();
}

/**
 * Parses the BGP attributes in the update
 *
 * \details
 *      Reads the update message from the socket.  The parsed output
 *      will be added to the DB.
 *
 * \param [in]     sock     Socket to read BGP message from
 */
void parseBGP::getPathAttr(int sock) {

    // Allocate path attributes buffer space
    upd_hdr.path_attrs = new vector<update_path_attrs>(0);

    // Read through the path attributes and update our pathAttrs vector with the parsed data
    update_path_attrs path_attr;
    path_attr.data = NULL;
    unsigned char len_char;         // Length as one byte
    unsigned short len_short = 0;       // Length as two bytes
    unsigned char len_sz;           // Length of the length field (1 or 2 bytes)

    // Parse and store all path attrs in a vector array
    for (int i=0;  i < upd_hdr.pathAttr_len; i += 2) {

        // Reset this element
        //   we leave the data allocated since the destructor takes care of deleting that
        bzero(&path_attr.flags, 1);
        path_attr.len = 0;
        path_attr.type = 0;

        // The the path attr flags
        if ( read(sock, &path_attr.flags, 1) <= 0) {
            throw "ERROR: parseUpdateMsg: Cannot read the path attribute in update message.";

        } else { // We have flags, process them
            // read the attr type
            if (read(sock, &path_attr.type, 1) <= 0)
                throw "ERROR: parseUpdateMsg: Cannot read the path attribute type";

            // Check if the length field is 1 or two bytes
            if (ATTR_FLAG_EXTENDED(path_attr.flags)) {
                SELF_DEBUG("extended length path attribute bit set for an entry");

                if (recv(sock, &len_short, 2, MSG_WAITALL) != 2)
                    throw "ERROR: parseUpdateMsg: Cannot read the path attribute length size.";

                // Fix the length
                BGP_SWAP_BYTES(len_short);
                path_attr.len = len_short;
                len_sz = 2;                         // Update the size of the length field

            } else {
                if (read(sock, &len_char, 1) != 1)
                    throw "ERROR: parseUpdateMsg: Cannot read the path attribute length size.";
                path_attr.len = len_char;
                len_sz = 1;                         // Update the size of hte length field
            }


            SELF_DEBUG("sock=%d : attribute type = %d len_sz = %d data length = %d",
                        sock, path_attr.type, len_sz, path_attr.len);

            // Get the attribute data, if we have any
            if (path_attr.len > 0) {
                // Allocate memory for the value of the attribute
                path_attr.data = new unsigned char[path_attr.len];

                // Get the value
                int read_bytes = 0;
                if ((read_bytes=recv(sock, path_attr.data, path_attr.len, MSG_WAITALL)) != path_attr.len) {
                    LOG_ERR("sock=%d : attribute type = %d len_sz = %d data length = %d, read = %d",
                        sock, path_attr.type, len_sz, path_attr.len, read_bytes);
                    throw "ERROR: parseUpdateMsg: Cannot read the attribute value";
                }
            } else
                path_attr.data = new unsigned char[1];            // we expect at least one byte


            // Insert this record into the vector array
            upd_hdr.path_attrs->insert(upd_hdr.path_attrs->end(), path_attr);

            // Update the offset for what we have read
            bgp_bytes_remaining -= 2 /* flags and type */ + len_sz + path_attr.len;

            // update the loop counter, so we don't loop more than we should
            i += len_sz + path_attr.len;
        }
    }

}

/**
 * Parses NLRI info (IPv4) from the BGP message
 *
 * \details
 *      Will get the NLRI and WithDrawn routes prefix entries
 *      from the open socket data stream.  As per RFC, this is only for
 *       v4.  V6/mpls is via mpbgp attributes (RFC4760)
 *
 * \param [in]     sock     Socket to read BGP message from
 * \param [out]    plist    pointer to prefix_2tuple vector in upd_hdr
 * \param [in]     len      length of NLRI or withdrawn routes in field
 */
void parseBGP::getPrefixes_v4(int sock, vector<prefix_2tuple> *plist, size_t len) {

    prefix_2tuple p_entry = {0};              // Prefix list entry
    int           bytes;                      // bytes to read for prefix
    unsigned char *buf;                       // buffer for prefix

    // Loop through all prefixes
    for (size_t i=0; i < len; i++) {

        // The the prefix bits
        if ( read(sock, &p_entry.len, 1) != 1) {
            throw "ERROR: getPrefixes_v4: Cannot read the prefix entry in update message.";

        } else {
            bytes = 0;
            // Determine how many bytes are used for the address
            if (p_entry.len <= 0)
                bytes = 0;
            else if (p_entry.len <= 8)
                bytes = 1;
            else if (p_entry.len > 8 && p_entry.len <= 16)
                bytes = 2;
            else if (p_entry.len > 16 && p_entry.len <= 24)
                bytes = 3;
            else
                bytes = 4;

            // if the route isn't a default route
            if (bytes > 0) {
                // allocate the buffer for the read
                buf = new unsigned char[bytes];

                // read the prefix
                if ( recv(sock, buf, bytes,MSG_WAITALL) != bytes) {
                    delete [] buf;                 // deallocate buffer
                    throw "ERROR: getPrefixes_v4: Cannot read the prefix entry in update message.";
                }

                // Adjust loop counter to indicate we read additional bytes
                i += bytes;

                // copy the buffer data to the prefix entry
                bzero(p_entry.prefix_v4, sizeof(p_entry.prefix_v4));
                memcpy(p_entry.prefix_v4, buf, bytes);

                delete [] buf;                     // deallocate buffer
            }

            // Insert this record into the vector array
            plist->insert(plist->end(), p_entry);
        }
    }

}

/**
 * Parses mp_reach_nlri and mp_unreach_nlri
 *
 * \details
 *      Will get the MPBGP NLRI and WithDrawn routes prefix entries
 *      from the a data buffer. Specifically gets IPv6 prefixes.
 *
 * \param [out]    plist    pointer to prefix_2tuple vector in upd_hdr
 * \param [in]     len      length of NLRI or withdrawn routes in field
 * \param [in]     data     for MP_REACH/MP_UNREACH NLRI
 */
void parseBGP::getPrefixes_v6(vector<prefix_2tuple> *plist, size_t len, unsigned char *data) {

    prefix_2tuple p_entry = {0};              // Prefix list entry
    int           bytes;                      // bytes to read for prefix
    unsigned char *buf;                       // buffer for prefix

    // Loop through all prefixes
    for (size_t i=0; i < len; i++) {

        // The the prefix bits
        memcpy(&p_entry.len, data, 1); data++;

        bytes = p_entry.len / 8;
        if (p_entry.len % 8)
           ++bytes;

        // if the route isn't a default route
        if (bytes > 0) {
            // allocate the buffer for the read
            buf = new unsigned char[bytes];

            // read the prefix
            memcpy(buf, data, bytes);

            // Adjust loop counter to indicate we read additional bytes
            i += bytes;
            data += bytes;              // Move the pointer

            // copy the buffer data to the prefix entry
            bzero(p_entry.prefix_v6, sizeof(p_entry.prefix_v6));
            memcpy(p_entry.prefix_v6, buf, bytes);

            delete [] buf;                     // deallocate buffer
        }

        // Insert this record into the vector array
        plist->insert(plist->end(), p_entry);
    }
}

/**
 * Process all path attributes and add them to the DB
 *
 * \details
 *      Parses and adds the path attributes to the DB.  This will call other
 *      methods to do the acutal reading/parsing.
 */
void parseBGP::processPathAttrs() {
    DbInterface::tbl_path_attr  record;                     // mysql record entry for path attributes
    unsigned char               as_cnt;                     // AS path count of as_paths in the segment
    unsigned char               seg_type;                   // AS Path segment type
    update_path_attrs           *p;                         // pointer for current path attribute
    mp_reach_nlri               mp_nlri = {0};              // MP nlri structure
    unsigned                    type;                       // Misc type variable - used by ext communities
    unsigned char               buf[24000];                 // misc buffer space for copying strings
    int                         path_len = 0;               // as_path length when working with path segments
    unsigned char               *ptr;                       // Misc pointer

    int max_blob_size = dbi_ptr->getMaxBlobSize();

    // Initialize the record
    record.as_path = new char[max_blob_size]();
    record.as_path_sz = max_blob_size;
    record.cluster_list = new char[max_blob_size]();
    record.cluster_list_sz = max_blob_size;
    record.community_list = new char[max_blob_size]();
    record.community_list_sz = max_blob_size;
    record.ext_community_list = new char[max_blob_size]();
    record.ext_community_list_sz = max_blob_size;
    record.local_pref = 0;
    record.med = 0;
    record.atomic_agg = 0;
    memcpy(record.peer_hash_id, p_entry->hash_id, sizeof(p_entry->hash_id));
    record.as_path_count = 0;
    record.origin_as = 0;

    // Loop through the path attr vector and parse the specific/supported path attributes
    for (size_t i=0; i < upd_hdr.path_attrs->size(); i++) {
        p = &upd_hdr.path_attrs->at(i);                  // Set the path attr table pointer to current record

        // each attribute type defines how to read the value.
        switch (upd_hdr.path_attrs->at(i).type) {
            case ATTR_TYPE_ORIGIN : // Origin
                switch (upd_hdr.path_attrs->at(i).data[0]) {
                   case 0 : sprintf(record.origin, "igp"); break;
                   case 1 : sprintf(record.origin, "egp"); break;
                   case 2 : sprintf(record.origin, "incomplete"); break;
                }

                break;

            case ATTR_TYPE_AS_PATH : // AS_PATH
                // multiple segments, process each segment
                // Initialize the loop variables and path data
                path_len = p->len;
                ptr = p->data;
                record.as_path[0] = 0;

                while (path_len > 0) {

                   seg_type = *ptr++;
                   as_cnt = *ptr++;
                   path_len -= 2;


                   // Indicate if path is seq or set
                   if (seg_type == 1) { // If AS-SET open with a brace
                       strncpy((char *)buf, record.as_path, sizeof(buf));
                       sprintf(record.as_path, "%s {", buf);
                   }

                   /*
                    *    to determine which is being used, we use the following
                    *    formula:
                    * A BGP speaker can use 4 octet or 2 octet (RFC4893) ASN's.   In order
                    *        <as_paths_data_size / as_cnt> = AS_LENGTH
                    */
                    // Play it safe and ensure we either have 2 or 4
                    if (as_cnt <= 0) {        // don't proceed if no ASN's in the list
                        path_len = 0;
                        break;
                    }

                    else if ( peer_asn_len == 2 ||  		// 2 octet ASN
                              (4 * as_cnt) > path_len) {        // 2 octet due to 4 byte ASN being too large

                        if (peer_asn_len == 4)
                           LOG_WARN("peer_asn is 4, but AS_PATH is using 2 byte ASN");

                        // The rest of the data is the as path sequence, in blocks of 2
                        for (int i2=0; i2 < as_cnt * 2; i2 += 2) {
                            strncpy((char *)buf, (char *)record.as_path, sizeof(buf));
                            // Build the as_path human readable string
                            snprintf(record.as_path, max_blob_size, "%s %u", buf,
                                    ptr[i2] << 8 | ptr[i2+1]);

                            // increase the as length counter
                            ++record.as_path_count;
                        }

                        // Indicate we read this segment
                        path_len -= as_cnt * 2;
                        ptr += as_cnt * 2;              // Move the current position

                    } else { // 4 octet ASN
                        peer_asn_len = 4;

                        // The rest of the data is the as path sequence, in blocks of 2 or 4 bytes
                        for (int i2=0; i2 < as_cnt * 4; i2 += 4) {
                           // Build the as_path human readable string
                           strncpy((char *)buf, record.as_path, sizeof(buf));
                           snprintf((char *)record.as_path, max_blob_size, "%s %u", buf,
                                 (unsigned int)(ptr[i2] << 24 | ptr[i2+1] << 16 |
                                 ptr[i2+2] << 8 | ptr[i2+3]));

                           // increase the as length counter
                           ++record.as_path_count;
                        }

                        // Indicate we read this segment
                        path_len -= as_cnt * 4;
                        ptr += as_cnt * 4;               // Move the current position
                    }

                    // Close as path set segment
                    if (seg_type == 1) { // If AS-SET close with a brace
                        strncpy((char *)buf, (char *)record.as_path, sizeof(buf));
                        sprintf((char *)record.as_path, "%s }", buf);
                    }

                    SELF_DEBUG("SEG_TYPE=%d path_lenghts=%d/%d ASN Length is %d (%s)",
                                 seg_type, p->len, path_len, peer_asn_len, record.as_path);

                }

                // Get the last ASN and record it as the Origin AS
                //   This can be the ASN that is in an AS-SET - for example if it's last
                {
                    long as_path_sz = strlen((const char *)record.as_path);
                    char *as_path = record.as_path + as_path_sz;
                    bool found_digit = 0;

                    // Make sure we have something
                    if (as_path != NULL && as_path_sz > 0) {
                        bool done = 0;

                        // walk backwards from end of string to get the last ASN
                        while (as_path_sz-- > 0 && !done) {


                            // Last char might not be the ASN, so keep checking
                            if (*as_path >= '0' && *as_path <= '9')
                                found_digit = 1;

                            // Else we already found the digit and the current value isn't another digit
                            else if (found_digit)
                                done = 1;           // Indicate to stop the loop, we are done

                            if (!done)
                                --as_path;              // Move back by one char
                        }
                    }

                    // If if found a digit, then we should have a last ASN available
                    if (found_digit) {
                        // Record the Origin ASN
                        record.origin_as = strtoll(as_path, NULL, 10);
                    }
                }

                break;

            case ATTR_TYPE_NEXT_HOP : // Next hop v4
                record.nexthop_isIPv4 = true;
                sprintf(record.next_hop, "%d.%d.%d.%d", p->data[0], p->data[1],
                        p->data[2], p->data[3]);

                break;

            case ATTR_TYPE_MED : // MED value
                reverseBytes(p->data, 4);               // convert to host byte order
                memcpy((void *)&record.med, (void *)p->data, 4);

                break;

            case ATTR_TYPE_LOCAL_PREF : // local pref value
                reverseBytes(p->data, 4);               // convert to host byte order
                memcpy((void *)&record.local_pref, (void *)p->data, 4);
                break;

            case ATTR_TYPE_ATOMIC_AGGREGATE : // Atomic aggregate
                record.atomic_agg = 1;
                break;

            case ATTR_TYPE_AGGEGATOR : // Aggregator
                // If using RFC4893, the len will be 8 instead of 6
                if (p->len == 8) { // RFC4893 ASN of 4 octets
                    sprintf(record.aggregator, "%u %d.%d.%d.%d",
                            (unsigned int)(p->data[0] << 24 | p->data[1] << 16 | p->data[2] << 8 | p->data[3]),
                            p->data[4], p->data[5], p->data[6], p->data[7]);
                } else if (p->len == 6) {
                    sprintf(record.aggregator, "%u %d.%d.%d.%d",
                            p->data[0] << 8 | p->data[1],
                            p->data[2], p->data[3], p->data[4], p->data[5]);

                } else {
                    LOG_ERR("path attribute is not the correct size of 6 or 8 octets.");
                }
                break;

            case ATTR_TYPE_ORIGINATOR_ID : // Originator ID
                sprintf((char *)record.originator_id, "%d.%d.%d.%d", p->data[0], p->data[1],
                        p->data[2], p->data[3]);
                break;

            case ATTR_TYPE_CLUSTER_LIST : // Cluster List (RFC 4456)
                // According to RFC 4456, the value is a sequence of cluster id's
                for (int i2=0; i2 < p->len; i2 += 4) {
                    // Build the as_path human readable string
                    strncpy((char *)buf, record.cluster_list, sizeof(buf));
                    snprintf(record.cluster_list, max_blob_size, "%s %d.%d.%d.%d", buf,
                            p->data[i2], p->data[i2+1], p->data[i2+2], p->data[i2+3]);
                }

                break;

            case ATTR_TYPE_COMMUNITIES : // Community list
                for (int i2=0; i2 < p->len; i2 += 4) {
                     // Build the as_path human readable string
                    strncpy((char *)buf, (char *)record.community_list, sizeof(buf));
                     snprintf((char *)record.community_list, max_blob_size, "%s %u:%u", buf,
                             p->data[i2+0] << 8 | p->data[i2+1], p->data[i2+2] << 8 | p->data[i2+3]);
                 }

                break;


            case ATTR_TYPE_EXT_COMMUNITY : // extended community list (RFC 4360)
                type = p->data[0];          // Type high
                /*
                 * Two classes of Type Field are introduced: Regular type and Extended type.
                 *  The size of Type Field for Regular types is 1 octet, and the size of the
                 *  Type Field for Extended types is 2 octets.
                 */
                // TODO: Need to add code to parse the extended communities, which requires
                //       mapping the IANA assigned types so we know how to read the value.

                break;

            case ATTR_TYPE_MP_REACH_NLRI :  // RFC4760
                process_MP_REACH_NLRI(mp_nlri, p->len, p->data);

                // Update the v6 next hop in the path attribute
                record.nexthop_isIPv4 = false;

                inet_ntop(AF_INET6, (char *)mp_nlri.next_hop, record.next_hop, sizeof(record.next_hop));
                break;


        } // END OF SWITCH ATTR TYPE

        SELF_DEBUG("%d: attr type=%d, size=%u", (int)i,
                p->type, (unsigned int)p->len);
    }

    SELF_DEBUG("adding attributes to DB");

    // Done processing the attributes, add the final record to mysql
    //   the mysql add method updates the hash for this record
    dbi_ptr->add_PathAttrs(record);

    // Update the current path hash ID
    memcpy(path_hash_id, record.hash_id, 16);

    // If we have MP_NLRI's add those to DB
    if ((mp_nlri.safi == 1 || mp_nlri.safi == 2) && mp_nlri.prefixes.size() > 0)
        MP_REACH_NLRI_toDB(mp_nlri);

    SELF_DEBUG("finished adding attributes to DB");


    // Free the record now that we are done with it.
    delete [] record.as_path;
    delete [] record.cluster_list;
    delete [] record.community_list;
    delete [] record.ext_community_list;
}

void parseBGP::enableDebug() {
    debug = true;
}

void parseBGP::disableDebug() {
    debug = false;
}
