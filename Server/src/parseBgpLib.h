/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef PARSE_BGP_LIB_H_
#define PARSE_BHP_LIB_H_

#include <string>
#include <list>
#include <map>
#include <boost/xpressive/xpressive.hpp>
#include <boost/exception/all.hpp>

//TODO:Remove
#include "Logger.h"

namespace parse_bgp_lib {

enum BGP_AFI {
    BGP_AFI_IPV4=1,
    BGP_AFI_IPV6=2,
    BGP_AFI_BGPLS=16388
};


/**
 * Defines the BGP subsequent address-families (SAFI)
 *      http://www.iana.org/assignments/safi-namespace/safi-namespace.xhtml
 */
enum BGP_SAFI {
    BGP_SAFI_UNICAST=1,
    BGP_SAFI_MULTICAST=2,
    BGP_SAFI_NLRI_LABEL=4,          // RFC3107
    BGP_SAFI_MCAST_VPN,             // RFC6514
    BGP_SAFI_VPLS=65,               // RFC4761, RFC6074
    BGP_SAFI_MDT,                   // RFC6037
    BGP_SAFI_4over6,                // RFC5747
    BGP_SAFI_6over4,                // yong cui
    BGP_SAFI_EVPN=70,               // draft-ietf-l2vpn-evpn
    BGP_SAFI_BGPLS=71,              // draft-ietf-idr-ls-distribution
    BGP_SAFI_MPLS=128,              // RFC4364
    BGP_SAFI_MCAST_MPLS_VPN,        // RFC6513, RFC6514
    BGP_SAFI_RT_CONSTRAINS=132      // RFC4684
};


enum BGP_LIB_ATTRS {
    LIB_ATTR_ORIGIN=1,
    LIB_ATTR_ORIGIN_BIN,
    LIB_ATTR_AS_PATH,
    LIB_ATTR_NEXT_HOP,
    LIB_ATTR_MED,
    LIB_ATTR_LOCAL_PREF,
    LIB_ATTR_ATOMIC_AGGREGATE,
    LIB_ATTR_AGGEGATOR,
    LIB_ATTR_COMMUNITIES,
    LIB_ATTR_ORIGINATOR_ID,
    LIB_ATTR_CLUSTER_LIST,
    LIB_ATTR_EXT_COMMUNITY,
};

enum BGP_LIB_NLRI {
    LIB_NLRI_PREFIX = 1,
    LIB_NLRI_PREFIX_BIN,
    LIB_NLRI_PREFIX_LENGTH,
    LIB_NLRI_PATH_ID,
    LIB_NLRI_LABELS,
};

/**
 * ENUM to define the prefix type used for prefix nlri in case AFI/SAFI is not sufficient, eg, BGP-LS nodes/link/prefix
 */
enum BGP_LIB_NLRI_TYPES {
    NLRI_LS_NODE=1,
    NLRI_LS_LINK,
    NLRI_LS_PREFIX,
};

/*********************************************************************//**
 * Simple function to swap bytes around from network to host or
 *  host to networking.  This method will convert any size byte variable,
 *  unlike ntohs and ntohl.
 *
 * @param [in/out] var   Variable containing data to update
 * @param [in]     size  Size of var - Default is size of var
 *********************************************************************/
template <typename VarT>
void SWAP_BYTES(VarT *var, int size=sizeof(VarT)) {
    if (size <= 1)
        return;

    u_char *v = (u_char *)var;

    // Allocate a working buffer
    u_char buf[size];

    // Make a copy
    memcpy(buf, var, size);

    int i2 = 0;
    for (int i=size-1; i >= 0; i--)
        v[i2++] = buf[i];

}

    /**
 * \class   parse_bgp
 *
 * \brief   parse bgp update
 * \details
 *      Parses the bgp message.
 */
class parseBgpLib {
    public:
    /*********************************************************************//**
     * Constructor for class
     ***********************************************************************/
    parseBgpLib(Logger *logPtr, bool enable_debug);
    virtual ~parseBgpLib();

    typedef std::map<parse_bgp_lib::BGP_LIB_ATTRS, std::string> attr_map;
    typedef std::map<parse_bgp_lib::BGP_LIB_NLRI, std::string> nlri_map;

    struct parse_bgp_lib_nlri {
        parse_bgp_lib::BGP_AFI              afi;
        parse_bgp_lib::BGP_SAFI             safi;
        parse_bgp_lib::BGP_LIB_NLRI_TYPES   type;
        nlri_map                            nlri;
    };

