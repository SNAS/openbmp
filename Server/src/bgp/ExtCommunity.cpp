/*
 * Copyright (c) 2014-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * Copyright (c) 2014 Sungard Availability Services and others. All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 * 
 */

#include <sstream>
#include <iostream>
#include <arpa/inet.h>

#include "UpdateMsg.h"
#include "ExtCommunity.h"

namespace bgp_msg {
    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     pperAddr     Printed form of peer address used for logging
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    ExtCommunity::ExtCommunity(Logger *logPtr, std::string peerAddr, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        peer_addr = peerAddr;
    }

    ExtCommunity::~ExtCommunity() {

    }

    /**
     * Parse the extended communities path attribute (8 byte as per RFC4360)
     *
     * \details
     *     Will parse the EXTENDED COMMUNITIES data passed. Parsed data will be stored
     *     in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void ExtCommunity::parseExtCommunities(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_update_data &parsed_data) {

        std::string decodeStr = "";
        extcomm_hdr ec_hdr;

        if ( (attr_len % 8) ) {
            LOG_NOTICE("%s: Parsing extended community len=%d is invalid, expecting divisible by 8", peer_addr.c_str(), attr_len);
            return;
        }

        /*
         * Loop through consecutive entries
         */
        for (int i = 0; i < attr_len; i += 8) {
            // Setup extended community header
            ec_hdr.high_type = data[0];
            ec_hdr.low_type  = data[1];
            ec_hdr.value     = data + 2;

            /*
             * Docode the community by type
             */
            switch (ec_hdr.high_type << 2 >> 2) {
                case EXT_TYPE_IPV4 :
                    decodeStr.append(decodeType_common(ec_hdr, true, true));
                    break;

                case EXT_TYPE_2OCTET_AS :
                    decodeStr.append(decodeType_common(ec_hdr));
                    break;

                case EXT_TYPE_4OCTET_AS :
                    decodeStr.append(decodeType_common(ec_hdr, true));
                    break;

                case EXT_TYPE_GENERIC :
                    decodeStr.append(decodeType_Generic(ec_hdr));
                    break;

                case EXT_TYPE_GENERIC_4OCTET_AS :
                    decodeStr.append(decodeType_Generic(ec_hdr, true));
                    break;

                case EXT_TYPE_GENERIC_IPV4 :
                    decodeStr.append(decodeType_Generic(ec_hdr, true, true));
                    break;

                case EXT_TYPE_OPAQUE :
                    decodeStr.append(decodeType_Opaque(ec_hdr));
                    break;

                case EXT_TYPE_EVPN      : // TODO: Implement
                case EXT_TYPE_QOS_MARK  : // TODO: Implement
                case EXT_TYPE_FLOW_SPEC : // TODO: Implement
                case EXT_TYPE_COS_CAP   : // TODO: Implement
                default:
                    LOG_INFO("%s: Extended community type %d,%d is not yet supported", peer_addr.c_str(),
                            ec_hdr.high_type, ec_hdr.low_type);
            }

            // Move data pointer to next entry
            data += 8;
            if ((i + 8) < attr_len)
                decodeStr.append(" ");
        }

