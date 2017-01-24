/*
 * Copyright (c) 2014-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * Copyright (c) 2014 Sungard Availability Services and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef PARSE_BGP_LIB_MPEVPN_H_
#define PARSE_BGP_LIB_MPEVPN_H_

#include "parseBgpLib.h"
#include <cstdint>
#include <cinttypes>
#include <sys/types.h>

#include "parseBgpLibMpReach.h"
#include "parseBgpLibMpUnReach.h"

namespace parse_bgp_lib {
    class EVPN {

    public:

        enum EVPN_ROUTE_TYPES {
            EVPN_ROUTE_TYPE_ETHERNET_AUTO_DISCOVERY = 1,
            EVPN_ROUTE_TYPE_MAC_IP_ADVERTISMENT,
            EVPN_ROUTE_TYPE_INCLUSIVE_MULTICAST_ETHERNET_TAG,
            EVPN_ROUTE_TYPE_ETHERNET_SEGMENT_ROUTE,
        };

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
        EVPN(Logger *logPtr, bool isUnreach, std::list<parseBgpLib::parse_bgp_lib_nlri> *nlri_list, bool enable_debug);
        virtual ~EVPN();

        /**
         * Parse Ethernet Segment Identifier
         *
         * \details
         *      Will parse the Segment Identifier. Based on https://tools.ietf.org/html/rfc7432#section-5
         *
         * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
         * \param [out]  parsed_nlri    Parsed NLRI to be filled
         */
        void parseEthernetSegmentIdentifier(u_char *data_pointer, parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &parsed_nlri);

        /**
         * Parse Route Distinguisher
         *
         * \details
         *      Will parse the Route Distinguisher. Based on https://tools.ietf.org/html/rfc4364#section-4.2
         *
         * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
         * \param [out]  parsed_nlri    Parsed NLRI to be filled
         */
        static void parseRouteDistinguisher(u_char *data_pointer, parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri &parsed_nlri);

        /**
         * Parse all EVPN nlri's
         *
         * \details
         *      Parsing based on https://tools.ietf.org/html/rfc7432.  Will process all NLRI's in data.
         *
         * \param [in]   data                   Pointer to the start of the prefixes to be parsed
         * \param [in]   data_len               Length of the data in bytes to be read
         *
         */
        void parseNlriData(u_char *data, uint16_t data_len);


    private:
        bool             debug;                           ///< debug flag to indicate debugging
        Logger           *logger;                         ///< Logging class pointer

        std::list<parseBgpLib::parse_bgp_lib_nlri> *nlri_list;

    };

}
#endif //PARSE_BGP_LIB_MPEVPN_H_
