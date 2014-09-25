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
#include <unistd.h>
#include <sys/socket.h>

#include <cstring>
#include <string>
#include <list>
#include <arpa/inet.h>

#include "DbInterface.hpp"
#include "NotificationMsg.h"
#include "OpenMsg.h"
#include "UpdateMsg.h"
#include "bgp_common.h"

using namespace std;

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
 * \param [in]     routerAddr  The router IP address - used for logging
 */
parseBGP::parseBGP(Logger *logPtr, DbInterface *dbi_ptr, DbInterface::tbl_bgp_peer *peer_entry, string routerAddr) {
    debug = false;

    logger = logPtr;

    data_bytes_remaining = 0;
    data = NULL;

    bzero(&common_hdr, sizeof(common_hdr));

    // Set our mysql pointer
    this->dbi_ptr = dbi_ptr;

    // Set our peer entry
    p_entry = peer_entry;

    // Set the default ASN size
    peer_asn_len = 4;

    router_addr = routerAddr;
}

/**
 * Desctructor
 */
parseBGP::~parseBGP() {
}

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
bool parseBGP::handleUpdate(u_char *data, size_t size) {
    bgp_msg::UpdateMsg::parsed_udpate_data parsed_data;
    int read_size = 0;

    if (parseBgpHeader(data, size) == BGP_MSG_UPDATE) {
        data += BGP_MSG_HDR_LEN;

        /*
         * Parse the update message - stored results will be in parsed_data
         */
        bgp_msg::UpdateMsg uMsg(logger, p_entry->peer_addr, router_addr, debug);
        if ((read_size=uMsg.parseUpdateMsg(data, data_bytes_remaining, parsed_data)) != (size - BGP_MSG_HDR_LEN)) {
            LOG_NOTICE("%s: rtr=%s: Failed to parse the update message, read %d expected %d", p_entry->peer_addr,
                        router_addr.c_str(), read_size, (size - read_size));
            return true;
        }

        data_bytes_remaining -= read_size;

        /*
         * Update the DB with the update data
         */
        UpdateDB(parsed_data);
    }

    return false;
}

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
bool parseBGP::handleDownEvent(u_char *data, size_t size, DbInterface::tbl_peer_down_event &down_event) {
    bool        rval;

    // Process the BGP message normally
    if (parseBgpHeader(data, size) == BGP_MSG_NOTIFICATION) {
        data += BGP_MSG_HDR_LEN;

        bgp_msg::parsed_notify_msg parsed_msg;
        bgp_msg::NotificationMsg nMsg(logger, debug);
        if ( (rval=nMsg.parseNotify(data, data_bytes_remaining, parsed_msg)))
            LOG_ERR("%s: rtr=%s: Failed to parse the BGP notification message", p_entry->peer_addr, router_addr.c_str());

        else {
            data += 2;                                                 // Move pointer past notification message
            data_bytes_remaining -= 2;

            down_event.bgp_err_code = parsed_msg.error_code;
            down_event.bgp_err_subcode = parsed_msg.error_subcode;
            strncpy(down_event.error_text, parsed_msg.error_text, sizeof(down_event.error_text));
        }
    }
    else {
        LOG_ERR("%s: rtr=%s: BGP message type is not a BGP notification, cannot parse the notification",
                p_entry->peer_addr, router_addr.c_str());
        throw "ERROR: Invalid BGP MSG for BMP down event, expected NOTIFICATION message.";
    }

    return rval;
}

/**
 * Handles the up event by parsing the BGP open messages - Up event will be updated
 *
 * \details
 *  This method will read the expected sent and receive open messages.
 *
 * \param [in]     data             Pointer to the raw BGP message header
 * \param [in]     size             length of the data buffer (used to prevent overrun)
 * \param [in,out] peer_up_event    Updated with details from the peer up message (sent/recv open msg)
 *
 * \returns True if error, false if no error.
 */