        parsed_data.attrs[ATTR_TYPE_EXT_COMMUNITY] = decodeStr;
    }

    /**
     * Decode common Type/Subtypes
     *
     * \details
     *      Decodes the common 2-octet, 4-octet, and IPv4 specific common subtypes.
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     * \param [in]   isGlobal4Bytes  True if the global admin field is 4 bytes, false if 2
     * \param [in]   isGlobalIPv4    True if the global admin field is an IPv4 address, false if not
     *
     * \return  Decoded string value
     */
    std::string ExtCommunity::decodeType_common(const extcomm_hdr &ec_hdr, bool isGlobal4Bytes, bool isGlobalIPv4) {
        std::stringstream   val_ss;
        uint16_t            val_16b;
        uint32_t            val_32b;
        char                ipv4_char[16] = {0};

        /*
         * Decode values based on bit size
         */
        if (isGlobal4Bytes) {
            // Four-byte global field
            memcpy(&val_32b, ec_hdr.value, 4);
            memcpy(&val_16b, ec_hdr.value + 4, 2);

            bgp::SWAP_BYTES(&val_16b);

            if (isGlobalIPv4) {
                inet_ntop(AF_INET, &val_32b, ipv4_char, sizeof(ipv4_char));
            } else
                bgp::SWAP_BYTES(&val_32b);

        } else {
            // Two-byte global field
            memcpy(&val_16b, ec_hdr.value, 2);
            memcpy(&val_32b, ec_hdr.value + 2, 4);

            // Chagne to host order
            bgp::SWAP_BYTES(&val_16b);
            bgp::SWAP_BYTES(&val_32b);
        }

        /*
         * Decode by subtype
         */
        switch (ec_hdr.low_type) {

            case EXT_COMMON_BGP_DATA_COL :
                if (isGlobal4Bytes)
                    val_ss << "colc=" << val_32b << ":" << val_16b;

                else
                    val_ss << "colc=" << val_16b << ":" << val_32b;

                break;

            case EXT_COMMON_ROUTE_ORIGIN :
                if (isGlobalIPv4)
                    val_ss << "soo=" << ipv4_char << ":" << val_16b;

                else if (isGlobal4Bytes)
                    val_ss << "soo=" << val_32b << ":" << val_16b;

                else
                    val_ss << "soo=" << val_16b << ":" << val_32b;
                break;

            case EXT_COMMON_ROUTE_TARGET :
                if (isGlobalIPv4)
                    val_ss << "rt=" << ipv4_char << ":" << val_16b;

                else if (isGlobal4Bytes)
                    val_ss << "rt=" << val_32b << ":" << val_16b;

                else
                    val_ss << "rt=" << val_16b << ":" << val_32b;
                break;

            case EXT_COMMON_SOURCE_AS :
                if (isGlobal4Bytes)
                    val_ss << "sas=" << val_32b << ":" << val_16b;

                else
                    val_ss << "sas=" << val_16b << ":" << val_32b;

                break;

            case EXT_COMMON_CISCO_VPN_ID :
                if (isGlobalIPv4)
                    val_ss << "vpn-id=" << ipv4_char << ":0x" << std::hex << val_16b;

                else if (isGlobal4Bytes)
                    val_ss << "vpn-id=" << val_32b << ":0x" << std::hex << val_16b;

                else
                    val_ss << "vpn-id=" << val_16b << ":0x" << std::hex << val_32b;

                break;

            case EXT_COMMON_L2VPN_ID :
                if (isGlobalIPv4)
                    val_ss << "vpn-id=" << ipv4_char << ":0x" << std::hex << val_16b;

                else if (isGlobal4Bytes)
                    val_ss << "vpn-id=" << val_32b << ":0x" << std::hex << val_16b;

                else
                    val_ss << "vpn-id=" << val_16b << ":0x" << std::hex << val_32b;

                break;

            case EXT_COMMON_LINK_BANDWIDTH : // is same as EXT_COMMON_GENERIC
                if (isGlobal4Bytes)
                    val_ss << "link-bw=" << val_32b << ":" << val_16b;

                else
                    val_ss << "link-bw=" << val_16b << ":" << val_32b;

                break;

            case EXT_COMMON_OSPF_DOM_ID :
                if (isGlobalIPv4)
                    val_ss << "ospf-did=" << ipv4_char << ":" << val_16b;
                else if (isGlobal4Bytes)
                    val_ss << "ospf-did=" << val_32b << ":" << val_16b;
                else
                    val_ss << "ospf-did=" << val_16b << ":" << val_32b;
                break;

            case EXT_COMMON_VRF_IMPORT :
                if (isGlobalIPv4)
                    val_ss << "import=" << ipv4_char << ":" << val_16b;

                else if (isGlobalIPv4)
                    val_ss << "import=" << val_32b << ":" << val_16b;

                else
                    val_ss << "import=" << val_16b << ":" << val_32b;

                break;

            case EXT_COMMON_IA_P2MP_SEG_NH :
                if (isGlobalIPv4)
                    val_ss << "p2mp-nh=" << ipv4_char << ":" << val_16b;

                else
                    val_ss << "p2mp-nh=" << val_16b << ":" << val_32b;

                break;

            case EXT_COMMON_OSPF_ROUTER_ID :
                if (isGlobalIPv4)
                    val_ss << "ospf-rid=" << ipv4_char << ":" << val_16b;
                else if (isGlobal4Bytes)
                    val_ss << "ospf-rid=" << val_32b << ":" << val_16b;
                else
                    val_ss << "ospf-rid=" << val_16b << ":" << val_32b;
                break;

            default :
                LOG_INFO("%s: Extended community common type %d subtype = %d is not yet supported", peer_addr.c_str(),
                        ec_hdr.high_type, ec_hdr.low_type);
                break;
        }

        return val_ss.str();
    }

    /**
     * Decode Opaque subtypes
     *
     * \details
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     *
     * \return  Decoded string value
     */
    std::string ExtCommunity::decodeType_Opaque(const extcomm_hdr &ec_hdr) {
        std::stringstream   val_ss;
        uint16_t            val_16b;
        uint32_t            val_32b;

        switch(ec_hdr.low_type) {
            case EXT_OPAQUE_COST_COMMUNITY: {
                u_char poi = ec_hdr.value[0];  // Point of Insertion
                u_char cid = ec_hdr.value[1];  // Community-ID
                memcpy(&val_32b, ec_hdr.value + 2, 4);
                bgp::SWAP_BYTES(&val_32b);

                val_ss << "cost=";

                switch (poi) {
                    case 128 : // Absolute_value
                        val_ss << "abs:";
                        break;
                    case 129 : // IGP Cost
                        val_ss << "igp:";
                        break;
                    case 130: // External_Internal
                        val_ss << "ext:";
                        break;
                    case 131: // BGP_ID
                        val_ss << "bgp_id:";
                        break;
                    default:
                        val_ss << "unkn";
                        break;
                }

                val_ss << (int)cid << ":" << val_32b;

                break;
            }

            case EXT_OPAQUE_CP_ORF:
                val_ss << "cp-orf=" << val_16b << ":" << val_32b;
                break;

            case EXT_OPAQUE_OSPF_ROUTE_TYPE: {
                memcpy(&val_32b, ec_hdr.value, 4);
                bgp::SWAP_BYTES(&val_32b);

                val_ss << "ospf-rt=area-" << val_32b << ":";

                // Get the route type
                switch (ec_hdr.value[4]) {
                    case 1: // intra-area routes
                    case 2: // intra-area routes
                        val_ss << "O:";
                        break;
                    case 3: // Inter-area routes
                        val_ss << "IA:";
                        break;
                    case 5: // External routes
                        val_ss << "E:";
                        break;
                    case 7: // NSSA routes
                        val_ss << "N:";
                        break;
                    default:
                        val_ss << "unkn:";
                        break;
                }

                // Add the options
                val_ss << (int)ec_hdr.value[5];

                break;
            }

            case EXT_OPAQUE_COLOR :
                memcpy(&val_32b, ec_hdr.value + 2, 4);
                bgp::SWAP_BYTES(&val_32b);

                val_ss << "color=" << val_32b;
                break;

            case EXT_OPAQUE_ENCAP :
                val_ss << "encap=" << (int)ec_hdr.value[5];
                break;

            case EXT_OPAQUE_DEFAULT_GW : // draft-ietf-l2vpn-evpn (value is zero/reserved)
                val_ss << "default-gw";
                break;
        }

        return val_ss.str();
    }

    /**
     * Decode Generic subtypes
     *
     * \details
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     * \param [in]   isGlobal4Bytes  True if the global admin field is 4 bytes, false if 2
     * \param [in]   isGlobalIPv4    True if the global admin field is an IPv4 address, false if not
     *
     * \return  Decoded string value
     */
    std::string ExtCommunity::decodeType_Generic(const extcomm_hdr &ec_hdr, bool isGlobal4Bytes, bool isGlobalIPv4) {
        std::stringstream   val_ss;
        uint16_t            val_16b;
        uint32_t            val_32b;
        char                ipv4_char[16] = {0};

        /*
         * Decode values based on bit size
         */
        if (isGlobal4Bytes) {
            // Four-byte global field
            memcpy(&val_32b, ec_hdr.value, 4);
            memcpy(&val_16b, ec_hdr.value + 4, 2);

            bgp::SWAP_BYTES(&val_16b);

            if (isGlobalIPv4) {
                inet_ntop(AF_INET, &val_32b, ipv4_char, sizeof(ipv4_char));
            } else
                bgp::SWAP_BYTES(&val_32b);

        } else {
            // Two-byte global field
            memcpy(&val_16b, ec_hdr.value, 2);
            memcpy(&val_32b, ec_hdr.value + 2, 4);

            // Chagne to host order
            bgp::SWAP_BYTES(&val_16b);
            bgp::SWAP_BYTES(&val_32b);
        }

        switch (ec_hdr.low_type) {
            case EXT_GENERIC_OSPF_ROUTE_TYPE :  // deprecated
            case EXT_GENERIC_OSPF_ROUTER_ID :   // deprecated
            case EXT_GENERIC_OSPF_DOM_ID :      // deprecated
                LOG_INFO("%s: Ignoring deprecated extended community %d/%d", peer_addr.c_str(),
                        ec_hdr.high_type, ec_hdr.low_type);
                break;

            case EXT_GENERIC_LAYER2_INFO : {    // rfc4761
                u_char encap_type    = ec_hdr.value[0];
                u_char ctrl_flags   = ec_hdr.value[1];
                memcpy(&val_16b, ec_hdr.value + 2, 2);          // Layer 2 MTU
                bgp::SWAP_BYTES(&val_16b);

                val_ss << "l2info=";

                switch (encap_type) {
                    case 19 : // VPLS
                        val_ss << "vpls:";
                        break;

                    default:
                        val_ss << (int) encap_type << ":";
                        break;
                }

                val_ss << ctrl_flags << ":mtu:" << val_16b;
                break;
            }

            case EXT_GENERIC_FLOWSPEC_TRAFFIC_RATE : {
                // 4 byte float
                // TODO: would prefer to use std::defaultfloat, but this is not available in centos6.5 gcc
                val_ss << "flow-rate=" << val_16b << ":" << (float) val_32b;

                break;
            }

            case EXT_GENERIC_FLOWSPEC_TRAFFIC_ACTION : {
                val_ss << "flow-act=";

                // TODO: need to validate if byte 0 or 5, using 5 here
                if (ec_hdr.value[5] & 0x02)             // Terminal action
                    val_ss << "S";

                if (ec_hdr.value[5] & 0x01)             // Sample and logging enabled
                    val_ss << "T";

                break;
            }

            case EXT_GENERIC_FLOWSPEC_REDIRECT : {
                val_ss << "flow-redir=";

                // Route target
                if (isGlobalIPv4)
                    val_ss << ipv4_char << ":" << val_16b;

                else if (isGlobal4Bytes)
                    val_ss << val_32b << ":" << val_16b;

                else
                    val_ss << val_16b << ":" << val_32b;
                break;
            }

            case EXT_GENERIC_FLOWSPEC_TRAFFIC_REMARK :
                val_ss << "flow-remark=" << (int)ec_hdr.value[5];
        }

        return val_ss.str();
    }

    /**
     * Parse the extended communities path attribute (20 byte as per RFC5701)
     *
     * \details
     *     Will parse the EXTENDED COMMUNITIES data passed. Parsed data will be stored
     *     in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void ExtCommunity::parsev6ExtCommunities(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_update_data &parsed_data) {
        std::string decodeStr = "";
        extcomm_hdr ec_hdr;

        LOG_INFO("%s: Parsing IPv6 extended community len=%d", peer_addr.c_str(), attr_len);

        if ( (attr_len % 20) ) {
            LOG_NOTICE("%s: Parsing IPv6 extended community len=%d is invalid, expecting divisible by 20", peer_addr.c_str(), attr_len);
            return;
        }

        /*
         * Loop through consecutive entries
         */
        for (int i = 0; i < attr_len; i += 20) {
            // Setup extended community header
            ec_hdr.high_type = data[0];
            ec_hdr.low_type = data[1];
            ec_hdr.value = data + 2;

            /*
             * Docode the community by type
             */
            switch (ec_hdr.high_type << 2 >> 2) {
                case 0 :  // Currently IPv6 specific uses this type field
                    decodeStr.append(decodeType_IPv6Specific(ec_hdr));
                    break;

                default :
                    LOG_NOTICE("%s: Unexpected type for IPv6 %d,%d", peer_addr.c_str(),
                            ec_hdr.high_type, ec_hdr.low_type);
                    break;
            }
        }
    }


    /**
     * Decode IPv6 Specific Type/Subtypes
     *
     * \details
     *      Decodes the IPv6 specific and 2-octet, 4-octet.  This is pretty much the as common for IPv4,
     *      but with some differences. Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     *
     * \return  Decoded string value
     */
    std::string ExtCommunity::decodeType_IPv6Specific(const extcomm_hdr &ec_hdr) {
        std::stringstream   val_ss;
        uint16_t            val_16b;
        u_char              ipv6_raw[16] = {0};
        char                ipv6_char[40] = {0};

        memcpy(ipv6_raw, ec_hdr.value, 16);
        if (inet_ntop(AF_INET6, ipv6_raw, ipv6_char, sizeof(ipv6_char)) != NULL)
            return "";

        memcpy(&val_16b, ec_hdr.value + 16, 2);
        bgp::SWAP_BYTES(&val_16b);

        switch (ec_hdr.low_type) {

            case EXT_IPV6_ROUTE_ORIGIN :
                val_ss << "soo=" << ipv6_char << ":" << val_16b;
                break;

            case EXT_IPV6_ROUTE_TARGET :
                val_ss << "rt=" << ipv6_char << ":" << val_16b;
                break;

            case EXT_IPV6_CISCO_VPN_ID :
                    val_ss << "vpn-id=" << ipv6_char << ":0x" << std::hex << val_16b;

                break;

            case EXT_IPV6_VRF_IMPORT :
                val_ss << "import=" << ipv6_char << ":" << val_16b;

                break;

            case EXT_IPV6_IA_P2MP_SEG_NH :
                val_ss << "p2mp-nh=" << ipv6_char << ":" << val_16b;

                break;

            default :
                LOG_INFO("%s: Extended community ipv6 specific type %d subtype = %d is not yet supported", peer_addr.c_str(),
                        ec_hdr.high_type, ec_hdr.low_type);
                break;
        }

        return val_ss.str();
    }
}
