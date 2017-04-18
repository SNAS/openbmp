/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include "parseBgpLib.h"

#include <string>
#include <cstring>
#include <sstream>
#include <arpa/inet.h>
#include <sys/time.h>
//TODO:Remove
#include "parseBgpLibExtCommunity.h"
#include "parseBgpLibMpReach.h"
#include "parseBgpLibMpUnReach.h"
#include "parseBgpLibMpLinkstateAttr.h"
#include "md5.h"

namespace parse_bgp_lib {
/**
 * Constructor for class
 *
 * \details Handles bgp update messages
 *
 */
parseBgpLib::parseBgpLib(Logger *logPtr, bool enable_debug, BMPReader::peer_info *peer_info)
        : logger(logPtr),
          debug(enable_debug),
          p_info(peer_info) {
        four_octet_asn = peer_info->recv_four_octet_asn and peer_info->sent_four_octet_asn;
        debug_prepend_string = peer_info->peerAddr + ", rtr= " + peer_info->routerAddr + ": ";
    }

parseBgpLib::parseBgpLib(Logger *logPtr, bool enable_debug)
        : logger(logPtr),
          debug(enable_debug) {
    debug_prepend_string = "";
}

parseBgpLib::~parseBgpLib() {
}

/**
* Set the peer Info
*
* \details
* Set the peer info used late to parse the update message and for logging
* \param [in]   p_info        peer info
*
*/
void parseBgpLib::setPeerInfo(BMPReader::peer_info *peer_info) {
    this->p_info = peer_info;
    four_octet_asn = peer_info->recv_four_octet_asn and peer_info->sent_four_octet_asn;
    debug_prepend_string = peer_info->peerAddr + ", rtr= " + peer_info->routerAddr + ": ";
}

/**
 * Get internal afi
 *
 * \details
 * Given the official AFI, get the lib internal AFI
 * \param [in]   afi           AFI
 *
 * \returns internal AFI
 */
parseBgpLib::BGP_AFI_INTERNAL parseBgpLib::getInternalAfi(parse_bgp_lib::BGP_AFI oafi) {
    switch (oafi) {
        case BGP_AFI_IPV4:
            return BGP_AFI_IPV4_INTERNAL;
        case BGP_AFI_IPV6:
            return BGP_AFI_IPV6_INTERNAL;
        case BGP_AFI_BGPLS:
            return BGP_AFI_BGPLS_INTERNAL;
        default:
            LOG_WARN("Unknown AFI: %d", oafi);
    }
    return BGP_AFI_MAX_INTERNAL;
}


/**
* Get internal safi
*
* \details
* Given the official SAFI, get the lib internal SAFI
* \param [in]  safi           SAFI
*
* \returns internal SAFI
*/
parseBgpLib::BGP_SAFI_INTERNAL parseBgpLib::getInternalSafi(parse_bgp_lib::BGP_SAFI osafi) {
    switch (osafi) {
        case BGP_SAFI_UNICAST:
            return BGP_SAFI_UNICAST_INTERNAL;
        case BGP_SAFI_MULTICAST:
            return BGP_SAFI_MULTICAST_INTERNAL;
        case BGP_SAFI_NLRI_LABEL:
            return BGP_SAFI_NLRI_LABEL_INTERNAL;
        case BGP_SAFI_MCAST_VPN:
            return BGP_SAFI_MCAST_VPN_INTERNAL;
        case BGP_SAFI_VPLS:
            return BGP_SAFI_VPLS_INTERNAL;
        case BGP_SAFI_MDT:
            return BGP_SAFI_MDT_INTERNAL;
        case BGP_SAFI_4over6:
            return BGP_SAFI_4over6_INTERNAL;
        case BGP_SAFI_6over4:
            return BGP_SAFI_6over4_INTERNAL;
        case BGP_SAFI_EVPN:
            return BGP_SAFI_EVPN_INTERNAL;
        case BGP_SAFI_BGPLS:
            return BGP_SAFI_BGPLS_INTERNAL;
        case BGP_SAFI_MPLS:
            return BGP_SAFI_MPLS_INTERNAL;
        case BGP_SAFI_MCAST_MPLS_VPN:
            return BGP_SAFI_MCAST_MPLS_VPN_INTERNAL;
        case BGP_SAFI_RT_CONSTRAINTS:
            return BGP_SAFI_RT_CONSTRAINTS_INTERNAL;
    }
    LOG_WARN("Unknown SAFI: %d", osafi);
    return BGP_SAFI_MAX_INTERNAL;
}


/**
* Addpath capability for a peer
*
* \details
* Enable Addpath capability for a peer which sent the Update message to be parsed
* \param [in]   afi           AFI
* \param [in]   safi          SAFI
*
* \return void
*/
void parseBgpLib::enableAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi) {
    addPathCap[getInternalAfi(afi)][getInternalSafi(safi)] = true;
}

/**
* Addpath capability for a peer
*
* \details
* Disable Addpath capability for a peer which sent the Update message to be parsed
* \param [in]   afi           AFI
* \param [in]   safi          SAFI
*
* \return void
*/
void parseBgpLib::disableAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi) {
    addPathCap[getInternalAfi(afi)][getInternalSafi(safi)] = false;
}

/**
* Addpath capability for a peer
*
* \details
* Get Addpath capability for a peer which sent the Update message to be parsed
* \param [in]   afi           AFI
* \param [in]   safi          SAFI
*
* \return void
*/
bool parseBgpLib::getAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi) {
    return addPathCap[getInternalAfi(afi)][getInternalSafi(safi)];
}

/**
* 4-octet capability for a peer
*
* \details
* Enable 4-octet capability for a peer which sent the Update message to be parsed
*
*/
void parseBgpLib::enableFourOctetCapability(){
        asn_octet_size = 4;
}

/**
 * 4-octet capability for a peer
 *
 * \details
 * Disable 4-octet capability for a peer which sent the Update message to be parsed
 *
 */
