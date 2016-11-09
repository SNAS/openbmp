/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "MPReachAttr.h"
#include "MPLinkState.h"
#include "BMPReader.h"

#include <arpa/inet.h>

namespace bgp_msg {

/**
 * Constructor for class
 *
 * \details Handles BGP MP Reach NLRI
 *
 * \param [in]     logPtr                   Pointer to existing Logger for app logging
 * \param [in]     pperAddr                 Printed form of peer address used for logging
 * \param [in]     peer_info                Persistent Peer info pointer
 * \param [in]     enable_debug             Debug true to enable, false to disable
 */
MPReachAttr::MPReachAttr(Logger *logPtr, std::string peerAddr, BMPReader::peer_info *peer_info, bool enable_debug)
    : logger{logPtr}, peer_info{peer_info}, debug{enable_debug}{
        this->peer_addr = peerAddr;
}

MPReachAttr::~MPReachAttr() {
}

/**
 * Parse Route Distinguisher
 *
 * \details
 *      Will parse the Route Distinguisher. Based on https://tools.ietf.org/html/rfc4364#section-4.2
 *
 * \param [in/out]  rd_beginning_data_pointer  Pointer to the beginning of Route Distinguisher
 * \param [out]     rd_type                    Reference to RD type.
 * \param [out]     rd_assigned_number         Reference to Assigned Number subfield
 * \param [out]     rd_administrator_subfield  Reference to Administrator subfield
 */
void MPReachAttr::parseRouteDistinguisher(u_char *rd_beginning_data_pointer, uint8_t *rd_type,
                                          std::string *rd_assigned_number, std::string *rd_administrator_subfield) {

    rd_beginning_data_pointer++;
    *rd_type = *rd_beginning_data_pointer;
    rd_beginning_data_pointer++;

    switch (*rd_type) {
        case 0: {
            uint16_t administration_subfield;
            bzero(&administration_subfield, 2);
            memcpy(&administration_subfield, rd_beginning_data_pointer, 2);

            rd_beginning_data_pointer += 2;

            uint32_t assigned_number_subfield;
            bzero(&assigned_number_subfield, 4);
            memcpy(&assigned_number_subfield, rd_beginning_data_pointer, 4);
            rd_beginning_data_pointer += 4;

            bgp::SWAP_BYTES(&administration_subfield);
            bgp::SWAP_BYTES(&assigned_number_subfield);

            *rd_assigned_number = std::to_string(assigned_number_subfield);
            *rd_administrator_subfield = std::to_string(administration_subfield);

            break;
        };

        case 1: {
            u_char administration_subfield[4];
            bzero(&administration_subfield, 4);
            memcpy(&administration_subfield, rd_beginning_data_pointer, 4);

            rd_beginning_data_pointer += 4;

            uint16_t assigned_number_subfield;
            bzero(&assigned_number_subfield, 2);
            memcpy(&assigned_number_subfield, rd_beginning_data_pointer, 2);
            rd_beginning_data_pointer += 2;

            bgp::SWAP_BYTES(&assigned_number_subfield);

            char administration_subfield_chars[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, administration_subfield, administration_subfield_chars, INET_ADDRSTRLEN);

            *rd_assigned_number = std::to_string(assigned_number_subfield);
            *rd_administrator_subfield = std::string(administration_subfield_chars);

            break;
        };

        case 2: {
            uint32_t administration_subfield;
            bzero(&administration_subfield, 4);
            memcpy(&administration_subfield, rd_beginning_data_pointer, 4);

            rd_beginning_data_pointer += 4;

            uint16_t assigned_number_subfield;
            bzero(&assigned_number_subfield, 2);
            memcpy(&assigned_number_subfield, rd_beginning_data_pointer, 2);

            rd_beginning_data_pointer += 2;

            bgp::SWAP_BYTES(&administration_subfield);
            bgp::SWAP_BYTES(&assigned_number_subfield);

            *rd_assigned_number = std::to_string(assigned_number_subfield);
            *rd_administrator_subfield = std::to_string(administration_subfield);

            break;
        };
    }
}

/**
 * Parse the MP_REACH NLRI attribute data
 *
 * \details
 *      Will parse the MP_REACH_NLRI data passed.  Parsed data will be stored
 *      in parsed_data.
 *
 *      \see RFC4760 for format details.
 *
 * \param [in]   attr_len               Length of the attribute data
 * \param [in]   data                   Pointer to the attribute data
 * \param [out]  parsed_data            Reference to parsed_update_data; will be updated with all parsed data
 */
void MPReachAttr::parseReachNlriAttr(int attr_len, u_char *data, UpdateMsg::parsed_update_data &parsed_data) {
    mp_reach_nlri nlri;
    /*
     * Set the MP NLRI struct
     */
    // Read address family
    memcpy(&nlri.afi, data, 2); data += 2; attr_len -= 2;
    bgp::SWAP_BYTES(&nlri.afi);                     // change to host order

    nlri.safi = *data++; attr_len--;                 // Set the SAFI - 1 octet
    nlri.nh_len = *data++; attr_len--;              // Set the next-hop length - 1 octet
    nlri.next_hop = data;  data += nlri.nh_len; attr_len -= nlri.nh_len;    // Set pointer position for nh data
    nlri.reserved = *data++; attr_len--;             // Set the reserve octet
    nlri.nlri_data = data;                          // Set pointer position for nlri data
    nlri.nlri_len = attr_len;                       // Remaining attribute length is for NLRI data

    /*
     * Make sure the parsing doesn't exceed buffer
     */
    if (attr_len < 0) {
        LOG_NOTICE("%s: MP_REACH NLRI data length is larger than attribute data length, skipping parse", peer_addr.c_str());
        return;
    }

    SELF_DEBUG("%s: afi=%d safi=%d nh_len=%d reserved=%d", peer_addr.c_str(),
                nlri.afi, nlri.safi, nlri.nh_len, nlri.reserved);

    /*
     * Next-hop and NLRI data depends on the AFI & SAFI
     *  Parse data based on AFI + SAFI
     */
    parseAfi(nlri, parsed_data);
}


/**
 * MP Reach NLRI parse based on AFI
 *
 * \details Will parse the next-hop and nlri data based on AFI.  A call to
 *          the specific SAFI method will be performed to further parse the message.
 *
 * \param [in]   nlri           Reference to parsed NLRI struct
 * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
 */
void MPReachAttr::parseAfi(mp_reach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data) {

    switch (nlri.afi) {
        case bgp::BGP_AFI_IPV6 :  // IPv6
            parseAfi_IPv4IPv6(false, nlri, parsed_data);
            break;

        case bgp::BGP_AFI_IPV4 : // IPv4
            parseAfi_IPv4IPv6(true, nlri, parsed_data);
            break;

        case bgp::BGP_AFI_BGPLS : // BGP-LS (draft-ietf-idr-ls-distribution-10)
        {
            MPLinkState ls(logger, peer_addr, &parsed_data, debug);
            ls.parseReachLinkState(nlri);

            break;
        }

        case bgp::BGP_AFI_L2VPN :
        {
            switch (nlri.safi) {

                case bgp::BGP_SAFI_EVPN : // https://tools.ietf.org/html/rfc7432
                {
                    u_char *pointer = nlri.nlri_data;
                    bgp::evpn_tuple tuple;

                    //------------------------------------------------------
                    std::cout << "EVPN" << std::endl;


                    while(pointer < nlri.nlri_data + nlri.nlri_len) {
                        std::cout << std::hex << setfill('0') << setw(2) << (int)(*pointer);
                        pointer += 1;
                        if ((long)pointer % 8 == 0) {
                            std::cout << std::endl;
                        }

                    }

                    std::cout << std::endl;
                    //------------------------------------------------------

                    pointer = nlri.nlri_data;

                    uint8_t route_type = *pointer;
                    pointer++;

                    uint8_t route_len

                    std::cout << "Route type:" << (int)route_type << std::endl;

                    parseRouteDistinguisher(
                        pointer,
                        &tuple.rd_type,
                        &tuple.rd_assigned_number,
                        &tuple.rd_administrator_subfield
                    );

                    pointer += 8;

                    std::cout << "RD: " << (int)tuple.rd_type << " " << tuple.rd_assigned_number << " " << tuple.rd_administrator_subfield << std::endl;

                    switch (route_type) {
                        case 1:
                        {
//                            std::cout << "HELLO" << std::endl;
                            break;
                        }
                        case 2:
                        {
//                            std::cout << "HELLO" << std::endl;
                            break;
                        }
                        case 3:
                        {
                            std::cout << "HELLO" << std::endl;
                            break;
                        }
                        case 4:
                        {
//                            std::cout << "HELLO" << std::endl;

                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }

                    break;
                }

                default :
                    LOG_INFO("%s: MP_REACH AFI=%d SAFI=%d is not implemented yet, skipping",
                             peer_addr.c_str(), nlri.afi, nlri.safi);
            }
            break;
        }

        default : // Unknown
            LOG_INFO("%s: MP_REACH AFI=%d is not implemented yet, skipping", peer_addr.c_str(), nlri.afi);
            return;
    }
}

/**
 * MP Reach NLRI parse for BGP_AFI_IPv4 & BGP_AFI_IPV6
 *
 * \details Will handle parsing the SAFI's for address family ipv6 and IPv4
 *
 * \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
 * \param [in]   nlri           Reference to parsed NLRI struct
 * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
 */
void MPReachAttr::parseAfi_IPv4IPv6(bool isIPv4, mp_reach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data) {
    u_char      ip_raw[16];
    char        ip_char[40];

    bzero(ip_raw, sizeof(ip_raw));

    /*
     * Decode based on SAFI
     */
    switch (nlri.safi) {
        case bgp::BGP_SAFI_UNICAST: // Unicast IP address prefix

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            if (not isIPv4)
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));

            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

            // Data is an IP address - parse the address and save it
            parseNlriData_IPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, peer_info, parsed_data.advertised);
            break;