    struct parsed_update {
        std::list<parse_bgp_lib_nlri>   nlri_list;
        std::list<parse_bgp_lib_nlri>   withdrawn_nlri_list;
        attr_map                        attr;
    };

    /**
     * Parses the update message
     *
     * \details
     * Parse BGP update message
     * \param [in]   data           Pointer to raw bgp payload data
     * \param [in]   size           Size of the data available to read; prevent overrun when reading
     * \param [in]  parsed_update  Reference to parsed_update; will be updated with all parsed data
     *
     * \return ZERO is error, otherwise a positive value indicating the number of bytes read from update message
     */
    size_t parseBgpUpdate(u_char *data, size_t size, parsed_update &update);

    /**
     * Addpath capability for a peer
     *
     * \details
     * Enable Addpath capability for a peer which sent the Update message to be parsed
     * \param [in]   afi           AFI
     * \param [in]   safi          SAFI
     *
     * \return void
     */
    void enableAddpathCapability(parse_bgp_lib::BGP_AFI, parse_bgp_lib::BGP_SAFI);

    /**
     * Addpath capability for a peer
     *
     * \details
     * Disable Addpath capability for a peer which sent the Update message to be parsed
     * \param [in]   afi           AFI
     * \param [in]   safi          SAFI
     *
     * \return void
     */
    void disableAddpathCapability(parse_bgp_lib::BGP_AFI, parse_bgp_lib::BGP_SAFI);


private:
    //TODO: Remove
    bool                    debug;                           ///< debug flag to indicate debugging
    Logger                  *logger;                         ///< Logging class pointer

    /**
    * Defines the BGP address-families (AFI) internal numbering
    */
    enum BGP_AFI_INTERNAL {
        BGP_AFI_IPV4_INTERNAL,
        BGP_AFI_IPV6_INTERNAL,
        BGP_AFI_BGPLS_INTERNAL,
        BGP_AFI_MAX_INTERNAL
    };

/**
 * Defines the BGP subsequent address-families (SAFI) internal numbering
 */
    enum BGP_SAFI_INTERNAL {
        BGP_SAFI_UNICAST_INTERNAL,
        BGP_SAFI_MULTICAST_INTERNAL,
        BGP_SAFI_NLRI_LABEL_INTERNAL,           // RFC3107
        BGP_SAFI_MCAST_VPN_INTERNAL,            // RFC6514
        BGP_SAFI_VPLS_INTERNAL,                 // RFC4761, RFC6074
        BGP_SAFI_MDT_INTERNAL,                  // RFC6037
        BGP_SAFI_4over6_INTERNAL,               // RFC5747
        BGP_SAFI_6over4_INTERNAL,               // yong cui
        BGP_SAFI_EVPN_INTERNAL,                 // draft-ietf-l2vpn-evpn
        BGP_SAFI_BGPLS_INTERNAL,                // draft-ietf-idr-ls-distribution
        BGP_SAFI_MPLS_INTERNAL,                 // RFC4364
        BGP_SAFI_MCAST_MPLS_VPN_INTERNAL,       // RFC6513, RFC6514
        BGP_SAFI_RT_CONSTRAINS_INTERNAL,        // RFC4684
        BGP_SAFI_MAX_INTERNAL
    };

    /*
     * An array to track if AddPath is enabled for a AFI/SAFI, this should be populated
     */
    bool addPathCap[BGP_AFI_MAX_INTERNAL][BGP_SAFI_MAX_INTERNAL] = { {0} };

    /**
     * Parses the BGP attributes in the update
     *
     * \details
     *     Parses all attributes.  Decoded values are updated in 'parsed_data'
     *
     * \param [in]   data       Pointer to the start of the prefixes to be parsed
     * \param [in]   len        Length of the data in bytes to be read
     * \param [in]  parsed_data    Reference to parsed_update;
     */
    void parseBgpAttr(u_char *data, uint16_t len, parsed_update &update);

    /**
     * Parses the BGP prefixes (advertised and withdrawn) in the update
     *
     * \details
     *     Parses all attributes.  Decoded values are updated in 'parsed_data'
     *
     * \param [in]   data       Pointer to the start of the prefixes to be parsed
     * \param [in]   len        Length of the data in bytes to be read
     * \param [in]   prefix_list Reference to parsed_update_data;
     */
    void parseBgpNlri_v4(u_char *data, uint16_t len, std::list<parse_bgp_lib_nlri> &nlri_list);

};

} /* namespace parse_bgp_lib */

#endif /* PARSE_BGP_LIB_H_ */