void parseBgpLib::disableFourOctetCapability(){
        asn_octet_size = 2;
}

    /**
* Parses the update message
*
* \details
* Parse BGP update message
* \param [in]   data           Pointer to raw bgp payload data
* \param [in]   size           Size of the data available to read; prevent overrun when reading
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*
* \return ZERO is error, otherwise a positive value indicating the number of bytes read from update message
*/
size_t parseBgpLib::parseBgpUpdate(u_char *data, size_t size, parsed_update &update) {
    size_t read_size = 0;
    u_char *bufPtr = data;
    uint16_t withdrawn_len;
    u_char *withdrawnPtr;
    uint16_t attr_len;
    u_char *attrPtr;
    u_char *nlriPtr;


    // Clear the parsed_data
    update.nlri_list.clear();
    update.withdrawn_nlri_list.clear();
    update.attrs.clear();

    SELF_DEBUG("%sParsing update message of size %d", debug_prepend_string.c_str(), size);

    if (size < 2) {
        LOG_WARN("%sUpdate message is too short to parse header", debug_prepend_string.c_str());
        return 0;
    }

    // Get the withdrawn length
    memcpy(&withdrawn_len, bufPtr, sizeof(withdrawn_len));
    bufPtr += sizeof(withdrawn_len);
    read_size += sizeof(withdrawn_len);
    parse_bgp_lib::SWAP_BYTES(&withdrawn_len);

        // Set the withdrawn data pointer
    if ((size - read_size) < withdrawn_len) {
        LOG_WARN("%sUpdate message is too short to parse withdrawn data", debug_prepend_string.c_str());
        return 0;
    }

    withdrawnPtr = bufPtr;
    bufPtr += withdrawn_len;
    read_size += withdrawn_len;

        SELF_DEBUG("%sWithdrawn len = %hu", debug_prepend_string.c_str(), withdrawn_len);

    // Get the attributes length
    memcpy(&attr_len, bufPtr, sizeof(attr_len));
    bufPtr += sizeof(attr_len);
    read_size += sizeof(attr_len);
        parse_bgp_lib::SWAP_BYTES(&attr_len);
    SELF_DEBUG("%sAttribute len = %hu", debug_prepend_string.c_str(), attr_len);

    // Set the attributes data pointer
    if ((size - read_size) < attr_len) {
        LOG_WARN("%sUpdate message is too short to parse attr data", debug_prepend_string.c_str());
        return 0;
    }
    attrPtr = bufPtr;
    bufPtr += attr_len;
    read_size += attr_len;

        // Set the NLRI data pointer
    nlriPtr = bufPtr;

    /*
     * Check if End-Of-RIB
     */
    if (not withdrawn_len and (size - read_size) <= 0 and not attr_len) {
        LOG_INFO("%sEnd-Of-RIB marker", debug_prepend_string.c_str());
    } else {
        /* ---------------------------------------------------------
         * Parse the withdrawn prefixes
         */
        SELF_DEBUG("%sGetting the IPv4 withdrawn data", debug_prepend_string.c_str());
        if (withdrawn_len > 0)
            parseBgpNlri_v4(withdrawnPtr, withdrawn_len, update.withdrawn_nlri_list);


        /* ---------------------------------------------------------
         * Parse the attributes
         *      Handles MP_REACH/MP_UNREACH parsing as well
         */
        if (attr_len > 0)
            parseBgpAttr(attrPtr, attr_len, update);

        /* ---------------------------------------------------------
         * Parse the NLRI data
         */
        SELF_DEBUG("%sGetting the IPv4 NLRI data, size = %d", debug_prepend_string.c_str(), (size - read_size));
        if ((size - read_size) > 0) {
            parseBgpNlri_v4(nlriPtr, (size - read_size), update.nlri_list);
            read_size = size;
        }
    }

    return read_size;
}

    /**
     * Parses the BMP router init message
     *
     * \details
     * Parse BMP Router Init message
     * \param [in]  bmp_data        Buffer containing the data
     * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
     *
     */
    void parseBgpLib::parseBmpInitMsg(int sock, u_char *bmp_data, size_t bmp_data_len, parsed_update &update) {
        parse_bgp_lib_bmp_msg_v3 initMsg;
        char infoBuf[BMP_ROUTER_DATA_SIZE];
        int infoLen;

        u_char *bufPtr = bmp_data;

        /*
         * Create router timestamp
         */
        update.router[LIB_ROUTER_TIMESTAMP].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_TIMESTAMP];
        string ts;
        getTimestamp(0, 0, ts);
        update.router[LIB_ROUTER_TIMESTAMP].value.push_back(ts);

        /*
         * Loop through the init message (in buffer) to parse each TLV
         */
        for (int i=0; i < bmp_data_len; i += BMP_MSG_LEN) {
            memcpy(&initMsg, bufPtr, BMP_MSG_LEN);
            initMsg.info = NULL;
            bgp::SWAP_BYTES(&initMsg.len);
            bgp::SWAP_BYTES(&initMsg.type);

            bufPtr += BMP_MSG_LEN;                // Move pointer past the info header

            // TODO: Change to SELF_DEBUG after IOS supports INIT messages correctly
            SELF_DEBUG("Init message type %hu and length %hu parsed", initMsg.type, initMsg.len);

            if (initMsg.len > 0) {
                infoLen = sizeof(infoBuf) < initMsg.len ? sizeof(infoBuf) : initMsg.len;
                bzero(infoBuf, sizeof(infoBuf));
                memcpy(infoBuf, bufPtr, infoLen);
                bufPtr += infoLen;                     // Move pointer past the info data
                i += infoLen;                          // Update the counter past the info data

                initMsg.info = infoBuf;

            }

            /*
             * Save the data based on info type
             */
            switch (initMsg.type) {
                case INIT_TYPE_FREE_FORM_STRING :
                    update.router[LIB_ROUTER_INITIATE_DATA].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_INITIATE_DATA];
                    update.router[LIB_ROUTER_INITIATE_DATA].value.push_back(std::string(initMsg.info, infoLen));

                    SELF_DEBUG("Init message type %hu = %s",
                               initMsg.type, update.router[LIB_ROUTER_INITIATE_DATA].value.front().c_str());

                    break;

                case INIT_TYPE_SYSNAME :
                    update.router[LIB_ROUTER_NAME].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_NAME];
                    update.router[LIB_ROUTER_NAME].value.push_back(std::string(initMsg.info, infoLen));

                    SELF_DEBUG("Init message type %hu = %s",
                               initMsg.type, update.router[LIB_ROUTER_NAME].value.front().c_str());

                    break;

                case INIT_TYPE_SYSDESCR :
                    update.router[LIB_ROUTER_DESCR].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_DESCR];
                    update.router[LIB_ROUTER_DESCR].value.push_back(std::string(initMsg.info, infoLen));

                    SELF_DEBUG("Init message type %hu = %s",
                               initMsg.type, update.router[LIB_ROUTER_DESCR].value.front().c_str());
                    break;

                case INIT_TYPE_ROUTER_BGP_ID:
                    if (initMsg.len != sizeof(in_addr_t)) {
                        LOG_NOTICE("Init message type BGP ID not of IPv4 addr length");
                        break;
                    }
                    char ipv4_char[16];
                    inet_ntop(AF_INET, initMsg.info, ipv4_char, sizeof(ipv4_char));

                    update.router[LIB_ROUTER_BGP_ID].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_BGP_ID];
                    update.router[LIB_ROUTER_BGP_ID].value.push_back(ipv4_char);

                    SELF_DEBUG("Init message type %hu = %s",
                               initMsg.type, update.router[LIB_ROUTER_BGP_ID].value.front().c_str());

                    break;

                default:
                    LOG_NOTICE("Init message type %hu is unexpected per rfc7854", initMsg.type);
            }
        }
    }

    /**
     * Parses the BMP router Term message
     *
     * \details
     * Parse BMP Router Init message
     * \param [in]  bmp_data        Buffer containing the data
     * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
     *
     */
    void parseBgpLib::parseBmpTermMsg(int sock, u_char *bmp_data, size_t bmp_data_len, parsed_update &update) {
        parse_bgp_lib_bmp_msg_v3 termMsg;
        char infoBuf[BMP_ROUTER_DATA_SIZE];
        int infoLen;

        u_char *bufPtr = bmp_data;

        /*
         * Create router timestamp
         */
        update.router[LIB_ROUTER_TIMESTAMP].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_TIMESTAMP];
        string ts;
        getTimestamp(0, 0, ts);
        update.router[LIB_ROUTER_TIMESTAMP].value.push_back(ts);

        /*
         * Loop through the term message (in buffer) to parse each TLV
         */
        for (int i=0; i < bmp_data_len; i += BMP_MSG_LEN) {
            memcpy(&termMsg, bufPtr, BMP_MSG_LEN);
            termMsg.info = NULL;
            bgp::SWAP_BYTES(&termMsg.len);
            bgp::SWAP_BYTES(&termMsg.type);

            bufPtr += BMP_MSG_LEN;                // Move pointer past the info header

            LOG_INFO("Term message type %hu and length %hu parsed", termMsg.type, termMsg.len);

            if (termMsg.len > 0) {
                infoLen = sizeof(infoBuf) < termMsg.len ? sizeof(infoBuf) : termMsg.len;
                bzero(infoBuf, sizeof(infoBuf));
                memcpy(infoBuf, bufPtr, infoLen);
                bufPtr += infoLen;                     // Move pointer past the info data
                i += infoLen;                       // Update the counter past the info data

                termMsg.info = infoBuf;

                LOG_INFO("Term message type %hu = %s", termMsg.type, termMsg.info);
            }

            /*
             * Save the data based on info type
             */
            switch (termMsg.type) {
                case TERM_TYPE_FREE_FORM_STRING :
                    update.router[LIB_ROUTER_TERM_DATA].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_TERM_DATA];
                    update.router[LIB_ROUTER_TERM_DATA].value.push_back(std::string(termMsg.info, infoLen));
                    break;

                case TERM_TYPE_REASON :
                {
                    // Get the term reason code from info data (first 2 bytes)
                    uint16_t term_reason;
                    memcpy(&term_reason, termMsg.info, 2);
                    bgp::SWAP_BYTES(&term_reason);
                    std::ostringstream numString;
                    numString << term_reason;
                    update.router[LIB_ROUTER_TERM_REASON_CODE].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_TERM_REASON_CODE];
                    update.router[LIB_ROUTER_TERM_REASON_CODE].value.push_back(numString.str());

                    string term_reason_text;
                    switch (term_reason) {
                        case TERM_REASON_ADMIN_CLOSE :
                            LOG_INFO("BMP session closed by remote administratively");
                            term_reason_text.assign("Remote session administratively closed");
                            break;

                        case TERM_REASON_OUT_OF_RESOURCES:
                            LOG_INFO("BMP session closed by remote due to out of resources");
                            term_reason_text.assign("Remote out of resources");
                            break;

                        case TERM_REASON_REDUNDANT_CONN:
                            LOG_INFO("BMP session closed by remote due to connection being redundant");
                            term_reason_text.assign("Remote considers connection redundant");
                            break;

                        case TERM_REASON_UNSPECIFIED:
                            LOG_INFO("BMP session closed by remote as unspecified");
                            term_reason_text.assign("Remote closed with unspecified reason");
                            break;

                        default:
                            LOG_INFO("closed with undefined reason code of %d", term_reason);
                            term_reason_text.assign("Unknown %d termination reason, which is not part of draft.", term_reason);
                    }
                    update.router[LIB_ROUTER_TERM_REASON_TEXT].name = parse_bgp_lib::parse_bgp_lib_router_names[LIB_ROUTER_TERM_REASON_TEXT];
                    update.router[LIB_ROUTER_TERM_REASON_TEXT].value.push_back(term_reason_text);

                    break;
                }

                default:
                    LOG_NOTICE("Term message type %hu is unexpected per draft", termMsg.type);
            }
        }
    }


    /**
     * Parses the BMP peer header message
     *
     * \details
     * Parse BGP update message
     * \param [in]   peer_hdr       Struct peer_hdr
     * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
     *
     */
    void parseBgpLib::parseBmpPeer(int sock, parse_bgp_lib_peer_hdr &p_hdr, parsed_update &update) {
        char peer_addr[40];                         ///< Printed format of the peer address (Ipv4 and Ipv6)
        char peer_as[32];                           ///< Printed format of the peer ASN
        char peer_rd[32];                           ///< Printed format of the peer RD
        char peer_bgp_id[16];                       ///< Printed format of the peer bgp ID
        std::ostringstream numString;

        SELF_DEBUG("sock = %d, parsePeerHdr: Peer Type is %d", sock, p_hdr.peer_type);

        if (p_hdr.peer_flags & 0x80) { // V flag of 1 means this is IPv6
            update.peer[LIB_PEER_ISIPV4].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISIPV4];
            update.peer[LIB_PEER_ISIPV4].value.push_back(std::string("0"));

            inet_ntop(AF_INET6, p_hdr.peer_addr, peer_addr, sizeof(peer_addr));

            SELF_DEBUG("sock = %d: Peer address is IPv6 %s", sock, peer_addr);

        } else {
            update.peer[LIB_PEER_ISIPV4].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISIPV4];
            update.peer[LIB_PEER_ISIPV4].value.push_back(std::string("1"));

            snprintf(peer_addr, sizeof(peer_addr), "%d.%d.%d.%d",
                     p_hdr.peer_addr[12], p_hdr.peer_addr[13], p_hdr.peer_addr[14],
                     p_hdr.peer_addr[15]);
            SELF_DEBUG("sock = %d: Peer address is IPv4 %s", sock, peer_addr);
        }

        if (p_hdr.peer_flags & 0x10) { // O flag of 1 means this is Adj-Rib-Out
            SELF_DEBUG("sock=%d : Msg is for Adj-RIB-Out", sock);
            update.peer[LIB_PEER_ISPREPOLICY].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISPREPOLICY];
            update.peer[LIB_PEER_ISPREPOLICY].value.push_back(std::string("0"));

            update.peer[LIB_PEER_ISADJIN].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISADJIN];
            update.peer[LIB_PEER_ISADJIN].value.push_back(std::string("0"));
        } else if (p_hdr.peer_flags & 0x40) { // L flag of 1 means this is post-policy of Adj-RIB-In
            SELF_DEBUG("sock=%d : Msg is for POST-POLICY Adj-RIB-In", sock);
            update.peer[LIB_PEER_ISPREPOLICY].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISPREPOLICY];
            update.peer[LIB_PEER_ISPREPOLICY].value.push_back(std::string("0"));

            update.peer[LIB_PEER_ISADJIN].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISADJIN];
            update.peer[LIB_PEER_ISADJIN].value.push_back(std::string("1"));
        } else {
            SELF_DEBUG("sock=%d : Msg is for PRE-POLICY Adj-RIB-In", sock);
            update.peer[LIB_PEER_ISPREPOLICY].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISPREPOLICY];
            update.peer[LIB_PEER_ISPREPOLICY].value.push_back(std::string("1"));

            update.peer[LIB_PEER_ISADJIN].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISADJIN];
            update.peer[LIB_PEER_ISADJIN].value.push_back(std::string("1"));
        }

        // convert the BMP byte messages to human readable strings
        snprintf(peer_as, sizeof(peer_as), "0x%04x%04x",
                 p_hdr.peer_as[0] << 8 | p_hdr.peer_as[1],
                 p_hdr.peer_as[2] << 8 | p_hdr.peer_as[3]);

        inet_ntop(AF_INET, p_hdr.peer_bgp_id, peer_bgp_id, sizeof(peer_bgp_id));
        SELF_DEBUG("sock=%d : Peer BGP-ID %x.%x.%x.%x (%s)", sock, p_hdr.peer_bgp_id[0],
                   p_hdr.peer_bgp_id[1], p_hdr.peer_bgp_id[2], p_hdr.peer_bgp_id[3], peer_bgp_id);

        // Format based on the type of RD
        SELF_DEBUG("sock=%d : Peer RD type = %d %d", sock, p_hdr.peer_dist_id[0], p_hdr.peer_dist_id[1]);
        switch (p_hdr.peer_dist_id[1]) {
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
            default: // Type 0:  // admin = 2 bytes, sub field = 4 bytes
                snprintf(peer_rd, sizeof(peer_rd), "%d:%lu",
                         p_hdr.peer_dist_id[2] << 8 | p_hdr.peer_dist_id[3],
                         (unsigned long) (p_hdr.peer_dist_id[4] << 24
                                          | p_hdr.peer_dist_id[5] << 16
                                          | p_hdr.peer_dist_id[6] << 8 | p_hdr.peer_dist_id[7]));
                break;
        }

        // Update the DB peer entry struct
        update.peer[LIB_PEER_ADDR].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ADDR];
        update.peer[LIB_PEER_ADDR].value.push_back(peer_addr);

        numString.str(std::string());
        numString << strtoll(peer_as, NULL, 16);

        update.peer[LIB_PEER_AS].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_AS];
        update.peer[LIB_PEER_AS].value.push_back(numString.str());

        update.peer[LIB_PEER_BGP_ID].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_BGP_ID];
        update.peer[LIB_PEER_BGP_ID].value.push_back(peer_bgp_id);

        update.peer[LIB_PEER_RD].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_RD];
        update.peer[LIB_PEER_RD].value.push_back(peer_rd);

        // Save the advertised timestamp
        bgp::SWAP_BYTES(&p_hdr.ts_secs);
        bgp::SWAP_BYTES(&p_hdr.ts_usecs);

        if (p_hdr.ts_secs != 0) {
            numString.str(std::string());
            numString << p_hdr.ts_secs;

            update.peer[LIB_PEER_TIMESTAMP_SECS].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP_SECS];
            update.peer[LIB_PEER_TIMESTAMP_SECS].value.push_back(numString.str());

            numString.str(std::string());
            numString << p_hdr.ts_usecs;

            update.peer[LIB_PEER_TIMESTAMP_USECS].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP_USECS];
            update.peer[LIB_PEER_TIMESTAMP_USECS].value.push_back(numString.str());

            update.peer[LIB_PEER_TIMESTAMP].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP];
            string ts;
            getTimestamp(p_hdr.ts_secs, p_hdr.ts_usecs, ts);
            update.peer[LIB_PEER_TIMESTAMP].value.push_back(ts);

        } else {
            timeval tv;

            gettimeofday(&tv, NULL);

            numString.str(std::string());
            numString << tv.tv_sec;

            update.peer[LIB_PEER_TIMESTAMP_SECS].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP_SECS];
            update.peer[LIB_PEER_TIMESTAMP_SECS].value.push_back(numString.str());

            numString.str(std::string());
            numString << tv.tv_usec;

            update.peer[LIB_PEER_TIMESTAMP_USECS].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP_USECS];
            update.peer[LIB_PEER_TIMESTAMP_USECS].value.push_back(numString.str());

            update.peer[LIB_PEER_TIMESTAMP].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_TIMESTAMP];
            string ts;
            getTimestamp(tv.tv_sec, tv.tv_usec, ts);
            update.peer[LIB_PEER_TIMESTAMP].value.push_back(ts);

        }


        // Is peer type L3VPN peer or global instance
        if (p_hdr.peer_type == 1) { // L3VPN
            update.peer[LIB_PEER_ISL3VPN].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISL3VPN];
            update.peer[LIB_PEER_ISL3VPN].value.push_back(std::string("1"));
        } else {
            // Global Instance
            update.peer[LIB_PEER_ISL3VPN].name = parse_bgp_lib::parse_bgp_lib_peer_names[LIB_PEER_ISL3VPN];
            update.peer[LIB_PEER_ISL3VPN].value.push_back(std::string("0"));
        }

        SELF_DEBUG("sock=%d : Peer Address = %s", sock, peer_addr);
        SELF_DEBUG("sock=%d : Peer AS = (%x-%x)%x:%x", sock,
                   p_hdr.peer_as[0], p_hdr.peer_as[1], p_hdr.peer_as[2],
                   p_hdr.peer_as[3]);
        SELF_DEBUG("sock=%d : Peer RD = %s", sock, peer_rd);

    }


    /**
* Parses the BGP prefixes (advertised and withdrawn) in the update
*
* \details
*     Parses all attributes.  Decoded values are updated in 'parsed_data'
*
* \param [in]   data       Pointer to the start of the prefixes to be parsed
* \param [in]   len        Length of the data in bytes to be read
* \param [in]   nlri_list Reference to parsed_update_data nlri list;
*/
void parseBgpLib::parseBgpNlri_v4(u_char *data, uint16_t len, std::list<parse_bgp_lib_nlri> &nlri_list) {
    u_char ipv4_raw[4];
    char ipv4_char[16];
    u_char addr_bytes;
    uint32_t path_id;
    u_char prefix_len;
    std::ostringstream numString;


    if (len <= 0 or data == NULL)
        return;

        // Loop through all prefixes
    for (size_t read_size = 0; read_size < len; read_size++) {
        parse_bgp_lib_nlri nlri;
        nlri.afi = parse_bgp_lib::BGP_AFI_IPV4;
        nlri.safi = parse_bgp_lib::BGP_SAFI_UNICAST;
        nlri.type = parse_bgp_lib::LIB_NLRI_TYPE_NONE;

        // Generate the hash
        MD5 hash;

        bzero(ipv4_raw, sizeof(ipv4_raw));

        // Parse add-paths if enabled
        bool peer_info_addpath = p_info and p_info->add_path_capability.isAddPathEnabled(bgp::BGP_AFI_IPV4, bgp::BGP_SAFI_UNICAST);

        if ((peer_info_addpath or addPathCap[BGP_AFI_IPV4_INTERNAL][BGP_SAFI_UNICAST_INTERNAL])
            and (len - read_size) >= 4) {
            memcpy(&path_id, data, 4);
            parse_bgp_lib::SWAP_BYTES(&path_id);
            data += 4;
            read_size += 4;
        } else
            path_id = 0;
        numString.str(std::string());
        numString << path_id;
        nlri.nlri[LIB_NLRI_PATH_ID].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PATH_ID];
        nlri.nlri[LIB_NLRI_PATH_ID].value.push_back(numString.str());

        if (path_id > 0)
            update_hash(&nlri.nlri[LIB_NLRI_PATH_ID].value, &hash);

        // set the address in bits length
        prefix_len = *data++;
        numString.str(std::string());
        numString << static_cast<unsigned>(prefix_len);

        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_LENGTH];
        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value.push_back(numString.str());
        update_hash(&nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value, &hash);

        // Figure out how many bytes the bits requires
        addr_bytes = prefix_len / 8;
        if (prefix_len % 8)
            ++addr_bytes;

        SELF_DEBUG("%sReading NLRI data prefix bits=%d bytes=%d", debug_prepend_string.c_str(), prefix_len, addr_bytes);

        if (addr_bytes <= 4) {
            memcpy(ipv4_raw, data, addr_bytes);
            read_size += addr_bytes;
            data += addr_bytes;

            // Convert the IP to string printed format
            inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
            nlri.nlri[LIB_NLRI_PREFIX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX];
            nlri.nlri[LIB_NLRI_PREFIX].value.push_back(ipv4_char);
            update_hash(&nlri.nlri[LIB_NLRI_PREFIX].value, &hash);
            SELF_DEBUG("%sAdding prefix %s len %d", debug_prepend_string.c_str(), ipv4_char, prefix_len);

            // set the raw/binary address
            nlri.nlri[LIB_NLRI_PREFIX_BIN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_BIN];
            nlri.nlri[LIB_NLRI_PREFIX_BIN].value.push_back(std::string(ipv4_raw, ipv4_raw + 4));

            //Update hash to include peer hash id
            if (p_info)
                hash.update((unsigned char *) p_info->peer_hash_str.c_str(), p_info->peer_hash_str.length());

            hash.finalize();

            // Save the hash
            unsigned char *hash_raw = hash.raw_digest();
            nlri.nlri[LIB_NLRI_HASH].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_HASH];
            nlri.nlri[LIB_NLRI_HASH].value.push_back(parse_bgp_lib::hash_toStr(hash_raw));
            delete[] hash_raw;

            nlri.nlri[LIB_NLRI_IS_IPV4].name =parse_bgp_lib_nlri_names[LIB_NLRI_IS_IPV4];
            nlri.nlri[LIB_NLRI_IS_IPV4].value.push_back(string("1"));

            // Add tuple to prefix list
            nlri_list.push_back(nlri);
        } else if (addr_bytes > 4) {
            LOG_NOTICE("%sNRLI v4 address is larger than 4 bytes bytes=%d len=%d", debug_prepend_string.c_str(), addr_bytes, prefix_len);
        }
    }
}

