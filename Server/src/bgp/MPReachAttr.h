/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
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
        uint16_t       nlri_len;            ///< Not in RFC header; length of the NLRI data
    };

    /**
     * Constructor for class
     *
     * \details Handles bgp MP_REACH attributes
     *
     * \param [in]     logPtr                   Pointer to existing Logger for app logging
     * \param [in]     pperAddr                 Printed form of peer address used for logging
     * \param [in]     peer_info                Persistent Peer info pointer
     * \param [in]     enable_debug             Debug true to enable, false to disable
     */
    MPReachAttr(Logger *logPtr, std::string peerAddr, BMPReader::peer_info *peer_info, bool enable_debug=false);

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
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void parseReachNlriAttr(int attr_len, u_char *data, UpdateMsg::parsed_update_data &parsed_data);

    /**
     * Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
     *
     * \details
     *      Will parse the NLRI encoding as defined in RFC4760 Section 5 (NLRI Encoding).
     *
     * \param [in]   isIPv4                     True false to indicate if IPv4 or IPv6
     * \param [in]   data                       Pointer to the start of the prefixes to be parsed
     * \param [in]   len                        Length of the data in bytes to be read
     * \param [in]   peer_info                  Persistent Peer info pointer
     * \param [out]  prefixes                   Reference to a list<prefix_tuple> to be updated with entries
     */
    static void parseNlriData_IPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                       BMPReader::peer_info *peer_info,
                                       std::list<bgp::prefix_tuple> &prefixes);

    /**
     * Parses VPN data in nlri (IPv4)
     *
     * \details
     *      Will parse the VPN as defined in https://tools.ietf.org/html/rfc4364
     *
     * \param [in]   isIPv4                     True false to indicate if IPv4 or IPv6
     * \param [in]   nlri                   Reference to MP Reach Nlri object
     * \param [out]  vpn_list               Reference to a list<vpn_tuple> to be updated with entries
     */
    static void parseNLRIData_VPN(bool isIPv4, mp_reach_nlri *nlri, std::list<bgp::vpn_tuple> &vpns);

    static void parseNlriData_LabelIPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                            BMPReader::peer_info *peer_info,
                                            std::list<bgp::prefix_tuple> &prefixes);

private:
    bool                    debug;                  ///< debug flag to indicate debugging
    Logger                   *logger;               ///< Logging class pointer
    std::string             peer_addr;              ///< Printed form of the peer address for logging
    BMPReader::peer_info    *peer_info;

    /**
     * MP Reach NLRI parse based on AFI
     *
     * \details Will parse the next-hop and nlri data based on AFI.  A call to
     *          the specific SAFI method will be performed to further parse the message.
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAfi(mp_reach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data);

    /**
     * MP Reach NLRI parse for BGP_AFI_IPv4 & BGP_AFI_IPV6
     *
     * \details Will handle parsing the SAFI's for address family ipv6 and IPv4
     *
     * \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
     * \param [in]   nlri           Reference to parsed NLRI struct
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAfi_IPv4IPv6(bool isIPv4, mp_reach_nlri &nlri, UpdateMsg::parsed_update_data &parsed_data);
};

} /* namespace bgp_msg */

#endif /* MPREACHATTR_H_ */
