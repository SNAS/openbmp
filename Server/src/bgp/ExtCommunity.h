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

namespace bgp_msg {

/**
 * \class   ExtCommunity
 *
 * \brief   BGP attribute extended community parser
 * \details This class parses extended community attributes.
 *          It can be extended to create attributes messages.
 *          See http://www.iana.org/assignments/bgp-extended-communities/bgp-extended-communities.xhtml
 */
class ExtCommunity {
public:
    /**
     * Defines the BGP Extended communities Types
     *      http://www.iana.org/assignments/bgp-extended-communities/bgp-extended-communities.xhtml
     */
    enum EXT_COMM_TYPES {

        // Either Transitive or non-Transitive high order byte Types
        EXT_TYPE_2OCTET_AS = 0,                      ///< Transitive Two-Octet AS-Specific (RFC7153)
        EXT_TYPE_IPV4,                               ///< Transitive IPv4-Address-Specific (RFC7153)
        EXT_TYPE_4OCTET_AS,                          ///< Transitive Four-Octet AS-Specific (RFC7153)

        EXT_TYPE_OPAQUE,                             ///< Transitive Opaque (RFC7153)
        EXT_TYPE_QOS_MARK,                           ///< QoS Marking (Thomas_Martin_Knoll)
        EXT_TYPE_COS_CAP,                            ///< CoS Capability (Thomas_Martin_Knoll)
        EXT_TYPE_EVPN,                               ///< EVPN (RFC7153)

        EXT_TYPE_FLOW_SPEC=8,                        ///< Flow spec redirect/mirror to IP next-hop (draft-simpson-idr-flowspec-redirect)

        EXT_TYPE_GENERIC=0x80,                       ///< Generic Transitive Experimental Use (RFC7153)
        EXT_TYPE_GENERIC_IPV4=0x81,                  ///< Generic/Experimental Use IPv4 (draft-ietf-idr-flowspec-redirect-rt-bis)
        EXT_TYPE_GENERIC_4OCTET_AS                   ///< Generic/Experimental Use 4Octet AS (draft-ietf-idr-flowspec-redirect-rt-bis)
    };

    /**
     * Defines the BGP Extended community subtype for EXT_TYPE_TRANS_EVPN
     */
    enum EXT_COMM_SUBTYPE_EVPN {
        EXT_EVPN_MAC_MOBILITY=0,                     ///< MAC Mobility (RFC-ietf-l2vpn-evpn-11)
        EXT_EVPN_MPLS_LABEL,                         ///< ESI MPLS Label (RFC-ietf-l2vpn-evpn-11)
        EXT_EVPN_ES_IMPORT,                          ///< ES Import (RFC-ietf-l2vpn-evpn-11)
        EXT_EVPN_ROUTER_MAC                          ///< EVPN Router’s MAC (draft-sajassi-l2vpn-evpn-inter-subnet-forwarding)
    };

    /**
     * Defines the BGP Extended community subtype for EXT_TYPE_IPV4, EXT_TYPE_4OCTET_AS,
     *  and EXT_TYPE_2OCTET_AS. The subtypes are in common with these.
     */
    enum EXT_COMM_SUBTYPE_IPV4 {
        EXT_COMMON_ROUTE_TARGET=2,                   ///< Route Target (RFC4360/RFC5668)
        EXT_COMMON_ROUTE_ORIGIN,                     ///< Route Origin (RFC5668/RFC5668)
        EXT_COMMON_GENERIC,                          ///< 4-Octet Generic (draft-ietf-idr-as4octet-extcommon-generic-subtype)
        EXT_COMMON_LINK_BANDWIDTH=4,                 ///< 2-Octet Link Bandwidth (draft-ietf-idr-link-bandwidth)

        EXT_COMMON_OSPF_DOM_ID=5,                    ///< OSPF Domain Identifier (RFC4577)
        EXT_COMMON_OSPF_ROUTER_ID=7,                 ///< OSPF Router ID (RFC4577)

        EXT_COMMON_BGP_DATA_COL=8,                   ///< BGP Data Collection (RFC4384)

        EXT_COMMON_SOURCE_AS=9,                      ///< Source AS (RFC6514)

        EXT_COMMON_L2VPN_ID=0x0a,                    ///< L2VPN Identifier (RFC6074)
        EXT_COMMON_VRF_IMPORT=0x0b,                  ///< VRF Route Import (RFC6514)

        EXT_COMMON_CISCO_VPN_ID=0x10,                ///< Cisco VPN-Distinguisher (Eric Rosen)

        EXT_COMMON_IA_P2MP_SEG_NH=0x12               ///< Inter-area P2MP Segmented Next-Hop (draft-ietf-mpls-seamless-mcast)
    };

    /**
    * Defines the BGP Extended community subtype for EXT_TYPE_IPV6 (same type as 2OCTET but attribute type is IPv6 ext comm)
    */
    enum EXT_COMM_SUBTYPE_IPV6 {
        EXT_IPV6_ROUTE_TARGET=2,                    ///< Route Target (RFC5701)
        EXT_IPV6_ROUTE_ORIGIN,                      ///< Route Origin (RFC5701)

        EXT_IPV6_OSPF_ROUTE_ATTRS=4,                ///< OSPFv3 Route Attributes (deprecated) (RFC6565)

        EXT_IPV6_VRF_IMPORT=0x0b,                   ///< VRF Route Import (RFC6514 & RFC6515)