/**
* Parses the BGP attributes in the update
*
* \details
*     Parses all attributes.  Decoded values are updated in 'parsed_data'
*
* \param [in]   data       Pointer to the start of the prefixes to be parsed
* \param [in]   len        Length of the data in bytes to be read
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void parseBgpLib::parseBgpAttr(u_char *data, uint16_t len, parsed_update &update) {
    /*
     * Per RFC4271 Section 4.3, flag indicates if the length is 1 or 2 octets
     */
    u_char attr_flags;
    u_char attr_type;
    uint16_t attr_len;

    if (len == 0)
        return;

    else if (len < 3) {
        LOG_WARN("%sCannot parse the attributes due to the data being too short, error in update message. len=%d",
                 debug_prepend_string.c_str(), len);
        return;
    }

        // Generate the hash
        MD5 hash;

     /*
     * Iterate through all attributes and parse them
     */
    for (int read_size = 0; read_size < len; read_size += 2) {
        attr_flags = *data++;
        attr_type = *data++;

        // Check if the length field is 1 or two bytes
        if (ATTR_FLAG_EXTENDED(attr_flags)) {
            SELF_DEBUG("%sExtended length path attribute bit set for an entry", debug_prepend_string.c_str());
            memcpy(&attr_len, data, 2);
            data += 2;
            read_size += 2;
            parse_bgp_lib::SWAP_BYTES(&attr_len);
        } else
            attr_len = *data++;
        read_size++;

        // Get the attribute data, if we have any; making sure to not overrun buffer
        if (attr_len > 0 and (read_size + attr_len) <= len) {
            // Data pointer is currently at the data position of the attribute

            /*
             * Parse data based on attribute type
             */
            parseAttrData(attr_type, attr_len, data, update, hash);
            data += attr_len;
            read_size += attr_len;

            SELF_DEBUG("%sParsed attr Type=%d, size=%hu", debug_prepend_string.c_str(), attr_type, attr_len);
        } else if (attr_len) {
            LOG_NOTICE("%sAttribute data len of %hu is larger than available data in update message of %hu",
                       debug_prepend_string.c_str(), attr_len, (len - read_size));
            return;
        }
    }
        //Now save the generate hash
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        update.attrs[LIB_ATTR_BASE_ATTR_HASH].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_BASE_ATTR_HASH];
        update.attrs[LIB_ATTR_BASE_ATTR_HASH].value.push_back(parse_bgp_lib::hash_toStr(hash_raw));
        delete[] hash_raw;

    }


