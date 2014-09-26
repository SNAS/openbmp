/*
 * Copyright (c) 2014 Sungard Availability Services and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef __EXTCOMMUNITY_H__
#define __EXTCOMMUNITY_H__

#include "bgp_common.h"
#include "Logger.h"
#include <list>
#include <map>
#include <string>

#include "UpdateMsg.h"

namespace bgp_msg {

/**
 * \class   ExtCommunity
 *
 * \brief   BGP attribute extended community parser
 * \details This class parses extended community attributes.
 *          It can be extended to create attributes messages.
 *          See http://www.iana.org/assignments/bgp-extended-communities/bgp-extended-communities.xhtml
 *
 *          IPv6 specific BGP Extended Community Attributes are not supported at this time (RFC 5701)
 */
class ExtCommunity {
public:
    /**
     * Extended community structure. RFC 4360.
     */
    struct ext_comm {
        unsigned char type;
        unsigned char subtype;
        union {
            struct __attribute__ ((__packed__)) ext_as {
                uint16_t       as;
                uint32_t       val;
            }         ext_as;
            struct __attribute__ ((__packed__)) ext_as4 {
                uint32_t       as;
                uint16_t       val;
            }         ext_as4;
            struct __attribute__ ((__packed__)) ext_ip {
                struct in_addr addr;
                uint16_t       val;
            }         ext_ip;
            struct __attribute__ ((__packed__)) ext_opaq {
                uint16_t       val[3];
	    }         ext_opaque;
            struct __attribute__ ((__packed__)) ext_l2info {
                uint8_t        encap;
                uint8_t        cf;
                uint16_t       mtu;
                uint16_t       reserved;
            }         ext_l2info;
            struct __attribute__ ((__packed__)) evpn_esilabel {
                uint8_t	       flags;
                uint16_t       reserved;
                uint8_t        label[3]; 
            }         evpn_esilabel;
        }       data;
     } __attribute__ ((__packed__));

     struct v6ext_comm {
         unsigned char   type;
         unsigned char   subtype;
         struct in6_addr addr;
         uint16_t	 val;
     } __attribute__ ((__packed__));
	
    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     pperAddr     Printed form of peer address used for logging
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
     ExtCommunity(Logger *logPtr, std::string peerAddr, bool enable_debug=false);
     virtual ~ExtCommunity();
		 
    /**
     * Parse the extended communties path attribute
     *
     * \details
     *     Will parse the EXTENDED COMMUNITIES data passed. Parsed data will be stored
     *     in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void parseExtCommunities(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_update_data &parsed_data);

    /**
     * Parse the extended communties path attribute
     *
     * \details
     *     Will parse the EXTENDED COMMUNITIES data passed. Parsed data will be stored
     *     in parsed_data.
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     *
     */
    void parsev6ExtCommunities(int attr_len, u_char *data, bgp_msg::UpdateMsg::parsed_update_data &parsed_data);

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging

    typedef std::map<unsigned char, std::string> subtypemap;
    typedef std::map<unsigned char, subtypemap>  typedict;

    static typedict   create_typedict(void);
    static subtypemap create_evpnsubtype(void);
    static subtypemap create_t2osubtype(void);
    static subtypemap create_nt2osubtype(void);
    static subtypemap create_t4osubtype(void);
    static subtypemap create_nt4osubtype(void);
    static subtypemap create_tip4subtype(void);
    static subtypemap create_ntip4subtype(void);
    static subtypemap create_topsubtype(void);
    static subtypemap create_ntopsubtype(void);
    static subtypemap create_gtesubtype(void);
    static subtypemap create_tafields(void);

    static typedict   create_typedictv6(void);
    static subtypemap create_tip6subtype(void);
    static subtypemap create_ntip6subtype(void);

    static const typedict   tdict;
    static const subtypemap evpnsubtype;
    static const subtypemap t2osubtype;
    static const subtypemap nt2osubtype;
    static const subtypemap t4osubtype;
    static const subtypemap nt4osubtype;
    static const subtypemap tip4subtype;
    static const subtypemap ntip4subtype;
    static const subtypemap topsubtype;
    static const subtypemap ntopsubtype;
    static const subtypemap gtesubtype;
    static const subtypemap tafields;

    static const typedict   tdict6;
    static const subtypemap tip6subtype;
    static const subtypemap ntip6subtype;

};

} /* namespace bgp_msg */

#endif /* __EXTCOMMUNITY_H__ */
