/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "parseBMP.h"
#include "DbInterface.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/**
 * Constructor for class
 *
 * \note
 *  This class will allocate via 'new' the bgp_peers variables
 *        as needed.  The calling method/class/function should check each var
 *        in the structure for non-NULL pointers.  Non-NULL pointers need to be
 *        freed with 'delete'
 *
 * \param [in]     logPtr      Pointer to existing Logger for app logging
 * \param [in,out] peer_entry  Pointer to the peer entry
 */
parseBMP::parseBMP(Logger *logPtr, DbInterface::tbl_bgp_peer *peer_entry) {
    debug = false;
    bmp_type = -1; // Initially set to error
    log = logPtr;

    // Set the passed storage for the router entry items.
    p_entry = peer_entry;
    bzero(p_entry, sizeof(DbInterface::tbl_bgp_peer));
}

parseBMP::~parseBMP() {
    // clean up
}

/**
 * Process the incoming BMP message
 *
 * \returns
 *      returns the BMP message type. A type of >= 0 is normal,
 *      < 0 indicates an error
 *
 * \param [in] sock     Socket to read the BMP message from
 *
 * \throws (const char *) on error.   String will detail error message.
 */
char parseBMP::handleMessage(int sock) {
    unsigned char ver;
    ssize_t bytes_read;

    // Get the version in order to determine what we read next
    //    As of Junos 10.4R6.5, it supports version 1
    bytes_read = read(sock, &ver, 1);

    if (bytes_read < 0)
        throw "1: Failed to read from socket.";
    else if (bytes_read == 0)
        throw "2: Connection closed";
    else if (bytes_read != 1)
        throw "3: Cannot read the BMP version byte from socket";

    // check the version
    if (ver == 3) { // draft-ietf-grow-bmp-04 - 07
        parseBMPv3(sock);

        // Handle the older versions
    } else if (ver == 1 || ver == 2) {
        parseBMPv2(sock);

    } else
        throw "ERROR: Unsupported BMP message version";

    SELF_DEBUG("BMP version = %d\n", ver);

    return bmp_type;
}

/**
 * Parse v3 BMP header
 *
 * \details
 *      v3 has a different header structure and changes the peer
 *      header format.
 *
 * \param [in]  sock        Socket to read the message from
 */
void parseBMP::parseBMPv3(int sock) {
    struct common_hdr_v3 c_hdr = { 0 };

    SELF_DEBUG("Parsing BMP version 3 (latest draft)");
    if ((recv(sock, &c_hdr, BMP_HDRv3_LEN, MSG_WAITALL)) != BMP_HDRv3_LEN) {
        throw "ERROR: Cannot read v3 BMP common header.";
    }

    reverseBytes((unsigned char *) &c_hdr.len, sizeof(c_hdr.len));

    SELF_DEBUG("BMP v3: type = %x len=%d", c_hdr.type, c_hdr.len);

    // Adjust length to remove common header size
    c_hdr.len -= 1 + BMP_HDRv3_LEN;

    // Process the message based on type
    bmp_type = c_hdr.type;
    switch (c_hdr.type) {
    case TYPE_ROUTE_MON: // Route monitoring
        SELF_DEBUG("BMP MSG : route monitor");
        parsePeerHdr(sock);
        break;

    case TYPE_STATS_REPORT: // Statistics Report
        SELF_DEBUG("BMP MSG : stats report");
        parsePeerHdr(sock);
        break;

    case TYPE_PEER_UP: // Peer Up notification
    {
        //TODO: implement
        SELF_DEBUG("BMP MSG : peer up");
        parsePeerHdr(sock);
        LOG_INFO("PEER_UP message, not yet implemented -- ignoring for now.");
        unsigned char *buf = new unsigned char[c_hdr.len];
        recv(sock, buf, c_hdr.len - BMP_PEER_HDR_LEN, MSG_WAITALL);
        delete[] buf;
        break;
    }
    case TYPE_PEER_DOWN: // Peer down notification
        SELF_DEBUG("BMP MSG : peer down");
        parsePeerHdr(sock);
        break;

    case TYPE_INIT_MSG: // BMP initial message
    {
        //TODO: implement
        SELF_DEBUG("BMP MSG : init message");
        LOG_INFO("INIT message, not yet implemented -- ignoring for now.");
        unsigned char *buf = new unsigned char[c_hdr.len];
        recv(sock, buf, c_hdr.len, MSG_WAITALL);
        delete[] buf;
        break;
    }
    case TYPE_TERMINATION: // BMP termination message
    {
        //TODO: implement
        SELF_DEBUG("BMP MSG : termination message\n");
        LOG_INFO("TERM message, not yet implemented -- ignoring for now.");
        unsigned char *buf = new unsigned char[c_hdr.len];
        recv(sock, buf, c_hdr.len, MSG_WAITALL);
        delete[] buf;
        break;
    }
    default:
        SELF_DEBUG("ERROR: Unknown BMP message type of %d", c_hdr.type);
        break;
    }
}

