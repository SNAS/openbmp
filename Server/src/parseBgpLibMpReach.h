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
#ifndef PARSE_BGP_LIB_MPREACH_H_
#define PARSE_BGP_LIB_MPREACH_H_

#include "parseBgpLib.h"
#include <list>
#include <string>

namespace parse_bgp_lib {
/**
* \class   MPReachAttr
*
* \brief   BGP attribute MP_REACH parser
* \details This class parses MP_REACH attributes.
*          It can be extended to create attributes messages.
*/
class MPReachAttr {
public:
    /**
     * struct defines the MP_REACH_NLRI (RFC4760 Section 3)
     */
    struct mp_reach_nlri {
        uint16_t       afi;                 ///< Address Family Identifier
        unsigned char  safi;                ///< Subsequent Address Family Identifier
        unsigned char  nh_len;              ///< Length of next hop
        unsigned char  *next_hop;           ///< Next hop - Pointer to data (normally does not require freeing)
        unsigned char  reserved;            ///< Reserved

        unsigned char  *nlri_data;          ///< NLRI data - Pointer to data (normally does not require freeing)
        uint16_t       nlri_len;            ///< Not in RFC header; length of the NLRI data
    };

    /**
     * Constructor for class
     *
     * \details Handles bgp MP_REACH attributes
     *
     * \param [in]      parser                  Pointer to the BGP update parser
     * \param [in]     logPtr                   Pointer to existing Logger for app logging
     * \param [in]     enable_debug             Debug true to enable, false to disable
     */
    MPReachAttr(parseBgpLib *parser, Logger *logPtr, bool enable_debug=false);

    virtual ~MPReachAttr();

    /**
     * Parse the MP_REACH NLRI attribute data
     *
     * \details
     *      Will parse the MP_REACH_NLRI data passed.  Parsed data will be stored
     *      in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     *
     */
    void parseReachNlriAttr(int attr_len, u_char *data, parseBgpLib::parsed_update &update);

    /**
     * Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
     *
     * \details
     *      Will parse the NLRI encoding as defined in RFC4760 Section 5 (NLRI Encoding).
     *
     * \param [in]   isIPv4                     True false to indicate if IPv4 or IPv6
     * \param [in]   data                       Pointer to the start of the prefixes to be parsed
     * \param [in]   len                        Length of the data in bytes to be read
     * \param [out]  nlri_list              Reference to a list<parse_bgp_lib_nlri> to be updated with entries
     * \param [in]   parser                  Pointer to the BGP update parser
     */
    static void parseNlriData_IPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                       std::list<parseBgpLib::parse_bgp_lib_nlri> &nlri_list,
                                       parseBgpLib *parser, bool debug, Logger *logger);

    static void parseNlriData_LabelIPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                            std::list<parseBgpLib::parse_bgp_lib_nlri> &nlri_list,
                                            parseBgpLib *parser, bool debug, Logger *logger);

private:
    bool                    debug;                  ///< debug flag to indicate debugging
    Logger                  *logger;               ///< Logging class pointer
    parseBgpLib             *caller;                /// BGP Update class pointer

    /**
     * MP Reach NLRI parse based on AFI
     *
     * \details Will parse the next-hop and nlri data based on AFI.  A call to
     *          the specific SAFI method will be performed to further parse the message.
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     */
    void parseAfi(mp_reach_nlri &nlri, parseBgpLib::parsed_update &update);

    /**
     * MP Reach NLRI parse for BGP_AFI_IPv4 & BGP_AFI_IPV6
     *
     * \details Will handle parsing the SAFI's for address family ipv6 and IPv4
     *
     * \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     */
    void parseAfi_IPv4IPv6(bool isIPv4, mp_reach_nlri &nlri, parseBgpLib::parsed_update &update);
};

} /* namespace parse_bgp_lib */

#endif /* PARSE_BGP_LIB_MPREACH_H_ */