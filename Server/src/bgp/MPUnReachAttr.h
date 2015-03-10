/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef MPUNREACHATTR_H_
#define MPUNREACHATTR_H_

#include "bgp_common.h"
#include "Logger.h"
#include <list>
#include <string>

#include "MPReachAttr.h"

namespace bgp_msg {

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
        uint16_t       nlri_len;            ///< Not in RFC header; length of the NRLI data
    };

    /**
     * Constructor for class
     *
     * \details Handles bgp MP_UNREACH attributes
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     pperAddr     Printed form of peer address used for logging
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    MPUnReachAttr(Logger *logPtr, std::string peerAddr, bool enable_debug=false);
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
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void parseUnReachNlriAttr(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_update_data &parsed_data);

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging

    /**
     * MP UnReach NLRI parse based on AFI
     *
     * \details Will parse the next-hop and nlri data based on AFI.  A call to
     *          the specific SAFI method will be performed to further parse the message.
     *
     * \param [in]   nlri           Reference to parsed UnReach NLRI struct
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAfi(mp_unreach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data);

    /**
     * MP Reach NLRI parse for BGP_AFI_IPV6
     *
     * \details Will handle the SAFI and parsing of AFI IPv6
     *
     * \param [in]   nlri           Reference to parsed UnReach NLRI struct
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAfiIPv6(mp_unreach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data);
};

} /* namespace bgp_msg */

#endif /* MPUNREACHATTR_H_ */