bool parseBGP::handleUpEvent(u_char *data, size_t size, DbInterface::tbl_peer_up_event *up_event) {
    bgp_msg::OpenMsg    oMsg(logger, p_entry->peer_addr, debug);
    list <string>       cap_list;
    string              local_bgp_id, remote_bgp_id;
    size_t              read_size;

    /*
     * Process the sent open message
     */
    if (parseBgpHeader(data, size) == BGP_MSG_OPEN) {
        data += BGP_MSG_HDR_LEN;

        read_size = oMsg.parseOpenMsg(data, data_bytes_remaining, up_event->local_asn, up_event->local_hold_time, local_bgp_id, cap_list);

        if (!read_size) {
            LOG_ERR("%s: rtr=%s: Failed to read sent open message",  p_entry->peer_addr, router_addr.c_str());
            throw "Failed to read open message";
        }

        data += read_size;                                          // Move the pointer pase the sent open message
        data_bytes_remaining -= read_size;

        strncpy(up_event->local_bgp_id, local_bgp_id.c_str(), sizeof(up_event->local_bgp_id));

        // Convert the list to string
        bzero(up_event->sent_cap, sizeof(up_event->sent_cap));

        string cap_str;
        for (list<string>::iterator it = cap_list.begin(); it != cap_list.end(); it++) {
            if ( it != cap_list.begin())
                cap_str.append(", ");

            cap_str.append((*it));
        }

        memcpy(up_event->sent_cap, cap_str.c_str(), sizeof(up_event->sent_cap));

    } else {
        LOG_ERR("%s: rtr=%s: BGP message type is not BGP OPEN, cannot parse the open message",  p_entry->peer_addr, router_addr.c_str());
        throw "ERROR: Invalid BGP MSG for BMP Sent OPEN message, expected OPEN message.";
    }

    /*
     * Process the received open message
     */
    cap_list.clear();

    if (parseBgpHeader(data, size) == BGP_MSG_OPEN) {
        data += BGP_MSG_HDR_LEN;

        read_size = oMsg.parseOpenMsg(data, data_bytes_remaining, up_event->remote_asn, up_event->remote_hold_time, remote_bgp_id, cap_list);

        if (!read_size) {
            LOG_ERR("%s: rtr=%s: Failed to read sent open message", p_entry->peer_addr, router_addr.c_str());
            throw "Failed to read open message";
        }

        data += read_size;                                          // Move the pointer pase the sent open message
        data_bytes_remaining -= read_size;

        strncpy(up_event->remote_bgp_id, remote_bgp_id.c_str(), sizeof(up_event->remote_bgp_id));

        // Convert the list to string
        bzero(up_event->recv_cap, sizeof(up_event->recv_cap));

        string cap_str;
        for (list<string>::iterator it = cap_list.begin(); it != cap_list.end(); it++) {
            if ( it != cap_list.begin())
                cap_str.append(", ");

            cap_str.append((*it));
        }

        memcpy(up_event->recv_cap, cap_str.c_str(), sizeof(up_event->recv_cap));

    } else {
        LOG_ERR("%s: rtr=%s: BGP message type is not BGP OPEN, cannot parse the open message",
                p_entry->peer_addr, router_addr.c_str());
        throw "ERROR: Invalid BGP MSG for BMP Received OPEN message, expected OPEN message.";
    }

    return false;
}

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
u_char parseBGP::parseBgpHeader(u_char *data, size_t size) {
    bzero(&common_hdr, sizeof(common_hdr));

    /*
     * Error out if data size is not large enough for common header
     */
    if (size < BGP_MSG_HDR_LEN) {
        LOG_WARN("%s: rtr=%s: BGP message is being parsed is %d but expected at least %d in size",
                p_entry->peer_addr, router_addr.c_str(), size, BGP_MSG_HDR_LEN);
        return 0;
    }

    memcpy(&common_hdr, data, BGP_MSG_HDR_LEN);

    // Change length to host byte order
    bgp::SWAP_BYTES(&common_hdr.len);

    // Update remaining bytes left of the message
    data_bytes_remaining = common_hdr.len - BGP_MSG_HDR_LEN;

    /*
     * Error out if the remaining size of the BGP message is grater than passed bgp message buffer
     *      It is expected that the passed bgp message buffer holds the complete BGP message to be parsed
     */
    if (common_hdr.len > size) {
        LOG_WARN("%s: rtr=%s: BGP message size of %hu is greater than passed data buffer, cannot parse the BGP message",
                p_entry->peer_addr, router_addr.c_str(), common_hdr.len, size);
    }

    SELF_DEBUG("%s: rtr=%s: BGP hdr len = %u, type = %d", p_entry->peer_addr, router_addr.c_str(), common_hdr.len, common_hdr.type);

    /*
     * Validate the message type as being allowed/accepted
     */
    switch (common_hdr.type) {
        case BGP_MSG_UPDATE         : // Update Message
        case BGP_MSG_NOTIFICATION   : // Notification message
        case BGP_MSG_OPEN           : // OPEN message
            // Message(s) are allowed - calling method will request further parsing of the bgp message type
            break;

        case BGP_MSG_ROUTE_REFRESH: // Route Refresh message
            LOG_NOTICE("%s: rtr=%s: Received route refresh, nothing to do with this message currently.",
                        p_entry->peer_addr, router_addr.c_str());
            break;

        default :
            LOG_WARN("%s: rtr=%s: Unsupported BGP message type = %d", p_entry->peer_addr, router_addr.c_str(), common_hdr.type);
            break;
    }

    return common_hdr.type;
}

