/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "MPReachAttr.h"
#include "MPLinkState.h"

#include <arpa/inet.h>

namespace bgp_msg {

/**
 * Constructor for class
 *
 * \details Handles BGP MP Reach NLRI
 *
 * \param [in]     logPtr       Pointer to existing Logger for app logging
 * \param [in]     pperAddr     Printed form of peer address used for logging
 * \param [in]     enable_debug Debug true to enable, false to disable
 */
MPReachAttr::MPReachAttr(Logger *logPtr, std::string peerAddr, bool enable_debug) {
    logger = logPtr;
    debug = enable_debug;

    peer_addr = peerAddr;
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
 * \param [in]   attr_len       Length of the attribute data
 * \param [in]   data           Pointer to the attribute data
 * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
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
            parseAfiIPv6(nlri, parsed_data);
            break;

        case bgp::BGP_AFI_BGPLS : // BGP-LS (draft-ietf-idr-ls-distribution-10)
        {
            MPLinkState ls(logger, peer_addr, &parsed_data, debug);
            ls.parseReachLinkState(nlri);

            break;
        }

        case bgp::BGP_AFI_IPV4 : // IPv4
        {
            // TODO: Add support for IPv4 in MPREACH/UNREACH
            break;

        }

        default : // Unknown
            LOG_INFO("%s: MP_REACH AFI=%d is not implemented yet, skipping", peer_addr.c_str(), nlri.afi);
            return;
    }
}

/**
 * MP Reach NLRI parse for BGP_AFI_IPV6
 *
 * \details Will handle parsing the SAFI's for address family ipv6
 *
 * \param [in]   nlri           Reference to parsed NLRI struct
 * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
 */
void MPReachAttr::parseAfiIPv6(mp_reach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data) {
    u_char      ipv6_raw[16];
    char        ipv6_char[40];

    bzero(ipv6_raw, sizeof(ipv6_raw));

    /*
     * Decode based on SAFI
     */
    switch (nlri.safi) {
        case bgp::BGP_SAFI_UNICAST: // Unicast IPv6 address prefix

            // Next-hop is an IPv6 address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ipv6_raw, nlri.next_hop, 16);
            else
                memcpy(ipv6_raw, nlri.next_hop, nlri.nh_len);

            inet_ntop(AF_INET6, ipv6_raw, ipv6_char, sizeof(ipv6_char));
            parsed_data.attrs[ATTR_TYPE_NEXT_HOP] = std::string(ipv6_char);

            // Data is an IPv6 address - parse the address and save it
            parseNlriData_v6(nlri.nlri_data, nlri.nlri_len, parsed_data.advertised);
            break;

        default :
            LOG_INFO("%s: MP_REACH AFI=ipv6 SAFI=%d is not implemented yet, skipping for now",
                     peer_addr.c_str(), nlri.safi);
            return;
    }
}

/**
 * Parses mp_reach_nlri and mp_unreach_nlri
 *
 * \details
 *      Will parse the NLRI encoding as defined in RFC4760 Section 5 (NLRI Encoding).
 *
 * \param [in]   data       Pointer to the start of the prefixes to be parsed
 * \param [in]   len        Length of the data in bytes to be read
 * \param [out]  prefixes   Reference to a list<prefix_tuple> to be updated with entries
 */
void MPReachAttr::parseNlriData_v6(u_char *data, uint16_t len, std::list<bgp::prefix_tuple> &prefixes) {
    u_char            ipv6_raw[16];
    char              ipv6_char[40];
    u_char            addr_bytes;
    bgp::prefix_tuple tuple;

    if (len <= 0 or data == NULL)
        return;

    // TODO: Can extend this to support multicast, but right now we set it to unicast v6
    // Set the type for all to be unicast V6
    tuple.type = bgp::PREFIX_UNICAST_V6;
    tuple.isIPv4 = false;

    // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {
        bzero(ipv6_raw, sizeof(ipv6_raw));

        // set the address in bits length
        tuple.len = *data++;

        // Figure out how many bytes the bits requires
        addr_bytes = tuple.len / 8;
        if (tuple.len % 8)
           ++addr_bytes;

        // if the route isn't a default route
        if (addr_bytes > 0) {
            memcpy(ipv6_raw, data, addr_bytes);
            data += addr_bytes;
            read_size += addr_bytes;

            // Convert the IP to string printed format
            inet_ntop(AF_INET6, ipv6_raw, ipv6_char, sizeof(ipv6_char));
            tuple.prefix.assign(ipv6_char);

            // set the raw/binary address
            memcpy(tuple.prefix_bin, ipv6_raw, sizeof(ipv6_raw));

            // Add tuple to prefix list
            prefixes.push_back(tuple);
        }
    }
}


} /* namespace bgp_msg */
