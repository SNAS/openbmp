/*
* Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*
*/

#include <arpa/inet.h>

#include "parseBgpLibMpLinkstateAttr.h"
#include "parseBgpLibMpLinkstate.h"

#include <arpa/inet.h>
#include <string>

namespace parse_bgp_lib {
    /* BGP-LS Node flags : https://tools.ietf.org/html/rfc7752#section-3.3.1.1
     *
     * +-----------------+-------------------------+------------+
     * |        Bit       | Description             | Reference  |
     * +-----------------+-------------------------+------------+
     * |       'O'       | Overload Bit            | [ISO10589] |
     * |       'T'       | Attached Bit            | [ISO10589] |
     * |       'E'       | External Bit            | [RFC2328]  |
     * |       'B'       | ABR Bit                 | [RFC2328]  |
     * |       'R'       | Router Bit              | [RFC5340]  |
     * |       'V'       | V6 Bit                  | [RFC5340]  |
     * | Reserved (Rsvd) | Reserved for future use |            |
     * +-----------------+-------------------------+------------+
     */
    const char * const MPLinkStateAttr::LS_FLAGS_NODE_NLRI[] = {
            "O", "T", "E", "B", "R", "V"
    };

    /* https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.2.1
     *      ISIS: https://tools.ietf.org/html/draft-ietf-isis-segment-routing-extensions-09#section-2.2.1
     *      OSPF: https://tools.ietf.org/html/draft-ietf-ospf-segment-routing-extensions-10#section-7.1
     *            https://tools.ietf.org/html/draft-ietf-ospf-ospfv3-segment-routing-extensions-07#section-7.1
     */
    const char * const MPLinkStateAttr::LS_FLAGS_PEER_ADJ_SID_ISIS[] = {
            "F",            // Address family flag; unset adj is IPv4, set adj is IPv6
            "B",            // Backup flag; set if adj is eligible for protection
            "V",            // Value flag; set = sid carries a value, default is set
            "L",            // Local flag; set = value has local significance
            "S"             // Set flag; set = SID refers to a set of adjacencies
    };

    // Currently ospfv3 is the same except that G is S, but means the same thing
    const char * const MPLinkStateAttr::LS_FLAGS_PEER_ADJ_SID_OSPF[] = {
            "B",            // Backup flag; set if adj is eligible for protection
            "V",            // Value flag; set = sid carries a value, default is set
            "L",            // Local flag; set = value has local significance
            "G"             // Group flag; set = sid referes to a group of adjacencies
    };

    /* https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.1.1
     *
     *      ISIS: https://tools.ietf.org/html/draft-ietf-isis-segment-routing-extensions-09#section-3.1
     *      OSPF: https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#ref-I-D.ietf-ospf-ospfv3-segment-routing-extensions
     *
     */
    const char * const MPLinkStateAttr::LS_FLAGS_SR_CAP_ISIS[] = {
            "I",            // MPLS IPv4 flag; set = router is capable of SR MPLS encaps IPv4 all interfaces
            "V",            // MPLS IPv6 flag; set = router is capable of SR MPLS encaps IPv6 all interfaces
            "H"             // SR-IPv6 flag; set = rouer is capable of IPv6 SR header on all interfaces defined in ...
    };


    /* https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.3.1
     *      ISIS: https://tools.ietf.org/html/draft-ietf-isis-segment-routing-extensions-09#section-2.1.1
     *      OSPF: https://tools.ietf.org/html/draft-ietf-ospf-segment-routing-extensions-10#section-5
     */
    const char * const MPLinkStateAttr::LS_FLAGS_PREFIX_SID_ISIS[] = {
            "R",            // Re-advertisement flag; set = prefix was redistributed or from l1 to l2
            "N",            // Node-SID flag; set = sid refers to the router
            "P",            // no-PHP flag; set = penultimate hop MUST NOT pop before delivering the packet
            "E",            // Explicit-Null flag; set = upstream neighbor must replace SID with Exp-Null label
            "V",            // Value flag; set = SID carries a value instead of an index; default unset
            "L"             // Local flag; set = value/index has local significance; default is unset
    };