/**
 * Parse v1 and v2 BMP header
 *
 * \details
 *      v2 uses the same common header, but adds the Peer Up message type.
 *
 * \param [in]  sock        Socket to read the message from
 */
void parseBMP::parseBMPv2(int sock) {
    struct common_hdr_old c_hdr = { 0 };
    ssize_t i = 0;

    SELF_DEBUG("parseBMP: sock=%d: Reading %d bytes", sock, BMP_HDRv1v2_LEN);

    if ((i = recv(sock, &c_hdr, BMP_HDRv1v2_LEN, MSG_WAITALL))
            != BMP_HDRv1v2_LEN) {
        SELF_DEBUG("sock=%d: Couldn't read all bytes, read %zd bytes",
                    sock, i);
        throw "ERROR: Cannot read v1/v2 BMP common header.";
    }
    // Process the message based on type
    bmp_type = c_hdr.type;
    switch (c_hdr.type) {
    case 0: // Route monitoring
        SELF_DEBUG("sock=%d : BMP MSG : route monitor", sock);
        break;

    case 1: // Statistics Report
        SELF_DEBUG("sock=%d : BMP MSG : stats report", sock);
        break;

    case 2: // Peer down notification
        SELF_DEBUG("sock=%d : BMP MSG : peer down", sock);
        break;
    case 3: // Peer Up notification
        SELF_DEBUG("sock=%d : BMP MSG : peer up", sock);
        break;
    }

    SELF_DEBUG("sock=%d : Peer Type is %d", sock, c_hdr.peer_type);

    if (c_hdr.peer_flags & 0x80) { // V flag of 1 means this is IPv6
        p_entry->isIPv4 = false;
        inet_ntop(AF_INET6, c_hdr.peer_addr, peer_addr, sizeof(peer_addr));

        SELF_DEBUG("sock=%d : Peer address is IPv6", sock);

    } else {
        p_entry->isIPv4 = true;
        snprintf(peer_addr, sizeof(peer_addr), "%d.%d.%d.%d",
                c_hdr.peer_addr[12], c_hdr.peer_addr[13], c_hdr.peer_addr[14],
                c_hdr.peer_addr[15]);

        SELF_DEBUG("sock=%d : Peer address is IPv4", sock);
    }

    if (c_hdr.peer_flags & 0x40) { // L flag of 1 means this is Loc-RIP and not Adj-RIB-In
        SELF_DEBUG("sock=%d : Msg is for Loc-RIB", sock);
    } else {
        SELF_DEBUG("sock=%d : Msg is for Adj-RIB-In", sock);
    }

    // convert the BMP byte messages to human readable strings
    snprintf(peer_as, sizeof(peer_as), "0x%04x%04x",
            c_hdr.peer_as[0] << 8 | c_hdr.peer_as[1],
            c_hdr.peer_as[2] << 8 | c_hdr.peer_as[3]);
    snprintf(peer_bgp_id, sizeof(peer_bgp_id), "%d.%d.%d.%d",
            c_hdr.peer_bgp_id[0], c_hdr.peer_bgp_id[1], c_hdr.peer_bgp_id[2],
            c_hdr.peer_bgp_id[3]);

    // Format based on the type of RD
    switch (c_hdr.peer_dist_id[0] << 8 | c_hdr.peer_dist_id[1]) {
    case 1: // admin = 4bytes (IP address), assign number = 2bytes
        snprintf(peer_rd, sizeof(peer_rd), "%d.%d.%d.%d:%d",
                c_hdr.peer_dist_id[2], c_hdr.peer_dist_id[3],
                c_hdr.peer_dist_id[4], c_hdr.peer_dist_id[5],
                c_hdr.peer_dist_id[6] << 8 | c_hdr.peer_dist_id[7]);
        break;
    case 2: // admin = 4bytes (ASN), assing number = 2bytes
        snprintf(peer_rd, sizeof(peer_rd), "%lu:%d",
                (unsigned long) (c_hdr.peer_dist_id[2] << 24
                        | c_hdr.peer_dist_id[3] << 16
                        | c_hdr.peer_dist_id[4] << 8 | c_hdr.peer_dist_id[5]),
                c_hdr.peer_dist_id[6] << 8 | c_hdr.peer_dist_id[7]);
        break;
    default: // admin = 2 bytes, sub field = 4 bytes
        snprintf(peer_rd, sizeof(peer_rd), "%d:%lu",
                c_hdr.peer_dist_id[1] << 8 | c_hdr.peer_dist_id[2],
                (unsigned long) (c_hdr.peer_dist_id[3] << 24
                        | c_hdr.peer_dist_id[4] << 16
                        | c_hdr.peer_dist_id[5] << 8 | c_hdr.peer_dist_id[6]
                        | c_hdr.peer_dist_id[7]));
        break;
    }

    // Update the MySQL peer entry struct
    strncpy(p_entry->peer_addr, peer_addr, sizeof(peer_addr));
    p_entry->peer_as = strtoll(peer_as, NULL, 16);
    strncpy(p_entry->peer_bgp_id, peer_bgp_id, sizeof(peer_bgp_id));
    strncpy(p_entry->peer_rd, peer_rd, sizeof(peer_rd));

    // Save the advertised timestamp
    uint32_t ts = c_hdr.ts_secs;
    reverseBytes((unsigned char *) &ts, sizeof(ts));
    if (ts != 0)
        p_entry->timestamp_secs = ts;
    else
        p_entry->timestamp_secs = time(NULL);

    // Is peer type L3VPN peer or global instance
    if (c_hdr.type == 1) // L3VPN
        p_entry->isL3VPN = 1;
    else
        // Global Instance
        p_entry->isL3VPN = 0;

    SELF_DEBUG("sock=%d : Peer Address = %s", sock, peer_addr);
    SELF_DEBUG("sock=%d : Peer AS = (%x-%x)%x:%x", sock,
                c_hdr.peer_as[0], c_hdr.peer_as[1], c_hdr.peer_as[2],
                c_hdr.peer_as[3]);
    SELF_DEBUG("sock=%d : Peer RD = %s", sock, peer_rd);
}

