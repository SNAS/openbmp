/*
* Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*
*/

#include <arpa/inet.h>
#include <iomanip>
#include "parseBgpLibMpEvpn.h"

namespace parse_bgp_lib {

    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     isUnreach    True if MP UNREACH, false if MP REACH
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    EVPN::EVPN(parseBgpLib *parse_lib, Logger *logPtr, bool isUnreach, std::list<parseBgpLib::parse_bgp_lib_nlri> *nlri_list, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        this->nlri_list = nlri_list;
        caller = parse_lib;
    }

    EVPN::~EVPN() {
    }

    /**
     * Parse Ethernet Segment Identifier
     *
     * \details
     *      Will parse the Segment Identifier. Based on https://tools.ietf.org/html/rfc7432#section-5
     *
     * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
     * \param [out]     rd_type                    Reference to RD type.
     * \param [out]     rd_assigned_number         Reference to Assigned Number subfield
     * \param [out]     rd_administrator_subfield  Reference to Administrator subfield
     */
    void EVPN::parseEthernetSegmentIdentifier(u_char *data_pointer, parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &parsed_nlri) {
        std::stringstream result;
        uint8_t type = *data_pointer;

        data_pointer++;

        result << (int) type << " ";

        switch (type) {
            case 0: {
                for (int i = 0; i < 9; i++) {
                    result << std::hex << std::setfill('0') << std::setw(2) << (int) data_pointer[i];
                }
                break;
            }
            case 1: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint16_t CE_LACP_port_key;
                memcpy(&CE_LACP_port_key, data_pointer, 2);
                parse_bgp_lib::SWAP_BYTES(&CE_LACP_port_key, 2);

                result << std::dec << (int) CE_LACP_port_key;

                break;
            }
            case 2: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint16_t root_bridge_priority;
                memcpy(&root_bridge_priority, data_pointer, 2);
                parse_bgp_lib::SWAP_BYTES(&root_bridge_priority, 2);

                result << std::dec << (int) root_bridge_priority;

                break;
            }
            case 3: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 3);
                parse_bgp_lib::SWAP_BYTES(&local_discriminator_value, 4);
                local_discriminator_value = local_discriminator_value >> 8;
                result << std::dec << (int) local_discriminator_value;

                break;
            }
            case 4: {
                uint32_t router_id;
                memcpy(&router_id, data_pointer, 4);
                parse_bgp_lib::SWAP_BYTES(&router_id, 4);
                result << std::dec << (int) router_id << " ";

                data_pointer += 4;

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 4);
                parse_bgp_lib::SWAP_BYTES(&local_discriminator_value, 4);
                result << std::dec << (int) local_discriminator_value;
                break;
            }
            case 5: {
                uint32_t as_number;
                memcpy(&as_number, data_pointer, 4);
                parse_bgp_lib::SWAP_BYTES(&as_number, 4);
                result << std::dec << (int) as_number << " ";

                data_pointer += 4;

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 4);
                parse_bgp_lib::SWAP_BYTES(&local_discriminator_value, 4);
                result << std::dec << (int) local_discriminator_value;
                break;
            }
            default:
                LOG_WARN("%sMP_REACH Cannot parse ethernet segment identifyer type: %d", caller->debug_prepend_string.c_str(), type);
                break;
        }

        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].official_type = type;
        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID];
        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].value.push_back(result.str());
    }

    /**
     * Parse Route Distinguisher
     *
     * \details
     *      Will parse the Route Distinguisher. Based on https://tools.ietf.org/html/rfc4364#section-4.2
     *
     * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
     * \param [out]     rd_type                    Reference to RD type.
     * \param [out]     rd_assigned_number         Reference to Assigned Number subfield
     * \param [out]     rd_administrator_subfield  Reference to Administrator subfield
     */
    void EVPN::parseRouteDistinguisher(u_char *data_pointer, parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &parsed_nlri) {
        std::stringstream   val_ss;

        uint8_t rd_type;
        std::string rd_assigned_number, rd_administrator_subfield;

        data_pointer++;
        rd_type = *data_pointer;
        data_pointer++;

        switch (rd_type) {
            case 0: {
                uint16_t administration_subfield;
                bzero(&administration_subfield, 2);
                memcpy(&administration_subfield, data_pointer, 2);

                data_pointer += 2;

                uint32_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 4);
                memcpy(&assigned_number_subfield, data_pointer, 4);

                parse_bgp_lib::SWAP_BYTES(&administration_subfield);
                parse_bgp_lib::SWAP_BYTES(&assigned_number_subfield);

                val_ss << assigned_number_subfield;

                rd_assigned_number = val_ss.str();

                val_ss.str(std::string());
                val_ss << administration_subfield;
                rd_administrator_subfield = val_ss.str();

                break;
            };

            case 1: {
                u_char administration_subfield[4];
                bzero(&administration_subfield, 4);
                memcpy(&administration_subfield, data_pointer, 4);

                data_pointer += 4;

                uint16_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 2);
                memcpy(&assigned_number_subfield, data_pointer, 2);

                parse_bgp_lib::SWAP_BYTES(&assigned_number_subfield);

                char administration_subfield_chars[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, administration_subfield, administration_subfield_chars, INET_ADDRSTRLEN);

                val_ss << assigned_number_subfield;
                rd_assigned_number = val_ss.str();

                rd_administrator_subfield = administration_subfield_chars;

                break;
            };

            case 2: {
                uint32_t administration_subfield;
                bzero(&administration_subfield, 4);
                memcpy(&administration_subfield, data_pointer, 4);

                data_pointer += 4;

                uint16_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 2);
                memcpy(&assigned_number_subfield, data_pointer, 2);

                parse_bgp_lib::SWAP_BYTES(&administration_subfield);
                parse_bgp_lib::SWAP_BYTES(&assigned_number_subfield);

                val_ss << assigned_number_subfield;
                rd_assigned_number = val_ss.str();

                val_ss.str(std::string());
                val_ss << administration_subfield;
                rd_administrator_subfield = val_ss.str();

                break;
            };
        }

        val_ss.str(std::string());
        val_ss << static_cast<int>(rd_type);
         parsed_nlri.nlri[LIB_NLRI_VPN_RD_TYPE].official_type = rd_type;
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_TYPE].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_VPN_RD_TYPE];
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_TYPE].value.push_back(val_ss.str());

        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER].official_type = rd_type;
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER];
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER].value.push_back(rd_assigned_number);

        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD].official_type = rd_type;
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD];
        parsed_nlri.nlri[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD].value.push_back(rd_administrator_subfield);

        parsed_nlri.nlri[LIB_NLRI_VPN_RD].name = parse_bgp_lib_nlri_names[parse_bgp_lib::LIB_NLRI_VPN_RD];
        parsed_nlri.nlri[LIB_NLRI_VPN_RD].value.push_back(std::string(rd_administrator_subfield + ":" + rd_assigned_number));
    }

    // TODO: Refactor this method as it's overloaded - each case statement can be its own method
    /**
     * Parse all EVPN nlri's
     *
     *
     * \details
     *      Parsing based on https://tools.ietf.org/html/rfc7432.  Will process all NLRI's in data.
     *
     * \param [in]   data                   Pointer to the start of the prefixes to be parsed
     * \param [in]   data_len               Length of the data in bytes to be read
     *
     */
    void EVPN::parseNlriData(u_char *data, uint16_t data_len) {
        u_char      *data_pointer = data;
        u_char      ip_binary[16];
        int         addr_bytes;
        char        ip_char[40];
        int         data_read = 0;

        uint8_t         ip_len;
        int             mpls_label_1;
        int             mpls_label_2;
        uint8_t         originating_router_ip_len;
        std::string     originating_router_ip;
        uint8_t mac_address_length;

        while ((data_read + 10 /* min read */) < data_len) {
            parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri parsed_nlri;
            parsed_nlri.afi =parse_bgp_lib::BGP_AFI_L2VPN;
            parsed_nlri.safi = parse_bgp_lib::BGP_SAFI_EVPN;
            parsed_nlri.type = parse_bgp_lib::LIB_NLRI_TYPE_NONE;

            ip_len = 0;
            mac_address_length = 0;
            mpls_label_1 = 0;
            mpls_label_2 = 0;
            originating_router_ip_len = 0;

            // Generate the hash
            MD5 hash;

            // TODO: Keep an eye on this, as we might need to support add-paths for evpn
            parsed_nlri.nlri[LIB_NLRI_PATH_ID].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PATH_ID];
            parsed_nlri.nlri[LIB_NLRI_PATH_ID].value.push_back(std::string("0"));

            uint8_t route_type = *data_pointer;
            data_pointer++;

            int len = *data_pointer;
            data_pointer++;

            parseRouteDistinguisher(data_pointer, parsed_nlri);
            update_hash(&parsed_nlri.nlri[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD].value, &hash);
            update_hash(&parsed_nlri.nlri[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER].value, &hash);
            data_pointer += 8;

            data_read += 10;
            len -= 8; // len doesn't include the route type and len octets

            switch (route_type) {
                case EVPN_ROUTE_TYPE_ETHERNET_AUTO_DISCOVERY: {

                    if ((data_read + 17 /* expected read size */) <= data_len) {

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, parsed_nlri);
                        update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].value, &hash);
                        data_pointer += 10;

                        //Ethernet Tag Id (4 bytes), printing in hex.

                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream val_ss;
                        std::stringstream val_ss2;

                        for (int i = 0; i < 4; i++) {
                            val_ss << std::hex << std::setfill('0') << std::setw(2) << (int) ethernet_id[i];
                        }
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].value.push_back(val_ss.str());

                        //MPLS Label (3 bytes)
                        memcpy(&mpls_label_1, data_pointer, 3);
                        parse_bgp_lib::SWAP_BYTES(&mpls_label_1);
                        mpls_label_1 >>= 8;
                        val_ss2.str(std::string());
                        val_ss2 << mpls_label_1;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_MPLS_LABEL1];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].value.push_back(val_ss2.str());

                        data_pointer += 3;
                        data_read += 17;
                        len -= 17;
                    }
                    break;
                }
                case EVPN_ROUTE_TYPE_MAC_IP_ADVERTISMENT: {

                    if ((data_read + 25 /* expected read size */) <= data_len) {

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, parsed_nlri);
                        update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].value, &hash);
                        data_pointer += 10;

                        // Ethernet Tag ID (4 bytes)

                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream val_ss;
                        std::stringstream val_ss2;

                        for (int i = 0; i < 4; i++) {
                            val_ss << std::hex << std::setfill('0') << std::setw(2) << (int) ethernet_id[i];
                        }

                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].value.push_back(val_ss.str());

                        // MAC Address Length (1 byte)
                        mac_address_length = *data_pointer;

                        val_ss2.str(std::string());
                        val_ss2 << static_cast<unsigned>(mac_address_length);
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC_LEN].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC_LEN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_MAC_LEN];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC_LEN].value.push_back(val_ss2.str());

                        data_pointer++;

                        // MAC Address (6 byte)
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_MAC];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_MAC].value.push_back(parse_bgp_lib::parse_mac(data_pointer));
                        update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_MAC].value, &hash);
                        data_pointer += 6;

                        // IP Address Length (1 byte)
                        ip_len = *data_pointer;
                        val_ss2.str(std::string());
                        val_ss2 << static_cast<unsigned>(ip_len);
                        parsed_nlri.nlri[LIB_NLRI_EVPN_IP_LEN].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_IP_LEN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_IP_LEN];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_IP_LEN].value.push_back(val_ss2.str());
                        update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_IP_LEN].value, &hash);

                        data_pointer++;

                        data_read += 22;
                        len -= 22;

                        addr_bytes = ip_len > 0 ? (ip_len / 8) : 0;

                        if (ip_len > 0 and (addr_bytes + data_read) <= data_len) {
                            // IP Address (0, 4, or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, addr_bytes);

                            inet_ntop(ip_len > 32 ? AF_INET6 : AF_INET, ip_binary, ip_char, sizeof(ip_char));

                            parsed_nlri.nlri[LIB_NLRI_EVPN_IP].official_type = route_type;
                            parsed_nlri.nlri[LIB_NLRI_EVPN_IP].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_IP];
                            parsed_nlri.nlri[LIB_NLRI_EVPN_IP].value.push_back(ip_char);
                            update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_IP].value, &hash);

                            data_pointer += addr_bytes;
                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }

                        if ((data_read + 3) <= data_len) {

                            // MPLS Label1 (3 bytes)
                            memcpy(&mpls_label_1, data_pointer, 3);
                            parse_bgp_lib::SWAP_BYTES(&mpls_label_1);
                            mpls_label_1 >>= 8;
                            val_ss2.str(std::string());
                            val_ss2 << mpls_label_1;
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].official_type = route_type;
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_MPLS_LABEL1];
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL1].value.push_back(val_ss2.str());

                            data_pointer += 3;
                            data_read += 3;
                            len -= 3;
                        }

                        // Parse second label if present
                        if (len == 3) {
                            SELF_DEBUG("%sparsing second evpn label\n", caller->debug_prepend_string.c_str());

                            memcpy(&mpls_label_2, data_pointer, 3);
                            parse_bgp_lib::SWAP_BYTES(&mpls_label_2);
                            mpls_label_2 >>= 8;
                            val_ss2.str(std::string());
                            val_ss2 << mpls_label_2;
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL2].official_type = route_type;
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL2].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_MPLS_LABEL2];
                            parsed_nlri.nlri[LIB_NLRI_EVPN_MPLS_LABEL2].value.push_back(val_ss2.str());

                            data_pointer += 3;
                            data_read += 3;
                            len -= 3;
                        }
                    }
                    break;
                }
                case EVPN_ROUTE_TYPE_INCLUSIVE_MULTICAST_ETHERNET_TAG: {

                    if ((data_read + 5 /* expected read size */) <= data_len) {

                        // Ethernet Tag ID (4 bytes)
                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream val_ss;
                        std::stringstream val_ss2;


                        for (int i = 0; i < 4; i++) {
                            val_ss << std::hex << std::setfill('0') << std::setw(2) << (int) ethernet_id[i];
                        }

                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX];
                        parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX].value.push_back(val_ss.str());

                        // IP Address Length (1 byte)
                        originating_router_ip_len = *data_pointer;
                        val_ss2.str(std::string());
                        val_ss2 << static_cast<int>(originating_router_ip_len);
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN];
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].value.push_back(val_ss2.str());

                        data_pointer++;

                        data_read += 5;
                        len -= 5;

                        addr_bytes = originating_router_ip_len > 0 ? (originating_router_ip_len / 8) : 0;

                        if (originating_router_ip_len > 0 and (addr_bytes + data_read) <= data_len) {

                            // Originating Router's IP Address (4 or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, addr_bytes);

                            inet_ntop(originating_router_ip_len > 32 ? AF_INET6 : AF_INET, ip_binary, ip_char, sizeof(ip_char));

                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].official_type = route_type;
                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_ORIGINATING_ROUTER_IP];
                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].value.push_back(ip_char);

                            data_pointer += addr_bytes;
                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }
                    }

                    break;
                }
                case EVPN_ROUTE_TYPE_ETHERNET_SEGMENT_ROUTE: {

                    if ((data_read + 11 /* expected read size */) <= data_len) {

                        std::stringstream val_ss;

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, parsed_nlri);
                        update_hash(&parsed_nlri.nlri[LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID].value, &hash);
                        data_pointer += 10;

                        // IP Address Length (1 bytes)
                        originating_router_ip_len = *data_pointer;
                        val_ss << static_cast<int>(originating_router_ip_len);
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].official_type = route_type;
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN];
                        parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP_LEN].value.push_back(val_ss.str());

                        data_pointer++;

                        data_read += 11;
                        len -= 11;

                        addr_bytes = originating_router_ip_len > 0 ? (originating_router_ip_len / 8) : 0;

                        if (originating_router_ip_len > 0 and (addr_bytes + data_read) <= data_len) {

                            // Originating Router's IP Address (4 or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, (int) originating_router_ip_len / 8);

                            inet_ntop(originating_router_ip_len > 32 ? AF_INET6 : AF_INET, ip_binary, ip_char, sizeof(ip_char));

                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].official_type = route_type;
                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_ORIGINATING_ROUTER_IP];
                            parsed_nlri.nlri[LIB_NLRI_ORIGINATING_ROUTER_IP].value.push_back(ip_char);

                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }
                    }

                    break;
                }
                default: {
                    LOG_INFO("%sEVPN ROUTE TYPE %d is not implemented yet, skipping", caller->debug_prepend_string.c_str(), route_type);
                    break;
                }
            }

            //Update hash to include peer hash id
            if (caller->p_info)
                hash.update((unsigned char *) caller->p_info->peer_hash_str.c_str(), caller->p_info->peer_hash_str.length());

            hash.finalize();

            // Save the hash
            unsigned char *hash_raw = hash.raw_digest();
            parsed_nlri.nlri[LIB_NLRI_HASH].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_HASH];
            parsed_nlri.nlri[LIB_NLRI_HASH].value.push_back(parse_bgp_lib::hash_toStr(hash_raw));
            delete[] hash_raw;

            nlri_list->push_back(parsed_nlri);


            SELF_DEBUG("%sProcessed evpn NLRI read %d of %d, nlri len %d", caller->debug_prepend_string.c_str(),
                       data_read, data_len, len);
        }
    }
} /* namespace parse_bgp_lib */