/*
* Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*
*/

#include "parseBgpLibMpUNReach.h"
#include "parseBgpLib.h"
#include "Logger.h"
#include "parseBgpLibMpLinkstate.h"

#include <arpa/inet.h>

namespace parse_bgp_lib {
/**
* Constructor for class
*
* \details Handles BGP MP UnReach NLRI
*
* \param [in]     logPtr                   Pointer to existing Logger for app logging
* \param [in]     enable_debug             Debug true to enable, false to disable
*/
MPUnReachAttr::MPUnReachAttr(parseBgpLib *parse_lib, Logger *logPtr, bool enable_debug)
        : logger{logPtr}, debug{enable_debug}, caller{parse_lib}{
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
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPUnReachAttr::parseUnReachNlriAttr(int attr_len, u_char *data, parse_bgp_lib::parseBgpLib::parsed_update &update) {
    mp_unreach_nlri nlri;
    /*
     * Set the MP Unreach NLRI struct
     */
    // Read address family
    memcpy(&nlri.afi, data, 2); data += 2; attr_len -= 2;
    parse_bgp_lib::SWAP_BYTES(&nlri.afi);                     // change to host order

    nlri.safi = *data++; attr_len--;                // Set the SAFI - 1 octe
    nlri.nlri_data = data;                          // Set pointer position for nlri data
    nlri.nlri_len = attr_len;                       // Remaining attribute length is for NLRI data

    /*
     * Make sure the parsing doesn't exceed buffer
     */
    if (attr_len < 0) {
        LOG_NOTICE("MP_UNREACH NLRI data length is larger than attribute data length, skipping parse");
        return;
    }

    SELF_DEBUG("afi=%d safi=%d", nlri.afi, nlri.safi);

    if (nlri.nlri_len == 0) {
        LOG_INFO("End-Of-RIB marker (mp_unreach len=0)");

    } else {
        /*
         * NLRI data depends on the AFI & SAFI
         *  Parse data based on AFI + SAFI
         */
        parseAfi(nlri, update);
    }
}


/**
* MP UnReach NLRI parse based on AFI
*
* \details Will parse the nlri data based on AFI.  A call to the specific SAFI method will
*          be performed to further parse the message.
*
* \param [in]   nlri           Reference to parsed Unreach NLRI struct
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPUnReachAttr::parseAfi(mp_unreach_nlri &nlri, parse_bgp_lib::parseBgpLib::parsed_update &update) {

    switch (nlri.afi) {
        case parse_bgp_lib::BGP_AFI_IPV6 :  // IPv6
            parseAfi_IPv4IPv6(false, nlri, update);
            break;

        case parse_bgp_lib::BGP_AFI_IPV4 : // IPv4
            parseAfi_IPv4IPv6(true, nlri, update);
            break;

        case parse_bgp_lib::BGP_AFI_BGPLS : // BGP-LS (draft-ietf-idr-ls-distribution-10)
        {
            MPLinkState ls(logger, &update, debug);
            ls.parseUnReachLinkState(nlri);
            break;
        }


        default : // Unknown
            LOG_INFO("MP_UNREACH AFI=%d is not implemented yet, skipping", nlri.afi);
            return;
    }
}

/**
* MP Reach NLRI parse for BGP_AFI_IPV4 & BGP_AFI_IPV6
*
* \details Will handle the SAFI and parsing of AFI IPv4 & IPv6
*
* \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
* \param [in]   nlri           Reference to parsed Unreach NLRI struct
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPUnReachAttr::parseAfi_IPv4IPv6(bool isIPv4, mp_unreach_nlri &nlri, parse_bgp_lib::parseBgpLib::parsed_update &update) {

    /*
     * Decode based on SAFI
     */
    switch (nlri.safi) {
        case parse_bgp_lib::BGP_SAFI_UNICAST: // Unicast IP address prefix

            // Data is an IP address - parse the address and save it
            MPReachAttr::parseNlriData_IPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, update.withdrawn_nlri_list, caller, debug, logger);
            break;

        case parse_bgp_lib::BGP_SAFI_NLRI_LABEL: // Labeled unicast
            MPReachAttr::parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, update.withdrawn_nlri_list, caller, debug, logger);
            break;

        default :
            LOG_INFO("MP_UNREACH AFI=ipv4/ipv6 (%d) SAFI=%d is not implemented yet, skipping for now", isIPv4, nlri.safi);
            return;
    }
}

} /* namespace parse_bgp_lib */