    const char * const MPLinkStateAttr::LS_FLAGS_PREFIX_SID_OSPF[] = {
            "",             // unused
            "NP",           // no-PHP flag; set = penultimate hop MUST NOT pop before delivering the packet
            "M",            // Mapping server flag; set = SID was advertised by mapping server
            "E",            // Explicit-Null flag; set = upstream neighbor must replace SID with Exp-Null label
            "V",            // Value flag; set = SID carries a value instead of an index; default unset
            "L"             // Local flag; set = value/index has local significance; default is unset
    };



    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    MPLinkStateAttr::MPLinkStateAttr(Logger *logPtr, parse_bgp_lib::parseBgpLib::parsed_update *update, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        this->update = update;
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

    /*******************************************************************************//**
     * Parse flags to string
     *
     * \details   Will parse flags from binary representation to string.
     *            Bits are read left to right as documented in RFC/drafts.   Left most
     *            bit == index 0 in array and so on.
     *
     * \param [in]   data             Flags byte
     * \param [in]   flags_array      Array of flags - Array item equals the bit position for flag
     *                                Must have a size of 8 or less.
     * \param [in]   flags_array_len  Length of flags array
     *
     * \returns string with flags
     */
    std::string MPLinkStateAttr::parse_flags_to_string(u_char data, const char * const flags_array[], int flags_array_len) {
        std::string flags_string = "";
        u_char current_mask = 0x80;

        for (int i=0; i < (flags_array_len / sizeof(char *)); i++)  {

            if(current_mask & data) {
                flags_string += flags_array[i];
            }
            current_mask >>= 1;
        }

        return flags_string;
    }

    /*******************************************************************************//**
     * Parse SID/Label value to string
     *
     * \details Parses the SID to index, label, or IPv6 string value
     *
     * \param [in]  data            Raw SID data to be parsed
     * \param [in]  len             Length of the data (min is 3 and max is 16).
     *
     * \returns string value of SID
     */
    std::string MPLinkStateAttr::parse_sid_value(u_char *data, int len) {
        std::stringstream   val_ss;
        uint32_t            value_32bit = 0;
        char                ip_char[46];


        if (len == 3) {
            // 3-octet -  20 rightmost bits are used for encoding the label value.
            memcpy(&value_32bit, data, 3);
            parse_bgp_lib::SWAP_BYTES(&value_32bit, 3);

            val_ss << value_32bit;

        } else if (len >= 16) {

            // 16-octet - IPv6 address
            inet_ntop(AF_INET6, data, ip_char, sizeof(ip_char));

            val_ss << ip_char;

        } else if (len == 4) {
            // 4-octet encoded offset in the SID/Label space advertised by this router using the encodings
            memcpy(&value_32bit, data, 4);
            parse_bgp_lib::SWAP_BYTES(&value_32bit);

            val_ss << value_32bit;

        } else {
            LOG_WARN("bgp-ls: SID/Label has unexpected length of %d", len);
            return "";
        }

        return val_ss.str();
    }


    /*******************************************************************************//**
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
        u_char              ip_raw[16];
        char                isis_area_id[32] = {0};
        uint32_t            value_32bit;
        uint16_t            value_16bit;
        int32_t             float_val;
        std::stringstream   val_ss;
        char    buf[8192];
        int     i;

        if (attr_len < 4) {
            LOG_NOTICE("bgp-ls: failed to parse attribute; too short");
            return attr_len;
        }

        memcpy(&type, data, 2);
        parse_bgp_lib::SWAP_BYTES(&type);

        memcpy(&len, data+2, 2);
        parse_bgp_lib::SWAP_BYTES(&len);

        data += 4;

        switch (type) {
            case ATTR_NODE_FLAG: {
                if (len != 1) {
                    LOG_INFO("bgp-ls: node flag attribute length is too long %d should be 1", len);
                }

                update->attrs[LIB_ATTR_LS_FLAGS].official_type = ATTR_NODE_FLAG;
                update->attrs[LIB_ATTR_LS_FLAGS].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_FLAGS];
                update->attrs[LIB_ATTR_LS_FLAGS].value.push_back(this->parse_flags_to_string(*data, LS_FLAGS_NODE_NLRI, sizeof(LS_FLAGS_NODE_NLRI)));
                SELF_DEBUG("bgp-ls: parsed node flags %s %x (len=%d)", update->attrs[LIB_ATTR_LS_FLAGS].value.front().c_str(), *data, len);

            }
                break;

            case ATTR_NODE_IPV4_ROUTER_ID_LOCAL:  // Includes ATTR_LINK_IPV4_ROUTER_ID_LOCAL
                if (len != 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute local router id IPv4 sub-tlv; too short");
                    break;
                }

                memcpy(ip_raw, data, 4);
                inet_ntop(AF_INET,ip_raw, ip_char, sizeof(ip_char));

                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].official_type = ATTR_NODE_IPV4_ROUTER_ID_LOCAL;
                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4];
                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].value.push_back(ip_char);

                SELF_DEBUG("bgp-ls: parsed local IPv4 router id attribute: addr = %s", ip_char);
                break;

            case ATTR_NODE_IPV6_ROUTER_ID_LOCAL:  // Includes ATTR_LINK_IPV6_ROUTER_ID_LOCAL
                if (len != 16) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute local router id IPv6 sub-tlv; too short");
                    break;
                }

                memcpy(ip_raw, data, 16);
                inet_ntop(AF_INET6,ip_raw, ip_char, sizeof(ip_char));

                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].official_type = ATTR_NODE_IPV6_ROUTER_ID_LOCAL;
                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6];
                update->attrs[LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].value.push_back(ip_char);


                SELF_DEBUG("bgp-ls: parsed local IPv6 router id attribute: addr = %s", ip_char);
                break;

            case ATTR_NODE_ISIS_AREA_ID:
                update->attrs[LIB_ATTR_LS_ISIS_AREA_ID].official_type = ATTR_NODE_ISIS_AREA_ID;
                update->attrs[LIB_ATTR_LS_ISIS_AREA_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_ISIS_AREA_ID];

                if (len <= 8)
                    for (i=0; i < len; i++) {
                        snprintf(buf, sizeof(buf), "%02hhX", data[i]);
                        strcat(isis_area_id, buf);
                        if (i == 0)
                            strcat(isis_area_id, ".");
                    }
                update->attrs[LIB_ATTR_LS_ISIS_AREA_ID].value.push_back(isis_area_id);

                SELF_DEBUG("bgp-ls: parsed node ISIS area id %x (len=%d)", update->attrs[LIB_ATTR_LS_ISIS_AREA_ID].value.front().c_str(), len);
                break;

            case ATTR_NODE_MT_ID:
                SELF_DEBUG("bgp-ls: parsing node MT ID attribute (len=%d)", len);

                update->attrs[LIB_ATTR_LS_MT_ID].official_type = ATTR_NODE_MT_ID;
                update->attrs[LIB_ATTR_LS_MT_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_MT_ID];

                for (int i=0; i < len; i += 2) {
                    val_ss.str(std::string());  // Clear
                    value_16bit = 0;
                    memcpy(&value_16bit, data, 2);
                    parse_bgp_lib::SWAP_BYTES(&value_16bit);
                    data += 2;
                    val_ss << value_16bit;
                    update->attrs[LIB_ATTR_LS_MT_ID].value.push_back(val_ss.str());
                }
                // LOG_INFO("%s: bgp-ls: parsed node MT_ID %s (len=%d)", peer_addr.c_str(), val_ss.str().c_str(), len);
                break;

            case ATTR_NODE_NAME:
                update->attrs[LIB_ATTR_LS_NODE_NAME].official_type = ATTR_NODE_NAME;
                update->attrs[LIB_ATTR_LS_NODE_NAME].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_NODE_NAME];
                update->attrs[LIB_ATTR_LS_NODE_NAME].value.push_back(std::string((char *)data, (char *)(data + len)));

                SELF_DEBUG("bgp-ls: parsed node name attribute: name = %s", update->attrs[LIB_ATTR_LS_NODE_NAME].value.front().c_str());
                break;

            case ATTR_NODE_OPAQUE:
                LOG_INFO("bgp-ls: opaque node attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_NODE_SR_CAPABILITIES: {
                update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].official_type = ATTR_NODE_SR_CAPABILITIES;
                update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_SR_CAPABILITIES_TLV];

                val_ss.str(std::string());

                // https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.1.1
                for (std::list<parseBgpLib::parse_bgp_lib_nlri>::iterator it = update->nlri_list.begin();
                     it != update->nlri_list.end();
                     it++) {
                    if (it->afi == BGP_AFI_BGPLS) {
                        if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "IS-IS") >= 0) {
                            val_ss << this->parse_flags_to_string(*data, LS_FLAGS_SR_CAP_ISIS, sizeof(LS_FLAGS_SR_CAP_ISIS));
                            update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.push_back(val_ss.str());
                        } else if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "OSPF") >= 0) {
                            val_ss << int(*data);   //this->parse_flags_to_string(*data, LS_FLAGS_SR_CAP_OSPF, sizeof(LS_FLAGS_SR_CAP_OSPF));
                            update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.push_back(val_ss.str());
                        }
                        break;
                    }
                }

                // 1 byte reserved (skipping) + 1 byte flags (already parsed)
                data += 2;

                // iterate over each range + sid-tlv
                for (int l=2; l < len; l += 10) {
                    val_ss.str(std::string());

                    if (l >= 12)

                    // 3 bytes for Range
                    value_32bit = 0;
                    memcpy(&value_32bit, data, 3);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit);

                    data += 3;

                    value_32bit = value_32bit >> 8;

                    val_ss << value_32bit;

                    // 2 byte type
                    u_int16_t type;
                    memcpy(&type, data, 2);
                    parse_bgp_lib::SWAP_BYTES(&type);
                    data += 2;

                    // 2 byte length
                    u_int16_t sid_label_size = 0;
                    memcpy(&sid_label_size, data, 2);
                    parse_bgp_lib::SWAP_BYTES(&sid_label_size);
                    data += 2;

                    // Parsing SID/Label Sub-TLV: https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-03#section-2.3.7.2
                    if (type == SUB_TLV_SID_LABEL) {

                        if (sid_label_size == 3 || sid_label_size == 4) {
                            memcpy(&value_32bit, data, sid_label_size);
                            parse_bgp_lib::SWAP_BYTES(&value_32bit);

                            if (sid_label_size == 3) {
                                value_32bit = value_32bit >> 8;

                            } else {
                                // Add extra byte for sid len of 4 instead of 3
                                l++;
                            }

                            val_ss << " " << value_32bit;

                        } else {
                            LOG_NOTICE("bgp-ls: parsed node sr capabilities, sid label size is unexpected");
                            break;
                        }
                    } else {
                        LOG_NOTICE("bgp-ls: parsed node sr capabilities, SUB TLV type %d is unexpected", type);
                        break;
                    }
                    update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.push_back(val_ss.str());
                }
                SELF_DEBUG("bgp-ls: parsed node sr capabilities (len=%d) %s", len);
                std::cout << "Parsed SR capability: ";
                std::list<std::string>::iterator last_value = update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.end();
                last_value--;
                for (std::list<std::string>::iterator it = update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.begin();
                     it != update->attrs[LIB_ATTR_LS_SR_CAPABILITIES_TLV].value.end();
                     it++) {
                    std::cout << *it;
                    if (it != last_value) {
                        std::cout << ", ";
                    }
                }
                std::cout << std::endl;

                break;
            }

            case ATTR_LINK_ADMIN_GROUP:
                val_ss.str(std::string());

                if (len != 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute link admin group sub-tlv, size not 4");
                    break;
                } else {
                    value_32bit = 0;
                    memcpy(&value_32bit, data, len);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit, len);
                    val_ss << value_32bit;
                    update->attrs[LIB_ATTR_LS_ADMIN_GROUP].official_type = ATTR_LINK_ADMIN_GROUP;
                    update->attrs[LIB_ATTR_LS_ADMIN_GROUP].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_ADMIN_GROUP];
                    update->attrs[LIB_ATTR_LS_ADMIN_GROUP].value.push_back(val_ss.str());
                    SELF_DEBUG("bgp-ls: parsed linked admin group attribute: "
                                       " 0x%x, len = %d", value_32bit, len);
                }
                break;

            case ATTR_LINK_IGP_METRIC:
                update->attrs[LIB_ATTR_LS_IGP_METRIC].official_type = ATTR_LINK_IGP_METRIC;
                update->attrs[LIB_ATTR_LS_IGP_METRIC].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_IGP_METRIC];

                val_ss.str(std::string());


                if (len <= 4) {
                    value_32bit = 0;
                    memcpy(&value_32bit, data, len);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit, len);
                }

                val_ss << value_32bit;
                update->attrs[LIB_ATTR_LS_IGP_METRIC].value.push_back(val_ss.str());

                SELF_DEBUG("bgp-ls: parsed link IGP metric attribute: metric = %u", value_32bit);
                break;

            case ATTR_LINK_IPV4_ROUTER_ID_REMOTE:
                if (len != 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute remote IPv4 sub-tlv; too short");
                    break;
                }

                memcpy(ip_raw, data, 4);
                inet_ntop(AF_INET,ip_raw, ip_char, sizeof(ip_char));

                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4].official_type = ATTR_LINK_IPV4_ROUTER_ID_REMOTE;
                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4];
                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4].value.push_back(ip_char);

                SELF_DEBUG("bgp-ls: parsed remote IPv4 router id attribute: addr = %s", ip_char);
                break;

            case ATTR_LINK_IPV6_ROUTER_ID_REMOTE:
                if (len != 16) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute remote router id IPv6 sub-tlv; too short");
                    break;
                }
                memcpy(ip_raw, data, 16);
                inet_ntop(AF_INET6,ip_raw, ip_char, sizeof(ip_char));

                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6].official_type = ATTR_LINK_IPV6_ROUTER_ID_REMOTE;
                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6];
                update->attrs[LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6].value.push_back(ip_char);

                SELF_DEBUG("bgp-ls: parsed remote IPv6 router id attribute: addr = %s", ip_char);
                break;

            case ATTR_LINK_MAX_LINK_BW:
                if (len != 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute maximum link bandwidth sub-tlv; too short");
                    break;
                }
                update->attrs[LIB_ATTR_LS_MAX_LINK_BW].official_type = ATTR_LINK_MAX_LINK_BW;
                update->attrs[LIB_ATTR_LS_MAX_LINK_BW].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_MAX_LINK_BW];

                val_ss.str(std::string());

                float_val = 0;
                memcpy(&float_val, data, len);
                parse_bgp_lib::SWAP_BYTES(&float_val, len);
                float_val = ieee_float_to_kbps(float_val);
                val_ss << float_val;
                update->attrs[LIB_ATTR_LS_MAX_LINK_BW].value.push_back(val_ss.str());

                memcpy(&value_32bit, data, 4);
                parse_bgp_lib::SWAP_BYTES(&value_32bit);
                SELF_DEBUG("bgp-ls: parsed attribute maximum link bandwidth (raw=%x) %u Kbits (len=%d)", value_32bit, *(int32_t *)&float_val, len);
                break;

            case ATTR_LINK_MAX_RESV_BW:
                if (len != 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute remote IPv4 sub-tlv; too short");
                    break;
                }

                update->attrs[LIB_ATTR_LS_MAX_RESV_BW].official_type = ATTR_LINK_MAX_RESV_BW;
                update->attrs[LIB_ATTR_LS_MAX_RESV_BW].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_MAX_RESV_BW];

                val_ss.str(std::string());


                float_val = 0;
                memcpy(&float_val, data, len);
                parse_bgp_lib::SWAP_BYTES(&float_val, len);
                float_val = ieee_float_to_kbps(float_val);
                val_ss << float_val;
                update->attrs[LIB_ATTR_LS_MAX_RESV_BW].value.push_back(val_ss.str());

                SELF_DEBUG("bgp-ls: parsed attribute maximum reserved bandwidth %u Kbits (len=%d)", *(uint32_t *)&float_val, len);
                break;

            case ATTR_LINK_MPLS_PROTO_MASK:
                // SELF_DEBUG("%s: bgp-ls: parsing link MPLS Protocol mask attribute", peer_ad dr.c_str());
                LOG_INFO("bgp-ls: link MPLS Protocol mask attribute, not yet implemented");
                break;

            case ATTR_LINK_PROTECTION_TYPE:
                // SELF_DEBUG("%s: bgp-ls: parsing link protection type attribute", peer_addr.c_str());
                LOG_INFO("bgp-ls: link protection type attribute, not yet implemented");
                break;

            case ATTR_LINK_NAME: {
                update->attrs[LIB_ATTR_LS_LINK_NAME].official_type = ATTR_LINK_NAME;
                update->attrs[LIB_ATTR_LS_LINK_NAME].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_LINK_NAME];
                update->attrs[LIB_ATTR_LS_LINK_NAME].value.push_back(std::string((char *)data, (char *)(data + len)));


                SELF_DEBUG("bgp-ls: parsing link name attribute: name = %s", update->attrs[LIB_ATTR_LS_LINK_NAME].value.front().c_str());
                break;
            }

            case ATTR_LINK_ADJACENCY_SID: {
                update->attrs[LIB_ATTR_LS_ADJACENCY_SID].official_type = ATTR_LINK_ADJACENCY_SID;
                update->attrs[LIB_ATTR_LS_ADJACENCY_SID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_ADJACENCY_SID];

                // https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.1.1
                for (std::list<parseBgpLib::parse_bgp_lib_nlri>::iterator it = update->nlri_list.begin();
                     it != update->nlri_list.end();
                     it++) {
                    if (it->afi == BGP_AFI_BGPLS) {
                        if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "IS-IS") >= 0) {
                            update->attrs[LIB_ATTR_LS_ADJACENCY_SID].value.push_back(this->parse_flags_to_string(*data,
                                                                                                                 LS_FLAGS_PEER_ADJ_SID_ISIS, sizeof(LS_FLAGS_PEER_ADJ_SID_ISIS)));
                        } else if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "OSPF") >= 0) {
                            update->attrs[LIB_ATTR_LS_ADJACENCY_SID].value.push_back(this->parse_flags_to_string(*data,
                                                                                                                 LS_FLAGS_PEER_ADJ_SID_OSPF, sizeof(LS_FLAGS_PEER_ADJ_SID_OSPF)));
                        }
                        break;
                    }
                }

                data += 1;

                u_int8_t weight;
                memcpy(&weight, data, 1);

                val_ss.str(std::string());
                val_ss << (int)weight;

                // 1 byte for Weight + 2 bytes for Reserved
                data += 3;

                // Parse the sid/value
                val_ss << " " << parse_sid_value(data, len - 4);
                update->attrs[LIB_ATTR_LS_ADJACENCY_SID].value.push_back(val_ss.str());

                SELF_DEBUG("bgp-ls: parsed sr link adjacency segment identifier %s", val_ss.str().c_str());
                break;
            }

            case ATTR_LINK_SRLG:
                // SELF_DEBUG("%s: bgp-ls: parsing link SRLG attribute", peer_addr.c_str());
                LOG_INFO("bgp-ls: link SRLG attribute, not yet implemented");
                break;

            case ATTR_LINK_TE_DEF_METRIC:
                value_32bit = 0;

                // Per rfc7752 Section 3.3.2.3, this is supposed to be 4 bytes, but some implementations have this <=4.

                if (len == 0) {
                    val_ss << value_32bit;
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].official_type = ATTR_LINK_TE_DEF_METRIC;
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_TE_DEF_METRIC];
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].value.push_back(val_ss.str());
                    break;
                } else if (len > 4) {
                    LOG_NOTICE("bgp-ls: failed to parse attribute TE default metric sub-tlv; too long %d", len);
                    break;
                } else {
                    memcpy(&value_32bit, data, len);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit, len);
                    val_ss << value_32bit;
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].official_type = ATTR_LINK_TE_DEF_METRIC;
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_TE_DEF_METRIC];
                    update->attrs[LIB_ATTR_LS_TE_DEF_METRIC].value.push_back(val_ss.str());
                    SELF_DEBUG("bgp-ls: parsed attribute te default metric 0x%X (len=%d)", value_32bit, len);
                }

                break;

            case ATTR_LINK_UNRESV_BW: {
                std::stringstream   val_ss;

                SELF_DEBUG("bgp-ls: parsing link unreserve bw attribute (len=%d)", len);

                if (len != 32) {
                    LOG_INFO("bgp-ls: link unreserve bw attribute is invalid, length is %d but should be 32", len);
                    break;
                }

                update->attrs[LIB_ATTR_LS_UNRESV_BW].official_type = ATTR_LINK_UNRESV_BW;
                update->attrs[LIB_ATTR_LS_UNRESV_BW].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_UNRESV_BW];


                for (int i=0; i < 32; i += 4) {
                    val_ss.str(std::string());  // Clear

                    float_val = 0;
                    memcpy(&float_val, data, 4);
                    parse_bgp_lib::SWAP_BYTES(&float_val);
                    float_val = ieee_float_to_kbps(float_val);

                    data += 4;

                    val_ss << float_val;
                    update->attrs[LIB_ATTR_LS_UNRESV_BW].value.push_back(val_ss.str());
                }

                break;
            }

            case ATTR_LINK_OPAQUE:
                LOG_INFO("bgp-ls: opaque link attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_LINK_PEER_EPE_NODE_SID:
                val_ss.str(std::string());

                /*
                 * Syntax of value is: [L] <weight> <sid value>
                 *
                 *      L flag indicates locally significant
                 */

                if (*data & 0x80)
                    val_ss << "V";

                if (*data & 0x40)
                    val_ss << "L";

                update->attrs[LIB_ATTR_LS_PEER_EPE_NODE_SID].official_type = ATTR_LINK_PEER_EPE_NODE_SID;
                update->attrs[LIB_ATTR_LS_PEER_EPE_NODE_SID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_PEER_EPE_NODE_SID];
                update->attrs[LIB_ATTR_LS_PEER_EPE_NODE_SID].value.push_back(val_ss.str());

                val_ss.str(std::string());

                val_ss << (int) *(data + 1) << " " << parse_sid_value( (data+4), len - 4);
                update->attrs[LIB_ATTR_LS_PEER_EPE_NODE_SID].value.push_back(val_ss.str());


                SELF_DEBUG("bgp-ls: parsed link peer node SID: %s (len=%d) %x", val_ss.str().c_str(), len, (data+4));

                break;

            case ATTR_LINK_PEER_EPE_SET_SID:
                LOG_INFO("bgp-ls: peer epe set SID link attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_LINK_PEER_EPE_ADJ_SID:
                LOG_INFO("bgp-ls: peer epe adjacency SID link attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_PREFIX_EXTEND_TAG:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix extended tag attribute", peer_addr.c_str());
                LOG_INFO("bgp-ls: prefix extended tag attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_PREFIX_IGP_FLAGS:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix IGP flags attribute", peer_addr.c_str());
                LOG_INFO("bgp-ls: prefix IGP flags attribute, not yet implemented");
                break;

            case ATTR_PREFIX_PREFIX_METRIC:
                value_32bit = 0;
                if (len <= 4) {
                    memcpy(&value_32bit, data, len);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit, len);
                }

                val_ss << value_32bit;
                update->attrs[LIB_ATTR_LS_PREFIX_METRIC].official_type = ATTR_PREFIX_PREFIX_METRIC;
                update->attrs[LIB_ATTR_LS_PREFIX_METRIC].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_PREFIX_METRIC];
                update->attrs[LIB_ATTR_LS_PREFIX_METRIC].value.push_back(val_ss.str());
                SELF_DEBUG("bgp-ls: parsing prefix metric attribute: metric = %u", value_32bit);
                break;