        case bgp::BGP_SAFI_NLRI_LABEL:
            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            if (not isIPv4)
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));

            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

            // Data is an Label, IP address tuple parse and save it
            parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, peer_info, parsed_data.advertised);
            break;
            
        default :
            LOG_INFO("%s: MP_REACH AFI=ipv4/ipv6 (%d) SAFI=%d is not implemented yet, skipping for now",
                     peer_addr.c_str(), isIPv4, nlri.safi);
            return;
    }
}

/**
 * Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
 *
 * \details
 *      Will parse the NLRI encoding as defined in RFC4760 Section 5 (NLRI Encoding).
 *
 * \param [in]   isIPv4                 True false to indicate if IPv4 or IPv6
 * \param [in]   data                   Pointer to the start of the prefixes to be parsed
 * \param [in]   len                    Length of the data in bytes to be read
 * \param [in]   peer_info              Persistent Peer info pointer
 * \param [out]  prefixes               Reference to a list<prefix_tuple> to be updated with entries
 */
void MPReachAttr::parseNlriData_IPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                         BMPReader::peer_info * peer_info,
                                         std::list<bgp::prefix_tuple> &prefixes) {
    u_char            ip_raw[16];
    char              ip_char[40];
    u_char            addr_bytes;
    bgp::prefix_tuple tuple;

    if (len <= 0 or data == NULL)
        return;

    // TODO: Can extend this to support multicast, but right now we set it to unicast v4/v6
    tuple.type = isIPv4 ? bgp::PREFIX_UNICAST_V4 : bgp::PREFIX_UNICAST_V6;
    tuple.isIPv4 = isIPv4;

    // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {
        tuple.path_id = 0;

        bzero(ip_raw, sizeof(ip_raw));

        // Parse add-paths if enabled
        if (peer_info->add_path_capability.isAddPathEnabled(isIPv4 ? bgp::BGP_AFI_IPV4 : bgp::BGP_AFI_IPV6, bgp::BGP_SAFI_UNICAST)
                and (len - read_size) >= 4) {
            memcpy(&tuple.path_id, data, 4);
            bgp::SWAP_BYTES(&tuple.path_id);
            data += 4; read_size += 4;
        } else
            tuple.path_id = 0;

        // set the address in bits length
        tuple.len = *data++;

        // Figure out how many bytes the bits requires
        addr_bytes = tuple.len / 8;
        if (tuple.len % 8)
           ++addr_bytes;

        // if the route isn't a default route
        if (addr_bytes > 0) {
            memcpy(ip_raw, data, addr_bytes);
            data += addr_bytes;
            read_size += addr_bytes;

            // Convert the IP to string printed format
            if (isIPv4)
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            tuple.prefix.assign(ip_char);

            // set the raw/binary address
            memcpy(tuple.prefix_bin, ip_raw, sizeof(ip_raw));
          
            // Add tuple to prefix list
            prefixes.push_back(tuple);
        }
    }
}