/**
 * Parse the v3 peer header
 *
 * \param [in]  sock        Socket to read the message from
 */
void parseBMP::parsePeerHdr(int sock) {
    peer_hdr_v3 p_hdr = { 0 };
    int i;

    if ((i = recv(sock, &p_hdr, BMP_PEER_HDR_LEN, MSG_WAITALL))
            != BMP_PEER_HDR_LEN) {
        LOG_ERR("sock=%d: Couldn't read all bytes, read %d bytes",
                sock, i);
    }

    SELF_DEBUG("parsePeerHdr: sock=%d : Peer Type is %d", sock,
            p_hdr.peer_type);

    if (p_hdr.peer_flags & 0x80) { // V flag of 1 means this is IPv6
        p_entry->isIPv4 = false;

        inet_ntop(AF_INET6, p_hdr.peer_addr, peer_addr, sizeof(peer_addr));

        SELF_DEBUG("sock=%d : Peer address is IPv6 %s", sock,
                peer_addr);

    } else {
        p_entry->isIPv4 = true;

        snprintf(peer_addr, sizeof(peer_addr), "%d.%d.%d.%d",
                p_hdr.peer_addr[12], p_hdr.peer_addr[13], p_hdr.peer_addr[14],
                p_hdr.peer_addr[15]);
        SELF_DEBUG("sock=%d : Peer address is IPv4 %s", sock,
                peer_addr);
    }

    if (p_hdr.peer_flags & 0x40) { // L flag of 1 means this is post-policy of Adj-RIB-In
        SELF_DEBUG("sock=%d : Msg is for POST-POLICY Adj-RIB-In", sock);
        p_entry->isPrePolicy = false;

    } else {
        SELF_DEBUG("sock=%d : Msg is for PRE-POLICY Adj-RIB-In", sock);
        p_entry->isPrePolicy = true;
    }

    // convert the BMP byte messages to human readable strings
    snprintf(peer_as, sizeof(peer_as), "0x%04x%04x",
            p_hdr.peer_as[0] << 8 | p_hdr.peer_as[1],
            p_hdr.peer_as[2] << 8 | p_hdr.peer_as[3]);

    inet_ntop(AF_INET, p_hdr.peer_bgp_id, peer_bgp_id, sizeof(peer_bgp_id));

    // Format based on the type of RD
    switch (p_hdr.peer_dist_id[0] << 8 | p_hdr.peer_dist_id[1]) {
    case 1: // admin = 4bytes (IP address), assign number = 2bytes
        snprintf(peer_rd, sizeof(peer_rd), "%d.%d.%d.%d:%d",
                p_hdr.peer_dist_id[2], p_hdr.peer_dist_id[3],
                p_hdr.peer_dist_id[4], p_hdr.peer_dist_id[5],
                p_hdr.peer_dist_id[6] << 8 | p_hdr.peer_dist_id[7]);
        break;

    case 2: // admin = 4bytes (ASN), sub field 2bytes
        snprintf(peer_rd, sizeof(peer_rd), "%lu:%d",
                (unsigned long) (p_hdr.peer_dist_id[2] << 24
                        | p_hdr.peer_dist_id[3] << 16
                        | p_hdr.peer_dist_id[4] << 8 | p_hdr.peer_dist_id[5]),
                p_hdr.peer_dist_id[6] << 8 | p_hdr.peer_dist_id[7]);
        break;
    default: // admin = 2 bytes, sub field = 4 bytes
        snprintf(peer_rd, sizeof(peer_rd), "%d:%lu",
                p_hdr.peer_dist_id[1] << 8 | p_hdr.peer_dist_id[2],
                (unsigned long) (p_hdr.peer_dist_id[3] << 24
                        | p_hdr.peer_dist_id[4] << 16
                        | p_hdr.peer_dist_id[5] << 8 | p_hdr.peer_dist_id[6]
                        | p_hdr.peer_dist_id[7]));
        break;
    }

    // Update the DB peer entry struct
    strncpy(p_entry->peer_addr, peer_addr, sizeof(peer_addr));
    p_entry->peer_as = strtoll(peer_as, NULL, 16);
    strncpy(p_entry->peer_bgp_id, peer_bgp_id, sizeof(peer_bgp_id));
    strncpy(p_entry->peer_rd, peer_rd, sizeof(peer_rd));

    // Save the advertised timestamp
    reverseBytes((unsigned char *) &p_hdr.ts_secs, sizeof(p_hdr.ts_secs));

    if (p_hdr.ts_secs != 0)
        p_entry->timestamp_secs = p_hdr.ts_secs;
    else
        p_entry->timestamp_secs = time(NULL);

    // Is peer type L3VPN peer or global instance
    if (p_hdr.peer_type == 1) // L3VPN
        p_entry->isL3VPN = 1;

    else
        // Global Instance
        p_entry->isL3VPN = 0;

    SELF_DEBUG("sock=%d : Peer Address = %s", sock, peer_addr);
    SELF_DEBUG("sock=%d : Peer AS = (%x-%x)%x:%x", sock,
                p_hdr.peer_as[0], p_hdr.peer_as[1], p_hdr.peer_as[2],
                p_hdr.peer_as[3]);
    SELF_DEBUG("sock=%d : Peer RD = %s", sock, peer_rd);
}

