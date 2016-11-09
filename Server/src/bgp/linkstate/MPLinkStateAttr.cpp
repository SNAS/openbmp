/*
 * Copyright (c) 2015-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <arpa/inet.h>
#include <string>

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

            if (attr_len > 0)
                data += tlv_len;
        }
    }

    uint32_t MPLinkStateAttr::ieee_float_to_kbps(int32_t float_val) {
        int32_t sign, exponent, mantissa;
        int64_t bits_value = 0;

        sign = float_val & IEEE_SIGN_MASK;
        exponent = float_val & IEEE_EXPONENT_MASK;
        mantissa = float_val & IEEE_MANTISSA_MASK;

        if ((float_val & ~IEEE_SIGN_MASK) == 0) {
            /* Number is zero, unnormalized, or not-a-float_val. */
            return 0;
        }

        if (IEEE_INFINITY == exponent) {
            /* Number is positive or negative infinity, or a special value. */
            return (sign ? MINUS_INFINITY : PLUS_INFINITY);
        }

        exponent = (exponent >> IEEE_MANTISSA_WIDTH) - IEEE_BIAS;
        if (exponent < 0) {
             /* Number is between zero and one. */
             return 0;
        }

        mantissa |= IEEE_IMPLIED_BIT;

        bits_value = mantissa;

        if (exponent <= IEEE_MANTISSA_WIDTH) {
           bits_value >>= IEEE_MANTISSA_WIDTH - exponent;
        } else {
           bits_value <<= exponent - IEEE_MANTISSA_WIDTH;
        }

        // Change sign
        if (sign)
            bits_value *= -1;

        bits_value *= 8;        // to bits
        bits_value /= 1000;     // to kbits

        return bits_value;
    }
    
    /*******************************************************************************//*
     * Parse flags to string
     *
     * \details   Will parse flags from binary representation to string. 
     *            Example: flags "std::array<std::string, 8>{'I', '\0', 'H'}"" 
     *            means data "11100000" would be parsed as "IH"
     *
     * \param [in]   flags          Array of flags
     * \param [in]   data           Pointer to the attribute data
     *
     * \returns string with flags
     */
    std::string MPLinkStateAttr::parse_flags_to_string(u_char data, const std::array<std::string, 8> flags){
        std::string flags_string = "";
        u_char current_mask = 0x80;
        for (std::array<std::string, 8>::const_iterator iterator = flags.begin(); iterator != flags.end(); ++iterator) {
            if(current_mask & data) {
                flags_string += *iterator;
            }
            current_mask = current_mask >> 1;
        }
        return flags_string;
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
        uint16_t            type;
        uint16_t            len;
        char                ip_char[46];
        uint32_t            value_32bit;
        uint16_t            value_16bit;
        int32_t             float_val;
        std::stringstream   val_ss;


        if (attr_len < 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse attribute; too short",
                    peer_addr.c_str());
            return attr_len;
        }

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);

        memcpy(&len, data+2, 2);
        bgp::SWAP_BYTES(&len);

        data += 4;

        switch (type) {
            case ATTR_NODE_FLAG: {
                if (len != 1) {
                    LOG_INFO("%s: bgp-ls: node flag attribute length is too long %d should be 1",
                             peer_addr.c_str(), len);
                }

                std::string flags = this->parse_flags_to_string(*data, node_flags);

                SELF_DEBUG("%s: bgp-ls: parsed node flags %s %x (len=%d)", peer_addr.c_str(), flags.c_str(), *data, len);

                parsed_data->ls_attrs[ATTR_NODE_FLAG].fill(0);
                strncpy((char *)parsed_data->ls_attrs[ATTR_NODE_FLAG].data(), flags.c_str(), flags.size());
            }
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

            case ATTR_NODE_IPV6_ROUTER_ID_LOCAL:  // Includes ATTR_LINK_IPV6_ROUTER_ID_LOCAL
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
                if (len <= 8)
                    memcpy(parsed_data->ls_attrs[ATTR_NODE_ISIS_AREA_ID].data(), data, len);
                parsed_data->ls_attrs[ATTR_NODE_ISIS_AREA_ID].data()[8] = len;

                SELF_DEBUG("%s: bgp-ls: parsed node ISIS area id %x (len=%d)", peer_addr.c_str(), value_32bit, len);
                break;

            case ATTR_NODE_MT_ID:
                SELF_DEBUG("%s: bgp-ls: parsing node MT ID attribute (len=%d)", peer_addr.c_str(), len);

                val_ss.str(std::string());  // Clear

                for (int i=0; i < len; i += 2) {
                    value_16bit = 0;
                    memcpy(&value_16bit, data, 2);
                    bgp::SWAP_BYTES(&value_16bit);
                    data += 2;

                    if (!i)
                        val_ss << value_16bit;
                    else
                        val_ss << ", " << value_16bit;
                }

                memcpy(parsed_data->ls_attrs[ATTR_NODE_MT_ID].data(), val_ss.str().data(), val_ss.str().length());
                // LOG_INFO("%s: bgp-ls: parsed node MT_ID %s (len=%d)", peer_addr.c_str(), val_ss.str().c_str(), len);

                break;

            case ATTR_NODE_NAME:
                parsed_data->ls_attrs[ATTR_NODE_NAME].fill(0);
                strncpy((char *)parsed_data->ls_attrs[ATTR_NODE_NAME].data(), (char *)data, len);

                SELF_DEBUG("%s: bgp-ls: parsed node name attribute: name = %s", peer_addr.c_str(),
                           parsed_data->ls_attrs[ATTR_NODE_NAME].data());
                break;

            case ATTR_NODE_OPAQUE:
                LOG_INFO("%s: bgp-ls: opaque node attribute (len=%d), not yet implemented", peer_addr.c_str(), len);
                break;

            case ATTR_NODE_SR_CAPABILITIES: {
                val_ss.str(std::string());

                // https://tools.ietf.org/html/draft-ietf-isis-segment-routing-extensions-07#section-3.1                
                val_ss << this->parse_flags_to_string(*data, node_sr_capabilities_flags);
                
                // 1 byte reserved (skipping) + 1 byte flags (already parsed)
                data += 2;

                // 3 bytes for Range
                memcpy(&value_32bit, data, 3);
                bgp::SWAP_BYTES(&value_32bit);
                data += 3;
                value_32bit = value_32bit >> 8;

                val_ss << " " << value_32bit;

                u_int16_t type;
                memcpy(&type, data, 2);
                bgp::SWAP_BYTES(&type);
                data += 2;

                val_ss << " " << type;

                // Parsing SID/Label Sub-TLV: https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-03#section-2.3.7.2

                if (type == SUB_TLV_SID_LABEL) {
                    u_int16_t sid_label_size = 0;
                    memcpy(&sid_label_size, data, 2);
                    bgp::SWAP_BYTES(&sid_label_size);
                    data += 2;

                    if (sid_label_size == 3 || sid_label_size == 4) {
                        memcpy(&value_32bit, data, sid_label_size);
                        bgp::SWAP_BYTES(&value_32bit);

                        if (sid_label_size == 3) {
                            value_32bit = value_32bit >> 8;
                        }

                        val_ss << " " << value_32bit;
                    } else {
                        LOG_NOTICE("%s: bgp-ls: parsed node sr capabilities, sid label size is unexpected",
                                peer_addr.c_str());
                    }
                } else {
                    LOG_NOTICE("%s: bgp-ls: parsed node sr capabilities, SUB TLV type is unexpected",
                            peer_addr.c_str());
                }

                SELF_DEBUG("%s: bgp-ls: parsed node sr capabilities %s", peer_addr.c_str(), val_ss.str().data());

                memcpy(parsed_data->ls_attrs[ATTR_NODE_SR_CAPABILITIES].data(), val_ss.str().data(), val_ss.str().length());
                break;
            }

            case ATTR_LINK_ADMIN_GROUP:
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute link admin group sub-tlv, size not 4",
                            peer_addr.c_str());
                    break;
                } else {
                    value_32bit = 0;
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit, len);
                    memcpy(parsed_data->ls_attrs[ATTR_LINK_ADMIN_GROUP].data(), &value_32bit, 4);
                    SELF_DEBUG("%s: bgp-ls: parsed linked admin group attribute: "
                               " 0x%x, len = %d",
                               peer_addr.c_str(), value_32bit, len);
                }
                break;

            case ATTR_LINK_IGP_METRIC:
                if (len <= 4) {
                    value_32bit = 0;
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
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute remote router id IPv6 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                memcpy(parsed_data->ls_attrs[ATTR_LINK_IPV6_ROUTER_ID_REMOTE].data(), data, 16);
                inet_ntop(AF_INET6, parsed_data->ls_attrs[ATTR_LINK_IPV6_ROUTER_ID_REMOTE].data(), ip_char, sizeof(ip_char));

                SELF_DEBUG("%s: bgp-ls: parsed remote IPv6 router id attribute: addr = %s", peer_addr.c_str(), ip_char);
                break;

            case ATTR_LINK_MAX_LINK_BW:
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute maximum link bandwidth sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }

                float_val = 0;
                memcpy(&float_val, data, len);
                bgp::SWAP_BYTES(&float_val, len);
                float_val = ieee_float_to_kbps(float_val);
                memcpy(parsed_data->ls_attrs[ATTR_LINK_MAX_LINK_BW].data(), &float_val, 4);

                memcpy(&value_32bit, data, 4);
                bgp::SWAP_BYTES(&value_32bit);
                SELF_DEBUG("%s: bgp-ls: parsed attribute maximum link bandwidth (raw=%x) %u Kbits (len=%d)",
                           peer_addr.c_str(), value_32bit, *(int32_t *)&float_val, len);
                break;

            case ATTR_LINK_MAX_RESV_BW:
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute remote IPv4 sub-tlv; too short",
                            peer_addr.c_str());
                    break;
                }
                float_val = 0;
                memcpy(&float_val, data, len);
                bgp::SWAP_BYTES(&float_val, len);
                float_val = ieee_float_to_kbps(float_val);
                memcpy(parsed_data->ls_attrs[ATTR_LINK_MAX_RESV_BW].data(), &float_val, 4);
                SELF_DEBUG("%s: bgp-ls: parsed attribute maximum reserved bandwidth %u Kbits (len=%d)",
                    peer_addr.c_str(), *(uint32_t *)&float_val, len);
                break;

            case ATTR_LINK_MPLS_PROTO_MASK:
                // SELF_DEBUG("%s: bgp-ls: parsing link MPLS Protocol mask attribute", peer_ad dr.c_str());
                LOG_INFO("%s: bgp-ls: link MPLS Protocol mask attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_PROTECTION_TYPE:
                // SELF_DEBUG("%s: bgp-ls: parsing link protection type attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link protection type attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_NAME: {
                parsed_data->ls_attrs[ATTR_LINK_NAME].fill(0);
                strncpy((char *)parsed_data->ls_attrs[ATTR_LINK_NAME].data(), (char *)data, len);

                SELF_DEBUG("%s: bgp-ls: parsing link name attribute: name = %s",
                    peer_addr.c_str(), parsed_data->ls_attrs[ATTR_LINK_NAME].data());
                break;
            }
            
            case ATTR_LINK_PEER_AJD_SID: {
                val_ss.str(std::string());
                
                u_char flags;
                memcpy(&flags, data, sizeof(flags));
                
                // Parsing flags:
                // https://tools.ietf.org/html/draft-ietf-isis-segment-routing-extensions-05#section-2.1

                val_ss << this->parse_flags_to_string(*data, peer_adj_sid_flags);
                data += 1;
                
                u_int8_t weight;
                memcpy(&weight, data, 1);
                
                val_ss << " " << (int)weight;
                
                // 1 byte for Weight + 2 bytes for Reserved
                data += 3;
                
                memcpy(&value_32bit, data, 3);
                bgp::SWAP_BYTES(&value_32bit);
                data += 3;
                
                value_32bit = value_32bit >> 8;

                val_ss << " " << value_32bit;
                
                SELF_DEBUG("%s: bgp-ls: parsed link adjency segment identifier %s", peer_addr.c_str(), val_ss.str().data());

                memcpy(parsed_data->ls_attrs[ATTR_LINK_PEER_AJD_SID].data(), val_ss.str().data(), val_ss.str().length());
                break;
            }

            case ATTR_LINK_SRLG:
                // SELF_DEBUG("%s: bgp-ls: parsing link SRLG attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: link SRLG attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_LINK_TE_DEF_METRIC:
                value_32bit = 0;

                // Per rfc7752 Section 3.3.2.3, this is supposed to be 4 bytes, but some implementations have this <=4.

                if (len == 0) {
                    memcpy(parsed_data->ls_attrs[ATTR_LINK_TE_DEF_METRIC].data(), &value_32bit, len);
                    break;
                } else if (len > 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse attribute TE default metric sub-tlv; too long %d",
                            peer_addr.c_str(), len);
                    break;
                } else {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit, len);
                    memcpy(parsed_data->ls_attrs[ATTR_LINK_TE_DEF_METRIC].data(), &value_32bit, len);
                    SELF_DEBUG("%s: bgp-ls: parsed attribute te default metric 0x%X (len=%d)", peer_addr.c_str(),
                               value_32bit, len);
                }

                break;

            case ATTR_LINK_UNRESV_BW: {
                std::stringstream   val_ss;

                SELF_DEBUG("%s: bgp-ls: parsing link unreserve bw attribute (len=%d)", peer_addr.c_str(), len);

                if (len != 32) {
                    LOG_INFO("%s: bgp-ls: link unreserve bw attribute is invalid, length is %d but should be 32",
                             peer_addr.c_str(), len);
                    break;
                }

                val_ss.str(std::string());  // Clear

                for (int i=0; i < 32; i += 4) {
                    float_val = 0;
                    memcpy(&float_val, data, 4);
                    bgp::SWAP_BYTES(&float_val);
                    float_val = ieee_float_to_kbps(float_val);

                    data += 4;

                    if (!i)
                        val_ss << float_val;
                    else
                        val_ss << ", " << float_val;
                }

                SELF_DEBUG("%s: bgp-ls: parsed unresvered bandwidth: %s", peer_addr.c_str(), val_ss.str().c_str());

                memcpy(parsed_data->ls_attrs[ATTR_LINK_UNRESV_BW].data(), val_ss.str().data(), val_ss.str().length());

                break;
            }

            case ATTR_LINK_OPAQUE:
                LOG_INFO("%s: bgp-ls: opaque link attribute (len=%d), not yet implemented", peer_addr.c_str(), len);
                break;

            case ATTR_LINK_PEER_NODE_SID:
                val_ss.str(std::string());
                value_32bit = 0;

                /*
                 * Syntax of value is: [L] <weight> <sid value>
                 *
                 *      L flag indicates locally significant
                 */

                if (*data & 0x80 and *data & 0x40 and len == 7) {

                    // 3-octet -  20 rightmost bits are used for encoding the label value.
                    memcpy(&value_32bit, (data+4), 3);
                    bgp::SWAP_BYTES(&value_32bit, 3);

                    val_ss << "L " << (int)*(data + 1) << " " << value_32bit;
                } else if (len >= 20) {
                    // 16-octet - IPv6 address
                    if (*data & 0x40)
                        val_ss << "L ";

                    inet_ntop(AF_INET6, (data + 4), ip_char, sizeof(ip_char));

                    val_ss << (int) *(data + 1) << " " << ip_char;
                } else if (len == 8) {
                    // 4-octet encoded offset in the SID/Label space advertised by this router using the encodings
                    memcpy(&value_32bit, (data+4), 4);
                    bgp::SWAP_BYTES(&value_32bit);

                    val_ss << (int) *(data + 1) << " " << value_32bit;
                } else {
                    LOG_WARN("%s: bgp-ls: Peer Node SID attribute has unexpected length of %d", peer_addr.c_str(), len);
                    break;
                }

                SELF_DEBUG("%s: bgp-ls: parsed link peer node SID: %s (len=%d) %d/%x", peer_addr.c_str(),
                           val_ss.str().c_str(), len, value_32bit, (data+4));

                memcpy(parsed_data->ls_attrs[ATTR_LINK_PEER_NODE_SID].data(), val_ss.str().data(), val_ss.str().length());
                break;

            case ATTR_LINK_PEER_SET_SID:
                LOG_INFO("%s: bgp-ls: peer set SID link attribute (len=%d), not yet implemented", peer_addr.c_str(), len);
                break;

            case ATTR_PREFIX_EXTEND_TAG:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix extended tag attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix extended tag attribute (len=%d), not yet implemented",
                         peer_addr.c_str(), len);
                break;

            case ATTR_PREFIX_IGP_FLAGS:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix IGP flags attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix IGP flags attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_PREFIX_METRIC:
                value_32bit = 0;
                if (len <= 4) {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit, len);
                }

                memcpy(parsed_data->ls_attrs[ATTR_PREFIX_PREFIX_METRIC].data(), &value_32bit, 4);
                SELF_DEBUG("%s: bgp-ls: parsing prefix metric attribute: metric = %u", peer_addr.c_str(), value_32bit);
                break;

            case ATTR_PREFIX_ROUTE_TAG:
            {
                SELF_DEBUG("%s: bgp-ls: parsing prefix route tag attribute (len=%d)", peer_addr.c_str(), len);

                // TODO(undefined): Per RFC7752 section 3.3.3, prefix tag can be multiples, but for now we only decode the first one.
                value_32bit = 0;

                if (len == 4) {
                    memcpy(&value_32bit, data, len);
                    bgp::SWAP_BYTES(&value_32bit);

                    memcpy(parsed_data->ls_attrs[ATTR_PREFIX_ROUTE_TAG].data(), &value_32bit, 4);
//                    SELF_DEBUG("%s: bgp-ls: parsing prefix route tag attribute %d (len=%d)", peer_addr.c_str(),
//                             value_32bit, len);
                }

                break;
            }
            case ATTR_PREFIX_OSPF_FWD_ADDR:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix OSPF forwarding address attribute", peer_addr.c_str());
                LOG_INFO("%s: bgp-ls: prefix OSPF forwarding address attribute, not yet implemented", peer_addr.c_str());
                break;

            case ATTR_PREFIX_OPAQUE_PREFIX:
                LOG_INFO("%s: bgp-ls: opaque prefix attribute (len=%d), not yet implemented", peer_addr.c_str(), len);
                break;
                
            case ATTR_PREFIX_SID_TLV: {
                val_ss.str(std::string());
                value_32bit = 0;
                
                // Package structure:
                // https://tools.ietf.org/html/draft-ietf-ospf-segment-routing-extensions-09#section-5
                
                val_ss << this->parse_flags_to_string(*data, sid_sub_tlv_flags);
                
                // 1 byte for Flags + 1 byte for reserved
                data += 2;
                
                u_int8_t mt_id = 0;
                memcpy(&mt_id, data, 1);
                data += 1;
                val_ss << " " << (int)mt_id;
                
                u_int8_t algorythm = 0;
                memcpy(&algorythm, data, 1);
                data += 1;
                val_ss << " " << (int)algorythm;
                
                // 4 bytes already read, looking for SID/Index/Label
                if (len - 4 <= 4) {
                    memcpy(&value_32bit, data, len - 4);
                    bgp::SWAP_BYTES(&value_32bit);
                    val_ss << " " << value_32bit;
                } else {
                    LOG_WARN("%s: bgp-ls: Prefix SID/Index/Label has unexpected length: %d", peer_addr.c_str(), len - 4);
                    break;
                }
                
                memcpy(parsed_data->ls_attrs[ATTR_PREFIX_SID_TLV].data(), val_ss.str().data(), val_ss.str().length());
                break;
            }

            default:
                LOG_INFO("%s: bgp-ls: Attribute type=%d len=%d not yet implemented, skipping",
                        peer_addr.c_str(), type, len);
        }

        return len + 4;
    }
}  // namespace bgp_msg
