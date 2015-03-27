/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <arpa/inet.h>

#include "MPLinkStateAttr.h"

namespace bgp_msg {
    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     peerAddr     Printed form of peer address used for logging
     * \param [out]    parsed_data  Reference to parsed_update_data; will be updated with all parsed data
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    MPLinkStateAttr::MPLinkStateAttr(Logger *logPtr, std::string peerAddr,
            UpdateMsg::parsed_update_data *parsed_data, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        peer_addr = peerAddr;
        this->parsed_data = parsed_data;
    }

    MPLinkStateAttr::~MPLinkStateAttr() {
    }


    /**
     * Parse Link State attribute
     *
     * \details Will handle parsing the link state attributes
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     */
    void MPLinkStateAttr::parseAttrLinkState(int attr_len, u_char *data) {

        /*
         * Loop through all TLV's for the attribute
         */
        int tlv_len;
        while (attr_len > 0) {
            tlv_len = parseAttrLinkStateTLV(attr_len, data);
            attr_len -= tlv_len;

            if (attr_len > 0);
                data += tlv_len;
        }
    }

    /*******************************************************************************//*
     * Parse Link State attribute TLV
     *
     * \details Will handle parsing the link state attribute
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     *
     * \returns length of the TLV attribute parsed (including the tlv header lenght)
     */
    int MPLinkStateAttr::parseAttrLinkStateTLV(int attr_len, u_char *data) {
        uint16_t        type;
        uint16_t        len;

        char            ip_char[46];
        uint32_t        value_32bit;


        if (attr_len < 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse attribute; too short",
                    peer_addr.c_str());
            return len + 4;
        }

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);

        memcpy(&len, data+2, 2);
        bgp::SWAP_BYTES(&len);

        data += 4;

        switch (type) {
            case ATTR_NODE_FLAG:
                //SELF_DEBUG("%s: bgp-ls: parsing node flag attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: node flag attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_NODE_IPV4_ROUTER_ID_LOCAL:  // Includes ATTR_LINK_IPV4_ROUTER_ID_LOCAL
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute local router id IPv4 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                memcpy(parsed_data->ls_attrs[ATTR_NODE_IPV4_ROUTER_ID_LOCAL].data(), data, 4);
                inet_ntop(AF_INET, parsed_data->ls_attrs[ATTR_NODE_IPV4_ROUTER_ID_LOCAL].data(), ip_char, sizeof(ip_char));

                SELF_DEBUG("%s: bgp-ls: parsed local IPv4 router id attribute: addr = %s", peer_addr.c_str(), ip_char);
                break;

            case ATTR_NODE_IPV6_ROUTER_ID_LOCAL: // Includes ATTR_LINK_IPV6_ROUTER_ID_LOCAL
                if (len != 16) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute local router id IPv6 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                memcpy(parsed_data->ls_attrs[ATTR_NODE_IPV6_ROUTER_ID_LOCAL].data(), data, 16);
                inet_ntop(AF_INET6, parsed_data->ls_attrs[ATTR_NODE_IPV6_ROUTER_ID_LOCAL].data(), ip_char, sizeof(ip_char));

                SELF_DEBUG("%s: bgp-ls: parsed local IPv6 router id attribute: addr = %s", peer_addr.c_str(), ip_char);
                break;

            case ATTR_NODE_ISIS_AREA_ID:
                if (len <= 4) {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit);
                    value_32bit >>= 8;
                    memcpy(parsed_data->ls_attrs[ATTR_NODE_ISIS_AREA_ID].data(), &value_32bit, sizeof(value_32bit));
                }

                SELF_DEBUG("%s: bgp-ls: parsed node ISIS area id %x (len=%d)", peer_addr.c_str(), value_32bit, len);
                break;

            case ATTR_NODE_MT_ID:
                //SELF_DEBUG("%s: bgp-ls: parsing node MT ID attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: node MT ID attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_NODE_NAME:
                parsed_data->ls_attrs[ATTR_NODE_NAME].fill(0);
                strncpy((char *)parsed_data->ls_attrs[ATTR_NODE_NAME].data(), (char *)data, len);

                SELF_DEBUG("%s: bgp-ls: parsed node name attribute: name = %s", peer_addr.c_str(),
                           parsed_data->ls_attrs[ATTR_NODE_NAME].data());
                break;

            case ATTR_NODE_OPAQUE:
                break;

            case ATTR_LINK_ADMIN_GROUP:
                //SELF_DEBUG("%s: bgp-ls: parsing link admin group attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: node MT ID attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_IGP_METRIC:
                if (len <= 4) {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit, len);
                }

                memcpy(parsed_data->ls_attrs[ATTR_LINK_IGP_METRIC].data(), &value_32bit, 4);

                SELF_DEBUG("%s: bgp-ls: parsed link IGP metric attribute: metric = %u", peer_addr.c_str(), value_32bit);
                break;

            case ATTR_LINK_IPV4_ROUTER_ID_REMOTE:
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute remote IPv4 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                memcpy(parsed_data->ls_attrs[ATTR_LINK_IPV4_ROUTER_ID_REMOTE].data(), data, 4);
                inet_ntop(AF_INET, parsed_data->ls_attrs[ATTR_LINK_IPV4_ROUTER_ID_REMOTE].data(), ip_char, sizeof(ip_char));

                SELF_DEBUG("%s: bgp-ls: parsed remote IPv4 router id attribute: addr = %s", peer_addr.c_str(), ip_char);
                break;

            case ATTR_LINK_IPV6_ROUTER_ID_REMOTE:
                if (len != 16) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute local router id IPv6 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                memcpy(parsed_data->ls_attrs[ATTR_LINK_IPV6_ROUTER_ID_REMOTE].data(), data, 16);
                inet_ntop(AF_INET6, parsed_data->ls_attrs[ATTR_LINK_IPV6_ROUTER_ID_REMOTE].data(), ip_char, sizeof(ip_char));

                SELF_DEBUG("%s: bgp-ls: parsed local IPv6 router id attribute: addr = %s", peer_addr.c_str(), ip_char);
                break;

            case ATTR_LINK_MAX_LINK_BW:
                //SELF_DEBUG("%s: bgp-ls: parsing link max link bw attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link max link bw attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_MAX_RESV_BW:
                //SELF_DEBUG("%s: bgp-ls: parsing link max reserve bw attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link max link bw attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_MPLS_PROTO_MASK:
                //SELF_DEBUG("%s: bgp-ls: parsing link MPLS Protocol mask attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link MPLS Protocol mask attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_PROTECTION_TYPE:
                //SELF_DEBUG("%s: bgp-ls: parsing link protection type attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link protection type attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_NAME: {
                parsed_data->ls_attrs[ATTR_LINK_NAME].fill(0);
                strncpy((char *)parsed_data->ls_attrs[ATTR_LINK_NAME].data(), (char *)data, len);

                SELF_DEBUG("%s: bgp-ls: parsing link name attribute: name = %s", peer_addr.c_str(), parsed_data->ls_attrs[ATTR_LINK_NAME].data());
                break;
            }
            case ATTR_LINK_SRLG:
                //SELF_DEBUG("%s: bgp-ls: parsing link SRLG attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link SRLG attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_TE_DEF_METRIC:
                //SELF_DEBUG("%s: bgp-ls: parsing link TE default metric attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link TE default metric attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_UNRESV_BW:
                //SELF_DEBUG("%s: bgp-ls: parsing link unreserve bw attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link unreserve bw attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_OPAQUE:
                break;


            case ATTR_PREFIX_EXTEND_TAG:
                //SELF_DEBUG("%s: bgp-ls: parsing prefix extended tag attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix extended tag attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_IGP_FLAGS:
                //SELF_DEBUG("%s: bgp-ls: parsing prefix IGP flags attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix IGP flags attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_PREFIX_METRIC:
                if (len <= 4) {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit, len);
                }

                memcpy(parsed_data->ls_attrs[ATTR_PREFIX_PREFIX_METRIC].data(), &value_32bit, 4);
                SELF_DEBUG("%s: bgp-ls: parsing prefix metric attribute: metric = %u", peer_addr.c_str(), value_32bit);
                break;

            case ATTR_PREFIX_ROUTE_TAG:
                //SELF_DEBUG("%s: bgp-ls: parsing prefix route tag attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix route tag attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_OSPF_FWD_ADDR:
                //SELF_DEBUG("%s: bgp-ls: parsing prefix OSPF forwarding address attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix OSPF forwarding address attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_OPAQUE_PREFIX:
                break;

            default:
                LOG_INFO("%s: bgp-ls: Attribute type=%d len=%d not yet implemented, skipping",
                        peer_addr.c_str(), type, len);
        }

        return len + 4;
    }
}
