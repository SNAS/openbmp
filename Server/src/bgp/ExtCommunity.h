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
            union {
                uint16_t       ga2;  // 2 byte AS
                struct in_addr gaip; // IP address
                uint32_t       ga4;  // same as above unless a struct in_addr is something weird
	    } global_admin;
            union {
                uint16_t       la2;  // 2 byte local admin
	        uint32_t       la4;  // 4 byte local admin
            } local_admin;
            struct {
              uint64_t	       val : 48; // 6 byte opaque value
	    } opaque;
        } value;
     };

    // Need to do something here for IPv6 Specific...
	
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

    typedef std::map<unsigned char, std::string> subtypemap;
    typedef std::map<unsigned char, subtypemap>  typedict;
    typedef std::map<uint16_t, std::string>      v6typemap; 

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging

    static subtypemap create_typedict(void);
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

    // IPv6 specific extended communities use a single type field
    static v6typemap  create_tip6types(void);
    static v6typemap  create_ntip6types(void);

    static const v6typemap tip6types;
    static const v6typemap ntip6types;

    /**
     * Initialize the type/subtype dictionary
     *
     * \details Initializes the type/sybtype dictionary
     */
     void initDict(void);

    /**
     * String representation of type/subtype
     *
     * \details Creates prefixes for string representations of extended community types.
     *
     * \param [in]   type	Extended Community type
     * \param [in]   subtype	Extended Community subtype
     */
    std::string subTypeStr(unsigned char type, unsigned char subtype);

    /**
     * String representation of IPv6 address specific types
     *
     * \details Creates prefixes for string representations of IPv6 specific extended community types.
     *
     * \param [in]   type	IPv6 Specific Extended Community type
     */ 
    std::string ip6TypeStr(uint16_t type);
};

} /* namespace bgp_msg */

#endif /* __EXTCOMMUNITY_H__ */
