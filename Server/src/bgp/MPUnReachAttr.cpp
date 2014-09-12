/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "MPUnReachAttr.h"
#include "UpdateMsg.h"

#include <arpa/inet.h>

namespace bgp_msg {

/**
 * Constructor for class
 *
 * \details Handles BGP MP UnReach NLRI
 *
 * \param [in]     logPtr       Pointer to existing Logger for app logging
 * \param [in]     pperAddr     Printed form of peer address used for logging
 * \param [in]     enable_debug Debug true to enable, false to disable
 */
MPUnReachAttr::MPUnReachAttr(Logger *logPtr, std::string peerAddr, bool enable_debug) {
    logger = logPtr;
    debug = enable_debug;

    peer_addr = peerAddr;
}

MPUnReachAttr::~MPUnReachAttr() {
}

/**
 * Parse the MP_UNREACH NLRI attribute data
 *
 * \details
 *      Will parse the MP_REACH_NLRI data passed.  Parsed data will be stored
 *      in parsed_data.
 *
 *      \see RFC4760 for format details.
 *
 * \param [in]   attr_len       Length of the attribute data
 * \param [in]   data           Pointer to the attribute data
 * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
 */
void MPUnReachAttr::parseUnReachNlriAttr(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_udpate_data &parsed_data) {
    mp_unreach_nlri nlri;
    /*
     * Set the MP Unreach NLRI struct
     */
    // Read address family
    memcpy(&nlri.afi, data, 2); data += 2; attr_len -= 2;
    bgp::SWAP_BYTES(&nlri.afi);                     // change to host order

    nlri.safi = *data++; attr_len--;                 // Set the SAFI - 1 octe
    nlri.nlri_data = data;                          // Set pointer position for nlri data
    nlri.nlri_len = attr_len;                       // Remaining attribute length is for NLRI data

    /*
     * Make sure the parsing doesn't exceed buffer
     */
    if (attr_len < 0) {
        LOG_NOTICE("%s: MP_UNREACH NLRI data length is larger than attribute data length, skipping parse", peer_addr.c_str());
        return;
    }

    SELF_DEBUG("%S: afi=%d safi=%d", peer_addr.c_str(), nlri.afi, nlri.safi);

    /*
     * NLRI data depends on the AFI & SAFI
     *  Parse data based on AFI + SAFI
     */
    parseAfi(nlri, parsed_data);
}


/**
 * MP UnReach NLRI parse based on AFI
 *
 * \details Will parse the nlri data based on AFI.  A call to the specific SAFI method will
 *          be performed to further parse the message.
 *
 * \param [in]   nlri           Reference to parsed Unreach NLRI struct
 * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
 */
void MPUnReachAttr::parseAfi(mp_unreach_nlri &nlri, UpdateMsg::parsed_udpate_data &parsed_data) {

    switch (nlri.afi) {
        case bgp::BGP_AFI_IPV6 :  // IPv6
            parseAfiUnicstIPv6(nlri, parsed_data);
            break;

        // TODO: Add other AFI parsing

        default : // Unknown
            LOG_INFO("%s: MP_UNREACH AFI=%d is not implemented yet, skipping", peer_addr.c_str(), nlri.afi);
            return;
    }
}

/**
 * MP UnReach NLRI parse for BGP_AFI_IPV6
 *
 * \details Will handle parsing the SAFI's for address family ipv6
 *
 * \param [in]   nlri           Reference to parsed Unreach NLRI struct
 * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
 */
void MPUnReachAttr::parseAfiUnicstIPv6(mp_unreach_nlri &nlri, UpdateMsg::parsed_udpate_data &parsed_data) {

    /*
     * Decode based on SAFI
     */
    switch (nlri.safi) {
        case bgp::BGP_SAFI_UNICAST: // Unicast IPv6 address prefix

            // Data is an IPv6 address - parse the address and save it
            MPReachAttr::parseNlriData_v6(nlri.nlri_data, nlri.nlri_len, parsed_data.withdrawn);
            break;

        default :
            LOG_INFO("%s: MP_UNREACH AFI=ipv6 SAFI=%d is not implemented yet, skipping for now",
                     peer_addr.c_str(), nlri.afi, nlri.safi);
            return;
    }
}

} /* namespace bgp_msg */
