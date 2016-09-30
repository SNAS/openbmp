/*
 * Copyright (c) 2015-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef _OPENBMP_LINKSTATEATTR_H_
#define _OPENBMP_LINKSTATEATTR_H_

#include <cstdint>
#include <cinttypes>
#include <sys/types.h>


#include "Logger.h"
#include "MsgBusInterface.hpp"
#include "UpdateMsg.h"

namespace bgp_msg {

    class MPLinkStateAttr {
    public:
        /**
  * Node Attribute types
  */
        enum ATTR_NODE_TYPES {
            ATTR_NODE_MT_ID                     = 263,    ///< Multi-Topology Identifier (len=variable)
            ATTR_NODE_FLAG                      = 1024,   ///< Node Flag Bits see enum NODE_FLAG_TYPES (len=1)
            ATTR_NODE_OPAQUE,                             ///< Opaque Node Properties (len=variable)
            ATTR_NODE_NAME,                               ///< Node Name (len=variable)
            ATTR_NODE_ISIS_AREA_ID,                       ///< IS-IS Area Identifier (len=variable)
            ATTR_NODE_IPV4_ROUTER_ID_LOCAL,               ///< Local NODE IPv4 Router ID (len=4) (rfc5305/4.3)
            ATTR_NODE_IPV6_ROUTER_ID_LOCAL,                ///< Local NODE IPv6 Router ID (len=16) (rfc6119/4.1)
            ATTR_NODE_SR_CAPABILITIES           = 1034    ///< SR Capabilities
        };

        /**
         * ATTR_NODE_FLAG Bits Types See RFC7752 Section 3.3.1.1 for node flags
         */
        enum NODE_FLAG_TYPES {
            NODE_FLAG_MASK_OVERLOAD                  = 0x80,         ///< Overload Bit (rfc1195)
            NODE_FLAG_MASK_ATTACH                    = 0x40,         ///< Attached Bit (rfc1195)
            NODE_FLAG_MASK_EXTERNAL                  = 0x20,         ///< External Bit (rfc2328)
            NODE_FLAG_MASK_ABR                       = 0x10,         ///< ABR Bit (rfc2328)
            NODE_FLAG_MASK_ROUTER                    = 0x08,         ///< Router bit (rfc5340)
            NODE_FLAG_MASK_V6                        = 0x04          ///< V6 bit (rfc5340)
        };

        enum ATTR_LINK_FLAG_TYPES {
            ATTR_LINK_FLAG_MASK_ADDRESS_FAMILY       = 0x20,
            ATTR_LINK_FLAG_MASK_BACKUP               = 0x10
        };

        /**
         * Link Attribute types
         */
        enum ATTR_LINK_TYPES {
            ATTR_LINK_IPV4_ROUTER_ID_LOCAL      = 1028,         ///< IPv4 Router-ID of local node 134/- (rfc5305/4.3)
            ATTR_LINK_IPV6_ROUTER_ID_LOCAL,                     ///< IPv6 Router-ID of local node 140/- (rfc6119/4.1)
            ATTR_LINK_IPV4_ROUTER_ID_REMOTE,                    ///< IPv4 Router-ID of remote node 134/- (rfc5305/4.3)
            ATTR_LINK_IPV6_ROUTER_ID_REMOTE,                    ///< IPv6 Router-ID of remote node 140- (rfc6119/4.1)
            ATTR_LINK_ADMIN_GROUP               = 1088,         ///< Administrative group (color) 22/3 (rfc5305/3.1)
            ATTR_LINK_MAX_LINK_BW,                              ///< Maximum link bandwidth 22/9 (rfc5305/3.3)
            ATTR_LINK_MAX_RESV_BW,                              ///< Maximum reservable link bandwidth 22/10 (rfc5305/3.5)
            ATTR_LINK_UNRESV_BW,                                ///< Unreserved bandwidth 22/11 (RFC5305/3.6)
            ATTR_LINK_TE_DEF_METRIC,                            ///< TE default metric 22/18
            ATTR_LINK_PROTECTION_TYPE,                          ///< Link protection type 22/20 (rfc5307/1.2)
            ATTR_LINK_MPLS_PROTO_MASK,                          ///< MPLS protocol mask
            ATTR_LINK_IGP_METRIC,                               ///< IGP link metric
            ATTR_LINK_SRLG,                                     ///< Shared risk link group
            ATTR_LINK_OPAQUE,                                   ///< Opaque link attribute
            ATTR_LINK_NAME,                                     ///< Link name
            ATTR_LINK_PEER_AJD_SID,                             ///< Peer Adjacency SID (draft-ietf-idr-bgpls-segment-routing-epe)

            ATTR_LINK_PEER_NODE_SID            = 1101,          ///< Peer Node SID (draft-ietf-idr-bgpls-segment-routing-epe)
            ATTR_LINK_PEER_SET_SID                              ///< Peer Set SID (draft-ietf-idr-bgpls-segment-routing-epe)
        };

        /**
         * MPLS Protocol Mask BIT flags/codes
         */
        enum MPLS_PROTO_MASK_CODES {
            MPLS_PROTO_MASK_LDP                 = 0x80,         ///< Label distribuion protocol (rfc5036)
            MPLS_PROTO_RSVP_TE                  = 0x40          ///< Extension to RSVP for LSP tunnels (rfc3209)
        };

        /**
         * Prefix Attribute types
         */
        enum ATTR_PREFIX_TYPES {
            ATTR_PREFIX_IGP_FLAGS               = 1152,         ///< IGP Flags (len=1)
            ATTR_PREFIX_ROUTE_TAG,                              ///< Route Tag (len=4*n)
            ATTR_PREFIX_EXTEND_TAG,                             ///< Extended Tag (len=8*n)
            ATTR_PREFIX_PREFIX_METRIC,                          ///< Prefix Metric (len=4)
            ATTR_PREFIX_OSPF_FWD_ADDR,                          ///< OSPF Forwarding Address
            ATTR_PREFIX_OPAQUE_PREFIX,                          ///< Opaque prefix attribute (len=variable)
            ATTR_PREFIX_SID_TLV                                 ///< Prefix-SID TLV (len=variable)
        };

        /**
         * Constructor for class
         *
         * \details Handles bgp Extended Communities
         *
         * \param [in]     logPtr       Pointer to existing Logger for app logging
         * \param [in]     peerAddr     Printed form of peer address used for logging
         * \param [out]    parsed_data  Reference to parsed_update_data; will be updated with all parsed data
         * \param [in]     enable_debug Debug true to enable, false to disable
         */
        MPLinkStateAttr(Logger *logPtr, std::string peerAddr,
                UpdateMsg::parsed_update_data *parsed_data, bool enable_debug);
        virtual ~MPLinkStateAttr();


        /**
         * Parse Link State attribute
         *
         * \details Will handle parsing the link state attributes
         *
         * \param [in]   attr_len       Length of the attribute data
         * \param [in]   data           Pointer to the attribute data
         */
        void parseAttrLinkState(int attr_len, u_char *data);



    private:
        bool             debug;                           ///< debug flag to indicate debugging
        Logger           *logger;                         ///< Logging class pointer
        std::string      peer_addr;                       ///< Printed form of the peer address for logging

        UpdateMsg::parsed_update_data *parsed_data;       ///< Parsed data structure

        #define IEEE_INFINITY         0x7F800000
        #define MINUS_INFINITY        (int32_t)0x80000000L
        #define PLUS_INFINITY         0x7FFFFFFF
        #define IEEE_NUMBER_WIDTH       32        /* bits in number */
        #define IEEE_EXP_WIDTH          8         /* bits in exponent */
        #define IEEE_MANTISSA_WIDTH     (IEEE_NUMBER_WIDTH - 1 - IEEE_EXP_WIDTH)
            #define IEEE_SIGN_MASK          0x80000000
        #define IEEE_EXPONENT_MASK      0x7F800000
        #define IEEE_MANTISSA_MASK      0x007FFFFF

        #define IEEE_IMPLIED_BIT        (1 << IEEE_MANTISSA_WIDTH)
        #define IEEE_INFINITE           ((1 << IEEE_EXP_WIDTH) - 1)
        #define IEEE_BIAS               ((1 << (IEEE_EXP_WIDTH - 1)) - 1)

        /*******************************************************************************//*
         * Parse Link State attribute TLV
         *
         * \details Will handle parsing the link state attribute
         *
         * \param [in]   attr_len       Length of the attribute data
         * \param [in]   data           Pointer to the attribute data
         *
         * \returns length of the TLV attribute parsed
         */
        int parseAttrLinkStateTLV(int attr_len, u_char *data);
        
        /*******************************************************************************//*
         * Parse flags to string
         *
         * \details   Will parse flags from binary representation to string. 
         *            Example: flags "std::array<std::string, 8>{'I', '\0', 'H'}"" 
         *            means data "11100000" would be parsed as "IH"
         *
         * \param [in]   flags          Array of flags
         * \param [in]   data           Pointer to the attribute data
         *
         * \returns string with flags
         */
        std::string parse_flags_to_string(u_char data, std::array<std::string, 8> flags);

        uint32_t ieee_float_to_kbps(int32_t float_val);
    };

}

#endif //_OPENBMP_LINKSTATEATTR_H_