        EXT_IPV6_CISCO_VPN_ID=0x10,                 ///< Cisco VPN-Distinguisher (Eric Rosen)

        EXT_IPV6_UUID_ROUTE_TARGET=0x11,            ///< UUID-based Route Target (Dhananjaya Rao)

        EXT_IPV6_IA_P2MP_SEG_NH=0x12                ///< Inter-area P2MP Segmented Next-Hop (draft-ietf-mpls-seamless-mcast)
    };

    /**
     * Defines the BGP Extended community subtype for EXT_TYPE_OPAQUE
     */
    enum EXT_COMM_SUBTYPE_TRANS_OPAQUE {
        EXT_OPAQUE_ORIGIN_VALIDATION=0,             ///< BGP Origin Validation State (draft-ietf-sidr-origin-validation-signaling)
        EXT_OPAQUE_COST_COMMUNITY=1,                ///< Cost Community (draft-ietf-idr-custom-decision)

        EXT_OPAQUE_CP_ORF=3,                        ///< CP-ORF (draft-ietf-l3vpn-orf-covering-prefixes)

        EXT_OPAQUE_OSPF_ROUTE_TYPE=6,               ///< OSPF Route Type (RFC4577)

        EXT_OPAQUE_COLOR=0x0b,                      ///< Color (RFC5512)
        EXT_OPAQUE_ENCAP,                           ///< Encapsulation (RFC5512)
        EXT_OPAQUE_DEFAULT_GW                       ///< Default Gateway (Yakov Rekhter)
    };

    /**
     * Defines the BGP Extended community subtype for EXT_TYPE_GENERIC
     *      Experimental Use
     */
    enum EXT_COMM_SUBTYPE_GENERIC {
        EXT_GENERIC_OSPF_ROUTE_TYPE=0,               ///< OSPF Route Type (deprecated) (RFC4577)
        EXT_GENERIC_OSPF_ROUTER_ID,                  ///< OSPF Router ID (deprecated) (RFC4577)

        EXT_GENERIC_OSPF_DOM_ID=5,                   ///< OSPF Domain ID (deprecated) (RFC4577)

        EXT_GENERIC_FLOWSPEC_TRAFFIC_RATE=6,         ///< Flow spec traffic-rate (RFC5575)
        EXT_GENERIC_FLOWSPEC_TRAFFIC_ACTION,         ///< Flow spec traffic-action (RFC5575)
        EXT_GENERIC_FLOWSPEC_REDIRECT,               ///< Flow spec traffic redirect (RFC5575)
        EXT_GENERIC_FLOWSPEC_TRAFFIC_REMARK,         ///< Flow spec traffic remarking (RFC5575)

        EXT_GENERIC_LAYER2_INFO                      ///< Layer 2 info (RFC4761)
    };

    /**
     * Extended Community header
     *      RFC4360 size is 8 bytes total (6 for value)
     *      RFC5701 size is 20 bytes total (16 for global admin, 2 for local admin)
     */
    struct extcomm_hdr {
        u_char      high_type;                      ///< Type high byte
        u_char      low_type;                       ///< Type low byte - subtype
        u_char      *value;                         ///<
    };

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
     * Parse the extended communities path attribute (8 byte as per RFC4360)
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
    void parseExtCommunities(int attr_len, u_char *data, UpdateMsg::parsed_update_data &parsed_data);

    /**
     * Parse the extended communities path attribute (20 byte as per RFC5701)
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
    void parsev6ExtCommunities(int attr_len, u_char *data, UpdateMsg::parsed_update_data &parsed_data);

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging

    /**
     * Decode common Type/Subtypes
     *
     * \details
     *      Decodes the common 2-octet, 4-octet, and IPv4 specific common subtypes.
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     * \param [in]   isGlobal4Bytes  True if the global admin field is 4 bytes, false if 2
     * \param [in]   isGlobalIPv4    True if the global admin field is an IPv4 address, false if not
     *
     * \return  Decoded string value
     */
    std::string decodeType_common(const extcomm_hdr &ec_hdr, bool isGlobal4Bytes = false, bool isGlobalIPv4 = false);

    /**
     * Decode Opaque subtypes
     *
     * \details
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     *
     * \return  Decoded string value
     */
    std::string decodeType_Opaque(const extcomm_hdr &ec_hdr);

    /**
     * Decode Generic subtypes
     *
     * \details
     *      Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     * \param [in]   isGlobal4Bytes  True if the global admin field is 4 bytes, false if 2
     * \param [in]   isGlobalIPv4    True if the global admin field is an IPv4 address, false if not
     *
     * \return  Decoded string value
     */
    std::string decodeType_Generic(const extcomm_hdr &ec_hdr,  bool isGlobal4Bytes = false, bool isGlobalIPv4 = false);

    /**
     * Decode IPv6 Specific Type/Subtypes
     *
     * \details
     *      Decodes the IPv6 specific and 2-octet, 4-octet.  This is pretty much the as common for IPv4,
     *      but with some differences. Converts to human readable form.
     *
     * \param [in]   ec_hdr          Reference to the extended community header
     *
     * \return  Decoded string value
     */
    std::string decodeType_IPv6Specific(const extcomm_hdr &ec_hdr);

};

} /* namespace bgp_msg */

#endif /* __EXTCOMMUNITY_H__ */