            case ATTR_PREFIX_ROUTE_TAG:
            {
                SELF_DEBUG("bgp-ls: parsing prefix route tag attribute (len=%d)", len);

                // TODO(undefined): Per RFC7752 section 3.3.3, prefix tag can be multiples, but for now we only decode the first one.
                value_32bit = 0;

                if (len == 4) {
                    memcpy(&value_32bit, data, len);
                    parse_bgp_lib::SWAP_BYTES(&value_32bit);

                    val_ss << value_32bit;
                    update->attrs[LIB_ATTR_LS_ROUTE_TAG].official_type = ATTR_PREFIX_ROUTE_TAG;
                    update->attrs[LIB_ATTR_LS_ROUTE_TAG].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_ROUTE_TAG];
                    update->attrs[LIB_ATTR_LS_ROUTE_TAG].value.push_back(val_ss.str());
//                    SELF_DEBUG("%s: bgp-ls: parsing prefix route tag attribute %d (len=%d)", peer_addr.c_str(),
//                             value_32bit, len);
                }

                break;
            }
            case ATTR_PREFIX_OSPF_FWD_ADDR:
                // SELF_DEBUG("%s: bgp-ls: parsing prefix OSPF forwarding address attribute", peer_addr.c_str());
                LOG_INFO("bgp-ls: prefix OSPF forwarding address attribute, not yet implemented");
                break;

            case ATTR_PREFIX_OPAQUE_PREFIX:
                LOG_INFO("bgp-ls: opaque prefix attribute (len=%d), not yet implemented", len);
                break;

            case ATTR_PREFIX_SID: {
                update->attrs[LIB_ATTR_LS_PREFIX_SID].official_type = ATTR_PREFIX_SID;
                update->attrs[LIB_ATTR_LS_PREFIX_SID].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_LS_PREFIX_SID];

                val_ss.str(std::string());
                // https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-04#section-2.1.1
                for (std::list<parseBgpLib::parse_bgp_lib_nlri>::iterator it = update->nlri_list.begin();
                     it != update->nlri_list.end();
                     it++) {
                    if (it->afi == BGP_AFI_BGPLS) {
                        if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "IS-IS") >= 0) {
                            update->attrs[LIB_ATTR_LS_PREFIX_SID].value.push_back(parse_flags_to_string(*data,
                                                                                                        LS_FLAGS_PREFIX_SID_ISIS, sizeof(LS_FLAGS_PREFIX_SID_ISIS)));
                        } else if (strcmp(it->nlri[LIB_NLRI_LS_PROTOCOL].value.front().c_str(), "OSPF") >= 0) {
                            update->attrs[LIB_ATTR_LS_PREFIX_SID].value.push_back(parse_flags_to_string(*data,
                                                                                                        LS_FLAGS_PREFIX_SID_OSPF, sizeof(LS_FLAGS_PREFIX_SID_OSPF)));
                        }
                        break;
                    }
                }

                uint8_t alg;
                alg = *data;
                data += 1;

                val_ss.str(std::string());
                switch (alg) {
                    case 0: // Shortest Path First (SPF) algorithm based on link metric
                        val_ss << "SPF ";
                        update->attrs[LIB_ATTR_LS_PREFIX_SID].value.push_back(val_ss.str());
                        break;

                    case 1: // Strict Shortest Path First (SPF) algorithm based on link metric
                        val_ss << "strict-SPF ";
                        update->attrs[LIB_ATTR_LS_PREFIX_SID].value.push_back(val_ss.str());
                        break;
                }

                // 2 bytes reserved
                data += 2;

                // Parse the sid/value
                val_ss.str(std::string());
                val_ss << parse_sid_value(data, len - 4);
                update->attrs[LIB_ATTR_LS_PREFIX_SID].value.push_back(val_ss.str());

                SELF_DEBUG("bgp-ls: parsed sr prefix segment identifier  flags = %x len=%d : %s", *(data - 4), len, val_ss.str().c_str());

                break;
            }

            default:
                LOG_INFO("bgp-ls: Attribute type=%d len=%d not yet implemented, skipping", type, len);
                break;
        }

        return len + 4;
    }

} /* namespace parse_bgp_lib */
