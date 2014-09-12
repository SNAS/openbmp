/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef MPREACHATTR_H_
#define MPREACHATTR_H_

#include "bgp_common.h"
#include "Logger.h"
#include <list>
#include <string>

#include "UpdateMsg.h"

namespace bgp_msg {

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
        uint16_t       nlri_len;            ///< Not in RFC header; length of the NRLI data
    };

    /**
     * Constructor for class
     *
     * \details Handles bgp MP_REACH attributes
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     pperAddr     Printed form of peer address used for logging
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    MPReachAttr(Logger *logPtr, std::string peerAddr, bool enable_debug=false);
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
     * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
     *
     */
    void parseReachNlriAttr(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_udpate_data &parsed_data);

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
    static void parseNlriData_v6(u_char *data, uint16_t len, std::list<bgp::prefix_tuple> &prefixes);

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging

    /**
     * MP Reach NLRI parse based on AFI
     *
     * \details Will parse the next-hop and nlri data based on AFI.  A call to
     *          the specific SAFI method will be performed to further parse the message.
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
     */
    void parseAfi(mp_reach_nlri &nlri, UpdateMsg::parsed_udpate_data &parsed_data);

    /**
     * MP Reach NLRI parse for BGP_AFI_IPV6 (unicast ipv6)
     *
     * \details Will handle the SAFI and parsing of AFI IPv6 (unicast)
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_data    Reference to parsed_udpate_data; will be updated with all parsed data
     */
    void parseAfiUnicstIPv6(mp_reach_nlri &nlri, UpdateMsg::parsed_udpate_data &parsed_data);
};

} /* namespace bgp_msg */

#endif /* MPREACHATTR_H_ */
