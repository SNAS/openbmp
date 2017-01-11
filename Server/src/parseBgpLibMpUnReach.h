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
#ifndef PARSE_BGP_LIB_MPUNREACH_H_
#define PARSE_BGP_LIB_MPUNREACH_H_

#include "parseBgpLib.h"
#include "parseBgpLibMpReach.h"
#include "Logger.h"
#include <list>
#include <string>

namespace parse_bgp_lib {
/**
* \class   MPUnReachAttr
*
* \brief   BGP attribute MP_UNREACH parser
* \details This class parses MP_UNREACH attributes.
*          It can be extended to create attributes messages.
*/
class MPUnReachAttr {
public:

    /**
     * struct defines the MP_UNREACH_NLRI (RFC4760 Section 4)
     */
    struct mp_unreach_nlri {
        uint16_t       afi;                 ///< Address Family Identifier
        unsigned char  safi;                ///< Subsequent Address Family Identifier
        unsigned char  *nlri_data;          ///< NLRI data - Pointer to data (normally does not require freeing)
        uint16_t       nlri_len;            ///< Not in RFC header; length of the NLRI data
    };

    /**
     * Constructor for class
     *
     * \details Handles bgp MP_UNREACH attributes
     *
     * \param [in]      parser                  Pointer to the BGP update parser
     * \param [in]     logPtr                   Pointer to existing Logger for app logging
     * \param [in]     enable_debug             Debug true to enable, false to disable
     */
    MPUnReachAttr(parseBgpLib *parser, Logger *logPtr, bool enable_debug=false);

    virtual ~MPUnReachAttr();

    /**
     * Parse the MP_UNREACH NLRI attribute data
     *
     * \details
     *      Will parse the MP_UNBREACH_NLRI data passed.  Parsed data will be stored
     *      in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     *
     */
    void parseUnReachNlriAttr(int attr_len, u_char *data, parseBgpLib::parsed_update &update);

private:
    bool                    debug;              ///< debug flag to indicate debugging
    Logger                  *logger;            ///< Logging class pointer
    parseBgpLib             *caller;                /// BGP Update class pointer

    /**
     * MP UnReach NLRI parse based on AFI
     *
     * \details Will parse the next-hop and nlri data based on AFI.  A call to
     *          the specific SAFI method will be performed to further parse the message.
     *
     * \param [in]   nlri           Reference to parsed UnReach NLRI struct
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     */
    void parseAfi(mp_unreach_nlri &nlri, parseBgpLib::parsed_update &update);

    /**
     * MP Reach NLRI parse for BGP_AFI_IPV4 & BGP_AFI_IPV6
     *
     * \details Will handle the SAFI and parsing of AFI IPv4 & IPv6
     *
     * \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
     * \param [in]   nlri           Reference to parsed UnReach NLRI struct
     * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     */
    void parseAfi_IPv4IPv6(bool isIPv4, mp_unreach_nlri &nlri, parseBgpLib::parsed_update &update);
};

} /* namespace parse_bgp_lib */

#endif /* PARSE_BGP_LIB_MPUNREACH_H_ */