/**
 * Update the Database with the parsed updated data
 *
 * \details This method will update the database based on the supplied parsed update data
 *
 * \param  parsed_data          Reference to the parsed update data
 */
void parseBGP::UpdateDB(bgp_msg::UpdateMsg::parsed_udpate_data &parsed_data) {
    /*
     * Update the path attributes
     */
    UpdateDBAttrs(parsed_data.attrs);

    /*
     * Update the advertised prefixes (both ipv4 and ipv6)
     */
    UpdateDBAdvPrefixes(parsed_data.advertised);

    /*
     * Update withdraws (both ipv4 and ipv6)
     */
    UpdateDBWdrawnPrefixes(parsed_data.withdrawn);

}

/**
 * Update the Database path attributes
 *
 * \details This method will update the database for the supplied path attributes
 *
 * \param  attrs            Reference to the parsed attributes map
 */
void parseBGP::UpdateDBAttrs(bgp_msg::UpdateMsg::parsed_attrs_map &attrs) {
    DbInterface::tbl_path_attr  record;

    /*
     * Setup the record
     */
    memcpy(record.peer_hash_id, p_entry->hash_id, sizeof(record.peer_hash_id));

    record.timestamp_secs           = p_entry->timestamp_secs;
    record.as_path                  = (char *)((string)attrs[bgp_msg::ATTR_TYPE_AS_PATH]).c_str();
    record.as_path_sz               = ((string)attrs[bgp_msg::ATTR_TYPE_AS_PATH]).length();
    record.cluster_list             = (char *)((string)attrs[bgp_msg::ATTR_TYPE_CLUSTER_LIST]).c_str();
    record.cluster_list_sz          = ((string)attrs[bgp_msg::ATTR_TYPE_CLUSTER_LIST]).length();
    record.community_list           = (char *)((string)attrs[bgp_msg::ATTR_TYPE_COMMUNITIES]).c_str();
    record.community_list_sz        = ((string)attrs[bgp_msg::ATTR_TYPE_COMMUNITIES]).length();
    record.ext_community_list       = (char *)((string)attrs[bgp_msg::ATTR_TYPE_EXT_COMMUNITY]).c_str();
    record.ext_community_list_sz    = ((string)attrs[bgp_msg::ATTR_TYPE_EXT_COMMUNITY]).length();

    record.atomic_agg               = ((string)attrs[bgp_msg::ATTR_TYPE_ATOMIC_AGGREGATE]).compare("1") == 0 ? true : false;

    if (((string)attrs[bgp_msg::ATTR_TYPE_LOCAL_PREF]).length() > 0)
        record.local_pref = std::stoul(((string)attrs[bgp_msg::ATTR_TYPE_LOCAL_PREF]));
    else
        record.local_pref = 0;

    if (((string)attrs[bgp_msg::ATTR_TYPE_MED]).length() > 0)
        record.med = std::stoul(((string)attrs[bgp_msg::ATTR_TYPE_MED]));
    else
        record.med = 0;

    if (((string)attrs[bgp_msg::ATTR_TYPE_INTERNAL_AS_COUNT]).length() > 0)
        record.as_path_count = std::stoi(((string)attrs[bgp_msg::ATTR_TYPE_INTERNAL_AS_COUNT]));
    else
        record.as_path_count = 0;

    if (((string)attrs[bgp_msg::ATTR_TYPE_INTERNAL_AS_ORIGIN]).length() > 0)
        record.origin_as = std::stoul(((string)attrs[bgp_msg::ATTR_TYPE_INTERNAL_AS_ORIGIN]));
    else
        record.origin_as = 0;

    if (((string)attrs[bgp_msg::ATTR_TYPE_ORIGINATOR_ID]).length() > 0)
        strncpy(record.originator_id, ((string)attrs[bgp_msg::ATTR_TYPE_ORIGINATOR_ID]).c_str(), sizeof(record.originator_id));
    else
        bzero(record.originator_id, sizeof(record.originator_id));

    if ( ((string)attrs[bgp_msg::ATTR_TYPE_NEXT_HOP]).find_first_of(':') ==  string::npos)
        record.nexthop_isIPv4 = true;
    else // is IPv6
        record.nexthop_isIPv4 = false;

    if ( ((string)attrs[bgp_msg::ATTR_TYPE_NEXT_HOP]).length() > 0)
        strncpy(record.next_hop, ((string)attrs[bgp_msg::ATTR_TYPE_NEXT_HOP]).c_str(), sizeof(record.next_hop));
    else
        bzero(record.next_hop, sizeof(record.next_hop));

    if ( ((string)attrs[bgp_msg::ATTR_TYPE_AGGEGATOR]).length() > 0)
        strncpy(record.aggregator, ((string)attrs[bgp_msg::ATTR_TYPE_AGGEGATOR]).c_str(), sizeof(record.aggregator));
    else
        bzero(record.aggregator, sizeof(record.aggregator));

    if ( ((string)attrs[bgp_msg::ATTR_TYPE_ORIGIN]).length() > 0)
            strncpy(record.origin, ((string)attrs[bgp_msg::ATTR_TYPE_ORIGIN]).c_str(), sizeof(record.origin));
    else
        bzero(record.origin, sizeof(record.origin));

    SELF_DEBUG("%s: adding attributes to DB", p_entry->peer_addr);

    // Update the DB entry
    dbi_ptr->add_PathAttrs(record);

    // Update the class instance variable path_hash_id
    memcpy(path_hash_id, record.hash_id, sizeof(path_hash_id));
}

