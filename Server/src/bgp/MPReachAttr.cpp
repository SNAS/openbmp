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
#include "EVPN.h"
#include <typeinfo>

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
    : logger{logPtr}, peer_info{peer_info}, debug{enable_debug} {
        this->peer_addr = peerAddr;
}

MPReachAttr::~MPReachAttr() {
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
            u_char      ip_raw[16];
            char        ip_char[40];

            bzero(ip_raw, sizeof(ip_raw));

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            inet_ntop(nlri.nh_len == 4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

            // parse by safi
            switch (nlri.safi) {
                case bgp::BGP_SAFI_EVPN : // https://tools.ietf.org/html/rfc7432
                {
                    EVPN evpn(logger, peer_addr, false, &parsed_data, debug);
                    evpn.parseNlriData(nlri.nlri_data, nlri.nlri_len);
                    break;
                }

                default :
                    LOG_INFO("%s: EVPN::parse SAFI=%d is not implemented yet, skipping",
                             peer_addr.c_str(), nlri.safi);
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

            inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

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

            inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

            // Data is an Label, IP address tuple parse and save it
            parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, peer_info, parsed_data.advertised);
            break;

        case bgp::BGP_SAFI_MPLS: {

            if (isIPv4) {
                //Next hop encoded in 12 bytes, last 4 bytes = IPv4
                nlri.next_hop += 8;
                nlri.nh_len -= 8;
            }

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

            parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, peer_info, parsed_data.vpn);

            break;
        }

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

    bool add_path_enabled = peer_info->add_path_capability.isAddPathEnabled(isIPv4 ? bgp::BGP_AFI_IPV4 : bgp::BGP_AFI_IPV6,
                                                                            bgp::BGP_SAFI_UNICAST);

    // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {
        tuple.path_id = 0;

        bzero(ip_raw, sizeof(ip_raw));

        // Parse add-paths if enabled
        if (add_path_enabled and (len - read_size) >= 4) {
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

        memcpy(ip_raw, data, addr_bytes);
        data += addr_bytes;
        read_size += addr_bytes;

        // Convert the IP to string printed format
        inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

        tuple.prefix.assign(ip_char);

        // set the raw/binary address
        memcpy(tuple.prefix_bin, ip_raw, sizeof(ip_raw));

        // Add tuple to prefix list
        prefixes.push_back(tuple);
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
template <typename PREFIX_TUPLE>
void MPReachAttr::parseNlriData_LabelIPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                              BMPReader::peer_info * peer_info,
                                              std::list<PREFIX_TUPLE> &prefixes) {
    u_char            ip_raw[16];
    char              ip_char[40];
    int               addr_bytes;
    PREFIX_TUPLE      tuple;

    if (len <= 0 or data == NULL)
        return;

    tuple.type = isIPv4 ? bgp::PREFIX_LABEL_UNICAST_V4 : bgp::PREFIX_LABEL_UNICAST_V6;
    tuple.isIPv4 = isIPv4;

    bool isVPN = typeid(bgp::vpn_tuple) == typeid(tuple);
    uint16_t label_bytes;

    bool add_path_enabled = peer_info->add_path_capability.isAddPathEnabled(isIPv4 ? bgp::BGP_AFI_IPV4 : bgp::BGP_AFI_IPV6,
                                                                            isVPN ? bgp::BGP_SAFI_MPLS : bgp::BGP_SAFI_NLRI_LABEL);

    // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {

        // Only check for add-paths if not mpls/vpn
        if (not isVPN and add_path_enabled and (len - read_size) >= 4) {
            memcpy(&tuple.path_id, data, 4);
            bgp::SWAP_BYTES(&tuple.path_id);
            data += 4;
            read_size += 4;

        } else
            tuple.path_id = 0;

        bzero(ip_raw, sizeof(ip_raw));

        // set the address in bits length
        tuple.len = *data++;

        // Figure out how many bytes the bits requires
        addr_bytes = tuple.len / 8;
        if (tuple.len % 8)
           ++addr_bytes;

        label_bytes = decodeLabel(data, addr_bytes, tuple.labels);

        tuple.len -= (8 * label_bytes);      // Update prefix len to not include the label(s)
        data += label_bytes;               // move data pointer past labels
        addr_bytes -= label_bytes;
        read_size += label_bytes;

        // Parse RD if VPN
        if (isVPN and addr_bytes >= 8) {
            bgp::vpn_tuple *vtuple = (bgp::vpn_tuple *)&tuple;
            EVPN::parseRouteDistinguisher(data, &vtuple->rd_type, &vtuple->rd_assigned_number,
                                          &vtuple->rd_administrator_subfield);
            data += 8;
            addr_bytes -= 8;
            read_size += 8;
            tuple.len -= 64;
        }

        // Parse the prefix if it isn't a default route
        if (addr_bytes > 0) {
            memcpy(ip_raw, data, addr_bytes);
            data += addr_bytes;
            read_size += addr_bytes;

            // Convert the IP to string printed format
            inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            tuple.prefix.assign(ip_char);

            // set the raw/binary address
            memcpy(tuple.prefix_bin, ip_raw, sizeof(ip_raw));

        } else {
            tuple.prefix.assign(isIPv4 ? "0.0.0.0" : "::");
        }

        prefixes.push_back(tuple);
    }
}

/**
 * Decode label from NLRI data
 *
 * \details
 *      Decodes the labels from the NLRI data into labels string
 *
 * \param [in]   data                   Pointer to the start of the label + prefixes to be parsed
 * \param [in]   len                    Length of the data in bytes to be read
 * \param [out]  labels                 Reference to string that will be updated with labels delimited by comma
 *
 * \returns number of bytes read to decode the label(s) and updates string labels
 *
 */
inline uint16_t MPReachAttr::decodeLabel(u_char *data, uint16_t len, std::string &labels) {
    int read_size = 0;
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

    labels.clear();

    u_char *data_ptr = data;

    // the label is 3 octets long
    while (read_size <= len)
    {
        bzero(&label, sizeof(label));

        memcpy(&label.data, data_ptr, 3);
        bgp::SWAP_BYTES(&label.data);     // change to host order

        data_ptr += 3;
        read_size += 3;

        ostringstream convert;
        convert << label.decode.value;
        labels.append(convert.str());

        //printf("label data = %x\n", label.data);
        if (label.decode.bos == 1 or label.data == 0x80000000 /* withdrawn label as 32bits instead of 24 */
                or label.data == 0 /* l3vpn seems to use zero instead of rfc3107 suggested value */) {
            break;               // Reached EoS

        } else {
            labels.append(",");
        }
    }

    return read_size;
}

} /* namespace bgp_msg */