/**
 * Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
 *
 * \details
 *      Will parse the NLRI encoding as defined in RFC3107 Section 3 (Carrying Label Mapping information).
 *
 * \param [in]   isIPv4                 True false to indicate if IPv4 or IPv6
 * \param [in]   data                   Pointer to the start of the label + prefixes to be parsed
 * \param [in]   len                    Length of the data in bytes to be read
 * \param [in]   peer_info              Persistent Peer info pointer
 * \param [out]  prefixes               Reference to a list<label, prefix_tuple> to be updated with entries
 */
void MPReachAttr::parseNlriData_LabelIPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                              BMPReader::peer_info * peer_info,
                                              std::list<bgp::prefix_tuple> &prefixes) {
    u_char            ip_raw[16];
    char              ip_char[40];
    int               addr_bytes;
    bgp::prefix_tuple tuple;

    typedef union {
        struct { 
            uint8_t   ttl     : 8;          // TTL - not present since only 3 octets are used
            uint8_t   bos     : 1;          // Bottom of stack
            uint8_t   exp     : 3;          // EXP - not really used
            uint32_t  value   : 20;         // Label value
        } decode;
        uint32_t  data;                 // Raw label - 3 octets only per RFC3107
    } mpls_label;
  
    mpls_label label;

    if (len <= 0 or data == NULL)
        return;

    tuple.type = isIPv4 ? bgp::PREFIX_LABEL_UNICAST_V4 :
			  bgp::PREFIX_LABEL_UNICAST_V6;
    tuple.isIPv4 = isIPv4;
    int parsed_bytes = 0;

    // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {

        // Parse add-paths if enabled
        if (peer_info->add_path_capability.isAddPathEnabled(isIPv4 ? bgp::BGP_AFI_IPV4 : bgp::BGP_AFI_IPV6, bgp::BGP_SAFI_NLRI_LABEL)
                and (len - read_size) >= 4) {

            memcpy(&tuple.path_id, data, 4);
            bgp::SWAP_BYTES(&tuple.path_id);
            data += 4; read_size += 4;
        } else
            tuple.path_id = 0;


        if (parsed_bytes == len) {
            break;
        }

        bzero(&label, sizeof(label));
        bzero(ip_raw, sizeof(ip_raw));

        tuple.labels = "";
        // set the address in bits length
        tuple.len = *data++;

        // Figure out how many bytes the bits requires
        addr_bytes = tuple.len / 8;
        if (tuple.len % 8)
           ++addr_bytes;

        // the label is 3 octets long
        while (addr_bytes >= 3) 
        {
            memcpy(&label.data, data, 3);
            bgp::SWAP_BYTES(&label.data);     // change to host order

            data += 3;
            addr_bytes -= 3;
            read_size += 3;
            tuple.len -= 24;        // Update prefix len to not include the label just parsed

            ostringstream convert;
            convert << label.decode.value;
            tuple.labels.append(convert.str());

            if (label.decode.bos == 1 or label.data == 0x80000000 /* withdrawn label as 32bits instead of 24 */) {
               break;               // Reached EoS

            } else {
               tuple.labels.append(",");
            }
        }

        // if the route isn't a default route
        if (addr_bytes > 0) {
            memcpy(ip_raw, data, addr_bytes);
            data += addr_bytes;
            read_size += addr_bytes;

            // Convert the IP to string printed format
            if (isIPv4)
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            tuple.prefix.assign(ip_char);

            // set the raw/binary address
            memcpy(tuple.prefix_bin, ip_raw, sizeof(ip_raw));

            // Add tuple to prefix list
            prefixes.push_back(tuple);
//            printf("Parsed Label: %s, prefix: %s len=%d\n", tuple.labels.c_str(), tuple.prefix.c_str(), tuple.len);
        }
    }
}

} /* namespace bgp_msg */
