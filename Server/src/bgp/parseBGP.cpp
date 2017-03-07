/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
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
#include <memory>
#include <arpa/inet.h>

#include <sstream>
#include <algorithm>

#include "MsgBusInterface.hpp"
#include "NotificationMsg.h"
#include "OpenMsg.h"
#include "bgp_common.h"

#include "template_cfg.h"
#include "parseBgpLib.h"

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
 * \param [in]     mbus_ptr     Pointer to exiting dB implementation
 * \param [in,out] peer_entry  Pointer to peer entry
 * \param [in]     routerAddr  The router IP address - used for logging
 * \param [in,out] peer_info   Persistent peer information
 */
parseBGP::parseBGP(Logger *logPtr, MsgBusInterface *mbus_ptr, MsgBusInterface::obj_bgp_peer *peer_entry, string routerAddr,
                   BMPReader::peer_info *peer_info) {
    debug = false;

    logger = logPtr;

    data_bytes_remaining = 0;
    data = NULL;

    bzero(&common_hdr, sizeof(common_hdr));

    // Set our mysql pointer
    this->mbus_ptr = mbus_ptr;

    // Set our peer entry
    p_entry = peer_entry;
    p_info = peer_info;

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
bool parseBGP::handleUpdate(u_char *data, size_t size, Template_map *template_map) {
    int read_size = 0;

    if (parseBgpHeader(data, size) == BGP_MSG_UPDATE) {
        data += BGP_MSG_HDR_LEN;

     //Fill p_info fields to be passed to the parser
        p_info->peer_hash_str= parse_bgp_lib::hash_toStr(p_entry->hash_id);
        p_info->routerAddr = this->router_addr;
        p_info->peerAddr = p_entry->peer_addr;

        parse_bgp_lib::parseBgpLib::parsed_update update;
        parse_bgp_lib::parseBgpLib parser(logger, debug, p_info);
        if ((read_size=parser.parseBgpUpdate(data, data_bytes_remaining, update)) != (size - BGP_MSG_HDR_LEN)) {
            LOG_NOTICE("%s: rtr=%s: Failed to parse the update message, read %d expected %d", p_entry->peer_addr,
                       router_addr.c_str(), read_size, (size - read_size));
            return true;
        }

        data_bytes_remaining -= read_size;

        /*
         * Update the DB with the update data
         */
        UpdateDB(update, template_map);

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
bool parseBGP::handleDownEvent(u_char *data, size_t size, MsgBusInterface::obj_peer_down_event &down_event) {
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
 *
 * \returns True if error, false if no error.
 */
bool parseBGP::handleUpEvent(u_char *data, size_t size, MsgBusInterface::obj_peer_up_event *up_event) {
    bgp_msg::OpenMsg    oMsg(logger, p_entry->peer_addr, this->p_info, debug);
    list <string>       cap_list;
    string              local_bgp_id, remote_bgp_id;
    size_t              read_size;

    p_info->recv_four_octet_asn = false;
    p_info->sent_four_octet_asn = false;
    p_info->using_2_octet_asn = false;


    /*
     * Process the sent open message
     */
    if (parseBgpHeader(data, size) == BGP_MSG_OPEN) {
        data += BGP_MSG_HDR_LEN;

        read_size = oMsg.parseOpenMsg(data, data_bytes_remaining, true, up_event->local_asn, up_event->local_hold_time,
                                      local_bgp_id, cap_list);

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

            // Check for 4 octet ASN support
            if ((*it).find("4 Octet ASN") != std::string::npos)
                p_info->sent_four_octet_asn = true;

            cap_str.append((*it));
        }

        strncpy(up_event->sent_cap, cap_str.c_str(), sizeof(up_event->sent_cap));

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

        read_size = oMsg.parseOpenMsg(data, data_bytes_remaining, false, up_event->remote_asn,
                                      up_event->remote_hold_time, remote_bgp_id, cap_list);

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

            // Check for 4 octet ASN support - reset to false if
            if ((*it).find("4 Octet ASN") != std::string::npos)
                p_info->recv_four_octet_asn = true;

            cap_str.append((*it));
        }

        strncpy(up_event->recv_cap, cap_str.c_str(), sizeof(up_event->recv_cap));

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
void parseBGP::UpdateDB(parse_bgp_lib::parseBgpLib::parsed_update &update, Template_map *template_map) {
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> rib_list;
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> ls_node_list;
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> ls_link_list;
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> ls_prefix_list;
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> l3vpn_list;
    vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> evpn_list;

    /*
     * Update the path attributes
     */
    std::map<parse_bgp_lib::BGP_LIB_ATTRS, parse_bgp_lib::parseBgpLib::parse_bgp_lib_data>::iterator it = update.attrs.find(parse_bgp_lib::LIB_ATTR_NEXT_HOP);
    if (it != update.attrs.end()) {
        SELF_DEBUG("%s: adding attributes to message bus", p_entry->peer_addr);
        // Update the DB entry
        mbus_ptr->update_baseAttribute(*p_entry, update.attrs, mbus_ptr->BASE_ATTR_ACTION_ADD);
    } else {
        SELF_DEBUG("%s: no next-hop, must be unreach; not sending attributes to message bus", p_entry->peer_addr);
    }

    for (std::list<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri>::iterator it = update.nlri_list.begin();
         it != update.nlri_list.end(); it++) {
        parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri advertise_nlri = *it;
        switch (advertise_nlri.afi) {
            case parse_bgp_lib::BGP_AFI_IPV4 :
            case parse_bgp_lib::BGP_AFI_IPV6 : {
                switch (advertise_nlri.safi) {
                    case parse_bgp_lib::BGP_SAFI_UNICAST :
                    case parse_bgp_lib::BGP_SAFI_NLRI_LABEL : {
                        SELF_DEBUG("%s: Adding prefix=%s len=%s", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX].value.front().c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX_LENGTH].value.front().c_str());
                        rib_list.insert(rib_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::BGP_SAFI_MPLS : {
                        SELF_DEBUG("%s: Adding vpn=%s len=%d", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX].value.front().c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX_LENGTH].value.front().c_str());
                        l3vpn_list.insert(l3vpn_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
            case parse_bgp_lib::BGP_AFI_BGPLS : {
                switch (advertise_nlri.type) {
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_NODE : {
                        ls_node_list.insert(ls_node_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_LINK : {
                        ls_link_list.insert(ls_link_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_PREFIX : {
                        ls_prefix_list.insert(ls_prefix_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
            case parse_bgp_lib::BGP_AFI_L2VPN : {
                switch (advertise_nlri.safi) {
                    case parse_bgp_lib::BGP_SAFI_EVPN : {
                        SELF_DEBUG("%s: Adding evpn mac=%s ip=%s", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_MAC].value.size() ?
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_MAC].value.front().c_str() : string("0").c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_IP].value.size() ?
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_IP].value.front().c_str() : string("0").c_str());
                        evpn_list.insert(evpn_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
        }
    }

    if (rib_list.size() > 0) {
        mbus_ptr->update_unicastPrefix(*p_entry, rib_list, update.attrs, mbus_ptr->UNICAST_PREFIX_ACTION_ADD);
        rib_list.clear();
    }

    if (ls_node_list.size() > 0) {
        SELF_DEBUG("%s: Adding BGP-LS: Nodes %d", p_entry->peer_addr, ls_node_list.size());
        mbus_ptr->update_LsNode(*p_entry, ls_node_list, update.attrs, mbus_ptr->LS_ACTION_ADD);
        ls_node_list.clear();
    }

    if (ls_link_list.size() > 0) {
        SELF_DEBUG("%s: Adding BGP-LS: Links %d", p_entry->peer_addr, ls_link_list.size());
        mbus_ptr->update_LsLink(*p_entry, ls_link_list, update.attrs, mbus_ptr->LS_ACTION_ADD);
        ls_link_list.clear();
    }

    if (ls_prefix_list.size() > 0) {
        SELF_DEBUG("%s: Adding BGP-LS: Prefixes %d", p_entry->peer_addr, ls_prefix_list.size());
        mbus_ptr->update_LsPrefix(*p_entry, ls_prefix_list, update.attrs, mbus_ptr->LS_ACTION_ADD);
        ls_prefix_list.clear();
    }

    if (l3vpn_list.size() > 0) {
        SELF_DEBUG("%s: Adding VPN: Prefixes %d", p_entry->peer_addr, l3vpn_list.size());
        mbus_ptr->update_L3Vpn(*p_entry, l3vpn_list, update.attrs, mbus_ptr->VPN_ACTION_ADD);
        l3vpn_list.clear();
    }

    if (evpn_list.size() > 0) {
        SELF_DEBUG("%s: Adding evpn: Prefixes %d", p_entry->peer_addr, evpn_list.size());
        mbus_ptr->update_eVPN(*p_entry, evpn_list, update.attrs, mbus_ptr->VPN_ACTION_ADD);
        evpn_list.clear();
    }


    for (std::list<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri>::iterator it = update.withdrawn_nlri_list.begin();
         it != update.withdrawn_nlri_list.end(); it++) {
        parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri advertise_nlri = *it;
        switch (advertise_nlri.afi) {
            case parse_bgp_lib::BGP_AFI_IPV4 :
            case parse_bgp_lib::BGP_AFI_IPV6 : {
                switch (advertise_nlri.safi) {
                    case parse_bgp_lib::BGP_SAFI_UNICAST :
                    case parse_bgp_lib::BGP_SAFI_NLRI_LABEL : {
                        SELF_DEBUG("%s: Removing prefix=%s len=%s", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX].value.front().c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX_LENGTH].value.front().c_str());
                        rib_list.insert(rib_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::BGP_SAFI_MPLS : {
                        SELF_DEBUG("%s: Removing vpn=%s len=%d", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX].value.front().c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_PREFIX_LENGTH].value.front().c_str());
                        l3vpn_list.insert(l3vpn_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
            case parse_bgp_lib::BGP_AFI_BGPLS : {
                switch (advertise_nlri.type) {
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_NODE : {
                        ls_node_list.insert(ls_node_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_LINK : {
                        ls_link_list.insert(ls_link_list.end(), advertise_nlri);
                        break;
                    }
                    case parse_bgp_lib::LIB_NLRI_TYPE_LS_PREFIX : {
                        ls_prefix_list.insert(ls_prefix_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
            case parse_bgp_lib::BGP_AFI_L2VPN : {
                switch (advertise_nlri.safi) {
                    case parse_bgp_lib::BGP_SAFI_EVPN : {
                        SELF_DEBUG("%s: Removing evpn mac=%s ip=%s", p_entry->peer_addr,
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_MAC].value.front().c_str(),
                                   advertise_nlri.nlri[parse_bgp_lib::LIB_NLRI_EVPN_IP].value.front().c_str());
                        evpn_list.insert(evpn_list.end(), advertise_nlri);
                        break;
                    }
                }
                break;
            }
        }
    }

    if (rib_list.size() > 0) {
        mbus_ptr->update_unicastPrefix(*p_entry, rib_list, update.attrs, mbus_ptr->UNICAST_PREFIX_ACTION_DEL);
    }

    if (ls_node_list.size() > 0) {
        SELF_DEBUG("%s: Removing BGP-LS: Nodes %d", p_entry->peer_addr, ls_node_list.size());
        mbus_ptr->update_LsNode(*p_entry, ls_node_list, update.attrs, mbus_ptr->LS_ACTION_DEL);
    }

    if (ls_link_list.size() > 0) {
        SELF_DEBUG("%s: Removing BGP-LS: Links %d", p_entry->peer_addr, ls_link_list.size());
        mbus_ptr->update_LsLink(*p_entry, ls_link_list, update.attrs, mbus_ptr->LS_ACTION_DEL);
    }

    if (ls_prefix_list.size() > 0) {
        SELF_DEBUG("%s: Removing BGP-LS: Prefixes %d", p_entry->peer_addr, ls_prefix_list.size());
        mbus_ptr->update_LsPrefix(*p_entry, ls_prefix_list, update.attrs, mbus_ptr->LS_ACTION_DEL);
    }
    if (l3vpn_list.size() > 0) {
        SELF_DEBUG("%s: Removing VPN: Prefixes %d", p_entry->peer_addr, l3vpn_list.size());
        mbus_ptr->update_L3Vpn(*p_entry, l3vpn_list, update.attrs, mbus_ptr->VPN_ACTION_DEL);
    }

    if (evpn_list.size() > 0) {
        SELF_DEBUG("%s: Removing evpn: Prefixes %d", p_entry->peer_addr, evpn_list.size());
        mbus_ptr->update_eVPN(*p_entry, evpn_list, update.attrs, mbus_ptr->VPN_ACTION_DEL);
    }
}


void parseBGP::enableDebug() {
    debug = true;
}

void parseBGP::disableDebug() {
    debug = false;
}