/**
* Parse attribute data based on attribute type
*
* \details
*      Parses the attribute data based on the passed attribute type.
*      Parsed_data will be updated based on the attribute data parsed.
*
* \param [in]   attr_type      Attribute type
* \param [in]   attr_len       Length of the attribute data
* \param [in]   data           Pointer to the attribute data
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void parseBgpLib::parseAttrData(u_char attr_type, uint16_t attr_len, u_char *data, parsed_update &update, MD5 &hash) {
    u_char ipv4_raw[4];
    char ipv4_char[16];
    uint32_t value32bit;
    uint16_t value16bit;

    if (p_info)
        hash.update((unsigned char *) p_info->peer_hash_str.c_str(), p_info->peer_hash_str.length());

        /*
         * Parse based on attribute type
         */
    switch (attr_type) {

        case ATTR_TYPE_ORIGIN : // Origin
            update.attrs[LIB_ATTR_ORIGIN].official_type = ATTR_TYPE_ORIGIN;
            update.attrs[LIB_ATTR_ORIGIN].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_ORIGIN];
            switch (data[0]) {
                case 0 :
                    update.attrs[LIB_ATTR_ORIGIN].value.push_back(std::string("igp"));
                    break;
                case 1 :
                    update.attrs[LIB_ATTR_ORIGIN].value.push_back(std::string("egp"));
                    break;
                case 2 :
                    update.attrs[LIB_ATTR_ORIGIN].value.push_back(std::string("incomplete"));
                    break;
            }
            update_hash(&update.attrs[LIB_ATTR_ORIGIN].value, &hash);

            break;

        case ATTR_TYPE_AS_PATH : // AS_PATH
            parseAttrDataAsPath(attr_len, data, update);
            update_hash(&update.attrs[LIB_ATTR_AS_PATH].value, &hash);
            break;

        case ATTR_TYPE_NEXT_HOP : // Next hop v4
            memcpy(ipv4_raw, data, 4);
            inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
            update.attrs[LIB_ATTR_NEXT_HOP].official_type = ATTR_TYPE_NEXT_HOP;
            update.attrs[LIB_ATTR_NEXT_HOP].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_NEXT_HOP];
            update.attrs[LIB_ATTR_NEXT_HOP].value.push_back(std::string(ipv4_char));
            update_hash(&update.attrs[LIB_ATTR_NEXT_HOP].value, &hash);

            break;

        case ATTR_TYPE_MED : // MED value
        {
            memcpy(&value32bit, data, 4);
            parse_bgp_lib::SWAP_BYTES(&value32bit);

            std::ostringstream numString;
            numString << value32bit;
            update.attrs[LIB_ATTR_MED].official_type = ATTR_TYPE_MED;
            update.attrs[LIB_ATTR_MED].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_MED];
            update.attrs[LIB_ATTR_MED].value.push_back(numString.str());
            update_hash(&update.attrs[LIB_ATTR_MED].value, &hash);
            break;
        }
        case ATTR_TYPE_LOCAL_PREF : // local pref value
        {
            memcpy(&value32bit, data, 4);
            parse_bgp_lib::SWAP_BYTES(&value32bit);

             std::ostringstream numString;
            numString << value32bit;
            char numstring[100] = {0};
            update.attrs[LIB_ATTR_LOCAL_PREF].official_type = ATTR_TYPE_LOCAL_PREF;
            update.attrs[LIB_ATTR_LOCAL_PREF].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LOCAL_PREF];
            update.attrs[LIB_ATTR_LOCAL_PREF].value.push_back(numString.str());
            update_hash(&update.attrs[LIB_ATTR_LOCAL_PREF].value, &hash);

            break;
        }
        case ATTR_TYPE_ATOMIC_AGGREGATE : // Atomic aggregate
            update.attrs[LIB_ATTR_ATOMIC_AGGREGATE].official_type = ATTR_TYPE_ATOMIC_AGGREGATE;
            update.attrs[LIB_ATTR_ATOMIC_AGGREGATE].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_ATOMIC_AGGREGATE];
            update.attrs[LIB_ATTR_ATOMIC_AGGREGATE].value.push_back(std::string("1"));
            break;

        case ATTR_TYPE_AGGREGATOR : // Aggregator
            parseAttrDataAggregator(attr_len, data, update);
            update_hash(&update.attrs[LIB_ATTR_AGGREGATOR].value, &hash);

            break;

        case ATTR_TYPE_ORIGINATOR_ID : // Originator ID
            memcpy(ipv4_raw, data, 4);
            inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
            update.attrs[LIB_ATTR_ORIGINATOR_ID].official_type = ATTR_TYPE_ORIGINATOR_ID;
            update.attrs[LIB_ATTR_ORIGINATOR_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_ORIGINATOR_ID];
            update.attrs[LIB_ATTR_ORIGINATOR_ID].value.push_back(std::string(ipv4_char));
            break;

        case ATTR_TYPE_CLUSTER_LIST : // Cluster List (RFC 4456)
            // According to RFC 4456, the value is a sequence of cluster id's
            update.attrs[LIB_ATTR_CLUSTER_LIST].official_type = ATTR_TYPE_CLUSTER_LIST;
            update.attrs[LIB_ATTR_CLUSTER_LIST].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_CLUSTER_LIST];
            for (int i = 0; i < attr_len; i += 4) {
                memcpy(ipv4_raw, data, 4);
                data += 4;
                inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
                update.attrs[LIB_ATTR_CLUSTER_LIST].value.push_back(std::string(ipv4_char));
            }
            break;

        case ATTR_TYPE_COMMUNITIES : // Community list
        {
            update.attrs[LIB_ATTR_COMMUNITIES].official_type = ATTR_TYPE_COMMUNITIES;
            update.attrs[LIB_ATTR_COMMUNITIES].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_COMMUNITIES];
            for (int i = 0; i < attr_len; i += 4) {
                std::ostringstream numString;
                // Add entry
                memcpy(&value16bit, data, 2);
                data += 2;
                parse_bgp_lib::SWAP_BYTES(&value16bit);
                numString << value16bit;
                numString << ":";

                memcpy(&value16bit, data, 2);
                data += 2;
                parse_bgp_lib::SWAP_BYTES(&value16bit);
                numString << value16bit;
                update.attrs[LIB_ATTR_COMMUNITIES].value.push_back(numString.str());
            }
            update_hash(&update.attrs[LIB_ATTR_COMMUNITIES].value, &hash);

            break;
        }
        case ATTR_TYPE_EXT_COMMUNITY : // extended community list (RFC 4360)
        {
            update.attrs[LIB_ATTR_EXT_COMMUNITY].official_type = ATTR_TYPE_EXT_COMMUNITY;
            update.attrs[LIB_ATTR_EXT_COMMUNITY].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_EXT_COMMUNITY];
            parse_bgp_lib::ExtCommunity ec(this, logger, debug);
            ec.parseExtCommunities(attr_len, data, update);
            update_hash(&update.attrs[LIB_ATTR_EXT_COMMUNITY].value, &hash);

            break;
        }

        case ATTR_TYPE_IPV6_EXT_COMMUNITY : // IPv6 specific extended community list (RFC 5701)
        {
            update.attrs[LIB_ATTR_IPV6_EXT_COMMUNITY].official_type = ATTR_TYPE_IPV6_EXT_COMMUNITY;
            update.attrs[LIB_ATTR_IPV6_EXT_COMMUNITY].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_IPV6_EXT_COMMUNITY];
            parse_bgp_lib::ExtCommunity ec6(this, logger, debug);
            ec6.parsev6ExtCommunities(attr_len, data, update);
            break;
        }

        case ATTR_TYPE_MP_REACH_NLRI :  // RFC4760
        {
            parse_bgp_lib::MPReachAttr mp(this, logger, debug);
            mp.parseReachNlriAttr(attr_len, data, update);
            break;
        }

        case ATTR_TYPE_MP_UNREACH_NLRI : // RFC4760
        {
            parse_bgp_lib::MPUnReachAttr mp(this, logger, debug);
            mp.parseUnReachNlriAttr(attr_len, data, update);
            break;
        }

        case ATTR_TYPE_AS_PATHLIMIT : // deprecated
        {
            break;
        }

        case ATTR_TYPE_BGP_LS: {
            MPLinkStateAttr ls(this, logger, &update, debug);
            ls.parseAttrLinkState(attr_len, data);
            break;
        }

        case ATTR_TYPE_AS4_PATH: {
            SELF_DEBUG("%sAttribute type AS4_PATH is not yet implemented, skipping for now.", debug_prepend_string.c_str());
            break;
        }

        case ATTR_TYPE_AS4_AGGREGATOR: {
            SELF_DEBUG("%sAttribute type AS4_AGGREGATOR is not yet implemented, skipping for now.", debug_prepend_string.c_str());
            break;
        }

        default:
            LOG_INFO("%sAttribute type %d is not yet implemented or intentionally ignored, skipping for now.",
                     debug_prepend_string.c_str(), attr_type);
            break;

    } // END OF SWITCH ATTR TYPE
}