/**
 * Handle the stats reports and add to DB
 *
 * \param [in]  dbi_ptr     Pointer to exiting dB implementation
 * \param [in]  sock        Socket to read the stats message from
 */
void parseBMP::handleStatsReport(DbInterface *dbi_ptr, int sock) {
    unsigned long stats_cnt = 0; // Number of counter stat objects to follow
    unsigned char b[4];

    if ((recv(sock, b, 4, MSG_WAITALL)) != 4)
        throw "ERROR:  Cannot proceed since we cannot read the stats mon counter";

    // Reverse the bytes and update int
    reverseBytes(b, 4);
    memcpy((void*) &stats_cnt, (void*) b, 4);

    SELF_DEBUG("sock = %d : STATS REPORT Count: %lu (%d %d %d %d)",
                sock, stats_cnt, b[0], b[1], b[2], b[3]);

    // Vars used per counter object
    unsigned short stat_type = 0;
    unsigned short stat_len = 0;
    DbInterface::tbl_stats_report stats = { 0 };
    memcpy(stats.peer_hash_id, p_entry->hash_id, sizeof(p_entry->hash_id));

    // Loop through each stats object
    for (unsigned long i = 0; i < stats_cnt; i++) {
        if ((recv(sock, &stat_type, 2, MSG_WAITALL)) != 2)
            throw "ERROR: Cannot proceed since we cannot read the stats type.";
        if ((recv(sock, &stat_len, 2, MSG_WAITALL)) != 2)
            throw "ERROR: Cannot proceed since we cannot read the stats len.";

        // convert integer from network to host bytes
        reverseBytes((unsigned char *) &stat_type, 2);
        reverseBytes((unsigned char *) &stat_len, 2);

        SELF_DEBUG("sock=%d STATS: %lu : TYPE = %u LEN = %u", sock,
                    i, stat_type, stat_len);

        // check if this is a 32 bit number  (default)
        if (stat_len == 4 or stat_len == 8) {

            // Read the stats counter - 32/64 bits
            if ((recv(sock, b, stat_len, MSG_WAITALL)) == stat_len) {
                // convert the bytes from network to host order
                reverseBytes(b, stat_len);

                // Update the table structure based on the stats counter type
                switch (stat_type) {
                case STATS_PREFIX_REJ:
                    memcpy((void*) &stats.prefixes_rej, (void*) b, stat_len);
                    break;
                case STATS_DUP_PREFIX:
                    memcpy((void*) &stats.known_dup_prefixes, (void*) b, stat_len);
                    break;
                case STATS_DUP_WITHDRAW:
                    memcpy((void*) &stats.known_dup_withdraws, (void*) b, stat_len);
                    break;
                case STATS_INVALID_CLUSTER_LIST:
                    memcpy((void*) &stats.invalid_cluster_list, (void*) b, stat_len);
                    break;
                case STATS_INVALID_AS_PATH_LOOP:
                    memcpy((void*) &stats.invalid_as_path_loop, (void*) b, stat_len);
                    break;
                case STATS_INVALID_ORIGINATOR_ID:
                    memcpy((void*) &stats.invalid_originator_id, (void*) b, stat_len);
                    break;
                case STATS_INVALID_AS_CONFED_LOOP:
                    memcpy((void*) &stats.invalid_as_confed_loop, (void*) b, stat_len);
                    break;
                case STATS_NUM_ROUTES_ADJ_RIB_IN:
                    memcpy((void*) &stats.routes_adj_rib_in, (void*) b, stat_len);
                    break;
                case STATS_NUM_ROUTES_LOC_RIB:
                    memcpy((void*) &stats.routes_loc_rib, (void*) b, stat_len);
                    break;
                }

                SELF_DEBUG("VALUE is %u",
                            b[3] << 24 | b[2] << 16 | b[1] << 8 | b[0]);
            }

        } else { // stats len not expected, we need to skip it.
            SELF_DEBUG("sock=%d : skipping stats report '%u' because length of '%u' is not expected.",
                        sock, stat_type, stat_len);
            while (stat_len-- > 0)
                read(sock, &b[0], 1);
        }
    }

    // Add to mysql
    dbi_ptr->add_StatReport(stats);
}


/**
 * Enable/Disable debug
 */
void parseBMP::enableDebug() {
    debug = true;
}
void parseBMP::disableDebug() {
    debug = false;
}