/**
 * Update the Database advertised prefixes
 *
 * \details This method will update the database for the supplied advertised prefixes
 *
 * \param  adv_prefixes         Reference to the list<prefix_tuple> of advertised prefixes
 */
void parseBGP::UpdateDBAdvPrefixes(std::list<bgp::prefix_tuple> &adv_prefixes) {
    vector<DbInterface::tbl_rib> rib_list;
    DbInterface::tbl_rib         rib_entry;

    /*
     * Loop through all prefixes and add/update them in the DB
     */
    for (std::list<bgp::prefix_tuple>::iterator it = adv_prefixes.begin();
                                                it != adv_prefixes.end();
                                                it++) {
        bgp::prefix_tuple &tuple = (*it);

        memcpy(rib_entry.path_attr_hash_id, path_hash_id, sizeof(rib_entry.path_attr_hash_id));
        memcpy(rib_entry.peer_hash_id, p_entry->hash_id, sizeof(rib_entry.peer_hash_id));

        strncpy(rib_entry.prefix, tuple.prefix.c_str(), sizeof(rib_entry.prefix));

        rib_entry.prefix_len     = tuple.len;
        rib_entry.timestamp_secs = p_entry->timestamp_secs;

        SELF_DEBUG("%s: Adding prefix=%s len=%d", p_entry->peer_addr, rib_entry.prefix, rib_entry.prefix_len);

        // Add entry to the list
        rib_list.insert(rib_list.end(), rib_entry);
    }

    // Update the DB
    if (rib_list.size() > 0)
        dbi_ptr->add_Rib(rib_list);

    rib_list.clear();
}

/**
 * Update the Database withdrawn prefixes
 *
 * \details This method will update the database for the supplied advertised prefixes
 *
 * \param  wdrawn_prefixes         Reference to the list<prefix_tuple> of withdrawn prefixes
 */
void parseBGP::UpdateDBWdrawnPrefixes(std::list<bgp::prefix_tuple> &wdrawn_prefixes) {
    vector<DbInterface::tbl_rib> rib_list;
    DbInterface::tbl_rib         rib_entry;

    /*
     * Loop through all prefixes and add/update them in the DB
     */
    for (std::list<bgp::prefix_tuple>::iterator it = wdrawn_prefixes.begin();
                                                it != wdrawn_prefixes.end();
                                                it++) {
        bgp::prefix_tuple &tuple = (*it);
        memcpy(rib_entry.path_attr_hash_id, path_hash_id, sizeof(rib_entry.path_attr_hash_id));
        memcpy(rib_entry.peer_hash_id, p_entry->hash_id, sizeof(rib_entry.peer_hash_id));
        strncpy(rib_entry.prefix, tuple.prefix.c_str(), sizeof(rib_entry.prefix));

        rib_entry.prefix_len     = tuple.len;
        rib_entry.timestamp_secs = p_entry->timestamp_secs;

        SELF_DEBUG("%s: Removing prefix=%s len=%d", p_entry->peer_addr, rib_entry.prefix, rib_entry.prefix_len);

        // Add entry to the list
        rib_list.insert(rib_list.end(), rib_entry);
    }

    // Update the DB
    if (rib_list.size() > 0)
        dbi_ptr->delete_Rib(rib_list);

    rib_list.clear();
}


void parseBGP::enableDebug() {
    debug = true;
}

void parseBGP::disableDebug() {
    debug = false;
}