/**
* Parse attribute AGGEGATOR data
*
* \param [in]   attr_len       Length of the attribute data
* \param [in]   data           Pointer to the attribute data
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void parseBgpLib::parseAttrDataAggregator(uint16_t attr_len, u_char *data, parsed_update &update) {
    std::string decodeStr;
    uint32_t    value32bit = 0;
    uint16_t    value16bit = 0;
    u_char      ipv4_raw[4];
    char        ipv4_char[16];

    // If using RFC6793, the len will be 8 instead of 6
    if (attr_len == 8) { // RFC6793 ASN of 4 octets
        memcpy(&value32bit, data, 4); data += 4;
        parse_bgp_lib::SWAP_BYTES(&value32bit);
        std::ostringstream numString;
        numString << value32bit;
        decodeStr.assign(numString.str());

    } else if (attr_len == 6) {
        memcpy(&value16bit, data, 2); data += 2;
        parse_bgp_lib::SWAP_BYTES(&value16bit);
        std::ostringstream numString;
        numString << value16bit;
        decodeStr.assign(numString.str());

    } else {
        LOG_ERR("%sPath attribute is not the correct size of 6 or 8 octets.", debug_prepend_string.c_str());
        return;
    }

    decodeStr.append(" ");
    memcpy(ipv4_raw, data, 4);
    inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
    decodeStr.append(ipv4_char);

    update.attrs[LIB_ATTR_AGGREGATOR].official_type = ATTR_TYPE_AGGREGATOR;
    update.attrs[LIB_ATTR_AGGREGATOR].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_AGGREGATOR];
    update.attrs[LIB_ATTR_AGGREGATOR].value.push_back(decodeStr);
}


    /**
* Parse attribute AS_PATH data
*
* \param [in]   attr_len       Length of the attribute data
* \param [in]   data           Pointer to the attribute data
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void parseBgpLib::parseAttrDataAsPath(uint16_t attr_len, u_char *data, parsed_update &update) {
    int         path_len    = attr_len;
    u_char      seg_type;
    u_char      seg_len;
    uint32_t    seg_asn;
    uint32_t    origin_asn = -1;

    if (path_len < 4) // Nothing to parse if length doesn't include at least one asn
        return;

    update.attrs[LIB_ATTR_AS_PATH].official_type = ATTR_TYPE_AS_PATH;
    update.attrs[LIB_ATTR_AS_PATH].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_AS_PATH];

    /*
* Per draft-ietf-grow-bmp, UPDATES must be sent as 4-octet, but this requires the
*    UPDATE to be modified. In draft 14 a new peer header flag indicates size, but
*    not all implementations support this draft yet.
*
*    IOS XE/XR does not modify the UPDATE and therefore a peers
*    that is using 2-octet ASN's will not be parsed correctly.  Global instance var
*    four_octet_asn is used to check if the OPEN cap sent/recv 4-octet or not. A compliant
*    BMP implementation will still use 4-octet even if the peer is 2-octet, so a check is
*    needed to see if the as path is encoded using 4 or 2 octet. This check is only done
*    once.
*
*    This is temporary and can be removed after all implementations are complete with bmp draft 14 or greater.
*/
    if (p_info and not p_info->checked_asn_octet_length and not four_octet_asn)
    {
        /*
         * Loop through each path segment
         */
        u_char *d_ptr = data;
        while (path_len > 0) {
            d_ptr++; // seg_type
            seg_len = *d_ptr++;

            path_len -= 2 + (seg_len * 4);

            if (path_len >= 0)
                d_ptr += seg_len * 4;
        }

        if (path_len != 0) {
            LOG_INFO("%sUsing 2-octet ASN path parsing", debug_prepend_string.c_str());
            p_info->using_2_octet_asn = true;
        }
        p_info->checked_asn_octet_length = true;         // No more checking needed
        path_len = attr_len;                                // Put the path length back to starting value
    }

    // Define the octet size by known/detected size
    if (p_info)
        asn_octet_size = (p_info->using_2_octet_asn and not four_octet_asn) ? 2 : 4;

    /*
         * Loop through each path segment
         */
    while (path_len > 0) {

        seg_type = *data++;
        seg_len  = *data++;                  // Count of AS's, not bytes
        path_len -= 2;
        std::string decoded_path;

        if (seg_type == 1) {                 // If AS-SET open with a brace
            decoded_path.append(" {");
        }

        SELF_DEBUG("%sas_path seg_len = %d seg_type = %d, path_len = %d total_len = %d as_octet_size = %d", debug_prepend_string.c_str(),
                   seg_len, seg_type, path_len, attr_len, asn_octet_size);

        if ((seg_len * asn_octet_size) > path_len){

            LOG_NOTICE("%sCould not parse the AS PATH due to update message buffer being too short when using ASN octet size %d",
                       debug_prepend_string.c_str(), asn_octet_size);
            LOG_NOTICE("%sSwitching encoding size to 2-octet due to parsing failure", debug_prepend_string.c_str());

            asn_octet_size = 2;
        }

        // The rest of the data is the as path sequence, in blocks of 2 or 4 bytes
        for (; seg_len > 0; seg_len--) {
            seg_asn = 0;
            memcpy(&seg_asn, data, asn_octet_size);
            data += asn_octet_size;
            path_len -= asn_octet_size;                               // Adjust the path length for what was read

            parse_bgp_lib::SWAP_BYTES(&seg_asn, asn_octet_size);
            std::ostringstream numString;
            numString << seg_asn;
            if (seg_type == 2) {
                update.attrs[LIB_ATTR_AS_PATH].value.push_back(numString.str());
                origin_asn = seg_asn;
            } else if (seg_type == 1) {
                decoded_path.append(", ");
                decoded_path.append(numString.str());
            } else {
                std::cout << "Malformed AS path segment of type: " << static_cast<int>(seg_type) <<std::endl;
            }
        }

        if (seg_type == 1) {            // If AS-SET close with a brace
            decoded_path.append(" }");
            update.attrs[LIB_ATTR_AS_PATH].value.push_back(decoded_path);
        }
    }

        if (origin_asn >= 0) {
            update.attrs[LIB_ATTR_AS_ORIGIN].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_AS_ORIGIN];
            std::ostringstream numString;
            numString << origin_asn;
            update.attrs[LIB_ATTR_AS_ORIGIN].value.push_back(numString.str());
        }

    SELF_DEBUG("%sParsed AS_PATH count %d, origin as: %s", debug_prepend_string.c_str(), update.attrs[LIB_ATTR_AS_PATH].value.size(),
               update.attrs[LIB_ATTR_AS_PATH].value.size() > 0 ? update.attrs[LIB_ATTR_AS_PATH].value.front().c_str() : "NULL");
}

}
