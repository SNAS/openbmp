/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef PARSE_BGP_LIB_H_
#define PARSE_BGP_LIB_H_

#include <string>
#include <list>
#include <map>
#include <array>
#include <boost/xpressive/xpressive.hpp>
#include <boost/exception/all.hpp>
#include "Logger.h"

namespace parse_bgp_lib {

    /**
     * Defines the attribute types
     *
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum UPDATE_ATTR_TYPES {
        ATTR_TYPE_ORIGIN=1,
        ATTR_TYPE_AS_PATH,
        ATTR_TYPE_NEXT_HOP,
        ATTR_TYPE_MED,
        ATTR_TYPE_LOCAL_PREF,
        ATTR_TYPE_ATOMIC_AGGREGATE,
        ATTR_TYPE_AGGREGATOR,
        ATTR_TYPE_COMMUNITIES,
        ATTR_TYPE_ORIGINATOR_ID,
        ATTR_TYPE_CLUSTER_LIST,
        ATTR_TYPE_DPA,
        ATTR_TYPE_ADVERTISER,
        ATTR_TYPE_RCID_PATH,
        ATTR_TYPE_MP_REACH_NLRI=14,
        ATTR_TYPE_MP_UNREACH_NLRI,
        ATTR_TYPE_EXT_COMMUNITY=16,
        ATTR_TYPE_AS4_PATH=17,
        ATTR_TYPE_AS4_AGGREGATOR=18,

        ATTR_TYPE_AS_PATHLIMIT=21,              // Deprecated - draft-ietf-idr-as-pathlimit, JunOS will send this

        ATTR_TYPE_IPV6_EXT_COMMUNITY=25,
        ATTR_TYPE_AIGP,                         ///< RFC7311 - Accumulated IGP metric

        ATTR_TYPE_BGP_LS=29,                    // BGP LS attribute draft-ietf-idr-ls-distribution

        ATTR_TYPE_BGP_LINK_STATE_OLD=99,        // BGP link state Older
        ATTR_TYPE_BGP_ATTRIBUTE_SET=128,

        /*
         * Below attribute types are for internal use only... These are derived/added based on other attributes
         */
        ATTR_TYPE_INTERNAL_AS_COUNT=9000,        // AS path count - number of AS's
        ATTR_TYPE_INTERNAL_AS_ORIGIN             // The AS that originated the entry
    };



    /**
     * defines whether the Attribute Length is one octet
     *      (if set to 0) or two octets (if set to 1)
     *
     * \details
     *         If the Extended Length bit of the Attribute Flags octet is set
     *         to 0, the third octet of the Path Attribute contains the length
     *         of the attribute data in octets.
     *
     *         If the Extended Length bit of the Attribute Flags octet is set
     *         to 1, the third and fourth octets of the path attribute contain
     *         the length of the attribute data in octets.
     */
    #define ATTR_FLAG_EXTENDED(flags)   ( flags & 0x10 )

    enum BGP_AFI {
        BGP_AFI_IPV4 = 1,
        BGP_AFI_IPV6 = 2,
        BGP_AFI_BGPLS = 16388
    };


    /**
     * Defines the BGP subsequent address-families (SAFI)
     *      http://www.iana.org/assignments/safi-namespace/safi-namespace.xhtml
     */
    enum BGP_SAFI {
        BGP_SAFI_UNICAST = 1,
        BGP_SAFI_MULTICAST = 2,
        BGP_SAFI_NLRI_LABEL = 4,          // RFC3107
        BGP_SAFI_MCAST_VPN,             // RFC6514
        BGP_SAFI_VPLS = 65,               // RFC4761, RFC6074
        BGP_SAFI_MDT,                   // RFC6037
        BGP_SAFI_4over6,                // RFC5747
        BGP_SAFI_6over4,                // yong cui
        BGP_SAFI_EVPN = 70,               // draft-ietf-l2vpn-evpn
        BGP_SAFI_BGPLS = 71,              // draft-ietf-idr-ls-distribution
        BGP_SAFI_MPLS = 128,              // RFC4364
        BGP_SAFI_MCAST_MPLS_VPN,        // RFC6513, RFC6514
        BGP_SAFI_RT_CONSTRAINTS = 132      // RFC4684
    };

/*
 * Define the library BGP attrs, well known attributes keep the same value as defined by iana,
 * followed by the library names of the attrs.
 * NOTE: The positions of these elements of these enums and names MUST be the same
 */
    enum BGP_LIB_ATTRS {
        //Common attributes
        LIB_ATTR_ORIGIN,
        LIB_ATTR_AS_PATH,
        LIB_ATTR_NEXT_HOP,
        LIB_ATTR_MED,
        LIB_ATTR_LOCAL_PREF,
        LIB_ATTR_ATOMIC_AGGREGATE,
        LIB_ATTR_AGGREGATOR,
        LIB_ATTR_COMMUNITIES,
        LIB_ATTR_ORIGINATOR_ID,
        LIB_ATTR_CLUSTER_LIST,
        LIB_ATTR_EXT_COMMUNITY,
        LIB_ATTR_IPV6_EXT_COMMUNITY,

        //Linkstate Node attributes
        LIB_ATTR_LS_MT_ID,
        LIB_ATTR_LS_FLAGS,
        LIB_ATTR_LS_NODE_NAME,
        LIB_ATTR_LS_ISIS_AREA_ID,
        LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4,
        LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6,
        LIB_ATTR_LS_SR_CAPABILITIES_TLV,

        //Linkstate Link attributes
        LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4,
        LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6,
        LIB_ATTR_LS_ADMIN_GROUP,
        LIB_ATTR_LS_MAX_LINK_BW,
        LIB_ATTR_LS_MAX_RESV_BW,
        LIB_ATTR_LS_UNRESV_BW,
        LIB_ATTR_LS_TE_DEF_METRIC,
        LIB_ATTR_LS_PROTECTION_TYPE,
        LIB_ATTR_LS_MPLS_PROTO_MASK,
        LIB_ATTR_LS_IGP_METRIC,
        LIB_ATTR_LS_SRLG,
        LIB_ATTR_LS_OPAQUE,
        LIB_ATTR_LS_LINK_NAME,
        LIB_ATTR_LS_ADJACENCY_SID,
        LIB_ATTR_LS_PEER_EPE_NODE_SID,
        LIB_ATTR_LS_PEER_EPE_ADJ_SID,
        LIB_ATTR_LS_PEER_EPE_SET_SID,

        //Linkstate Prefix attributes
        LIB_ATTR_LS_PREFIX_IGP_FLAGS,
        LIB_ATTR_LS_ROUTE_TAG,
        LIB_ATTR_LS_EXTENDED_TAG,
        LIB_ATTR_LS_PREFIX_METRIC,
        LIB_ATTR_LS_OSPF_FWD_ADDR,
        LIB_ATTR_LS_OPAQUE_PREFIX,
        LIB_ATTR_LS_PREFIX_SID,


        LIB_ATTR_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_ATTR_MAX> parse_bgp_lib_attr_names = {
            //Common attributes
            std::string("origin"),
            "asPath",
            "nextHop",
            "med",
            "localPref",
            "atomicAggregate",
            "aggregator",
            "communities",
            "originator",
            "clusterList",
            "extendedCommunities",
            "ipv6ExtendedCommunities",

            //Linkstate Node attributes
            "linkstateMtId",
            "linkstateFlags",
            "linkstateNodeName",
            "linkstateIsisAreaId",
            "linkstateLocalRouterIdIpv4",
            "linkstateLocalRouterIdIpv6",
            "linkstateSrCapabilitiesTlv",

            //Linkstate Link attributes
            "linkstateRemoteRouterIdIpv4",
            "linkstateRemoteRouterIdIpv6",
            "linkstateAdminGroup",
            "linkstateMaxLinkBw",
            "linkstateMaxReservedBw",
            "linkstateUnreservedBw",
            "linkstateTeDefaultMetric",
            "linkstateProtectionType",
            "linkstateMplsProtoMask",
            "linkstateIgpMetric",
            "linkstateSrlg",
            "linkstateOpaque",
            "linkstateLinkName",
            "linkstateAdjacencySid",
            "linkstatePeerEpeNodeSid",
            "linkstatePeerEpeAdjSid",
            "linkstatePeerEpeSetSid",

            //Linkstate Prefix attributes
            "linkstatePrefixIgpFlags",
            "linkstateRouteTag",
            "linkstatExtendedTag",
            "linkstatePrefixMetric",
            "linkstateOspfForwardingAddr",
            "linkstateOpaquePrefix",
            "linkstatePrefixSid"
    };

/*
* Define the library BGP nlri fields, followed by the library names of the prefix fields.
* NOTE: The positions of these elements of these enums and names MUST be the same
*/
    enum BGP_LIB_NLRI {
        LIB_NLRI_PREFIX,
        LIB_NLRI_PREFIX_BIN,
        LIB_NLRI_PREFIX_LENGTH,
        LIB_NLRI_PATH_ID,
        LIB_NLRI_LABELS,

        LIB_NLRI_LS_PROTOCOL,
        LIB_NLRI_LS_ROUTING_ID, //Identified in Linkstate NLRI header
        LIB_NLRI_LS_ASN_LOCAL,
        LIB_NLRI_LS_BGP_LS_ID_LOCAL,
        LIB_NLRI_LS_OSPF_AREA_ID_LOCAL,
        LIB_NLRI_LS_IGP_ROUTER_ID_LOCAL,
        LIB_NLRI_LS_BGP_ROUTER_ID_LOCAL,
        LIB_NLRI_LS_ASN_REMOTE,
        LIB_NLRI_LS_BGP_LS_ID_REMOTE,
        LIB_NLRI_LS_OSPF_AREA_ID_REMOTE,
        LIB_NLRI_LS_IGP_ROUTER_ID_REMOTE,
        LIB_NLRI_LS_BGP_ROUTER_ID_REMOTE,

        LIB_NLRI_LS_LINK_ID,
        LIB_NLRI_LS_IPV4_INTF_ADDR,
        LIB_NLRI_LS_IPV6_INTF_ADDR,
        LIB_NLRI_LS_IPV4_NEIGHBOR_ADDR,
        LIB_NLRI_LS_IPV6_NEIGHBOR_ADDR,
        LIB_NLRI_LS_MT_ID,

        LIB_NLRI_LS_OSPF_ROUTE_TYPE,
        LIB_NLRI_LS_IP_REACH_PREFIX,
        LIB_NLRI_LS_IP_REACH_PREFIX_LENGTH,
        LIB_NLRI_LS_IP_REACH_PREFIX_BCAST,

        LIB_NLRI_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_NLRI_MAX> parse_bgp_lib_nlri_names = {
            std::string("prefix"),
            "prefixBinary",
            "prefixLength",
            "pathId",
            "labels",

            "linkstateProtocol",
            "linkstateRoutingId",
            "linkstateAsnLocal",
            "linkstateBgpLsIdLocal",
            "linkstateOspfAreaIdLocal",
            "linkstateIgpRouterIdLocal",
            "linkstateBgpRouterIdLocal",
            "linkstateAsnRemote",
            "linkstateBgpLsIdRemote",
            "linkstateOspfAreaIdRemote",
            "linkstateIgpRouterIdRemote",
            "linkstateBgpRouterIdRemote",

            "linkstateLinkId",
            "linkstateIpv4InterfaceAddr",
            "linkstateIpv6InterfaceAddr",
            "linkstateIpv4NeighborAddr",
            "linkstateIpv6NeighborAddr",
            "linkstateMtId",

            "linkstateOspfRouteType",
            "linkstateIpReachPrefix",
            "linkstateIpReachPrefixLength",
            "linkstateIpReachPrefixBcast",

    };

    /**
     * ENUM to define the prefix type used for prefix nlri in case AFI/SAFI is not sufficient, eg, BGP-LS nodes/link/prefix
     */
    enum BGP_LIB_NLRI_TYPES {
        LIB_NLRI_TYPE_NONE,
        LIB_NLRI_TYPE_LS_NODE,
        LIB_NLRI_TYPE_LS_LINK,
        LIB_NLRI_TYPE_LS_PREFIX,
    };

    /*********************************************************************//**
     * Simple function to swap bytes around from network to host or
     *  host to networking.  This method will convert any size byte variable,
     *  unlike ntohs and ntohl.
     *
     * @param [in/out] var   Variable containing data to update
     * @param [in]     size  Size of var - Default is size of var
     *********************************************************************/
    template<typename VarT>
    void SWAP_BYTES(VarT *var, int size = sizeof(VarT)) {
        if (size <= 1)
            return;

        u_char *v = (u_char *) var;

        // Allocate a working buffer
        u_char buf[size];

        // Make a copy
        memcpy(buf, var, size);

        int i2 = 0;
        for (int i = size - 1; i >= 0; i--)
            v[i2++] = buf[i];

    }

    /**
     * \class   parseBgpLib
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

        /*
         * Internal structure consisting of
         * iana or standard codepoint, included well known BGP attribute types and other types defined in an ietf draft
         * eg., BGP-LS TLV types carried over from IGP
         */
        struct parse_bgp_lib_data {
            uint32_t    official_type;
            std::string name;
            std::list<std::string> value;
        };

        typedef std::map<parse_bgp_lib::BGP_LIB_ATTRS, parse_bgp_lib_data> attr_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_NLRI, parse_bgp_lib_data> nlri_map;

        struct parse_bgp_lib_nlri {
            parse_bgp_lib::BGP_AFI afi;
            parse_bgp_lib::BGP_SAFI safi;
            parse_bgp_lib::BGP_LIB_NLRI_TYPES type;
            nlri_map nlri;
        };

        struct parsed_update {
            std::list<parse_bgp_lib_nlri> nlri_list;
            std::list<parse_bgp_lib_nlri> withdrawn_nlri_list;
            attr_map attrs;
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
         */
        void enableAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi);

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
        void disableAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi);


        /**
         * Addpath capability for a peer
         *
         * \details
         * Get Addpath capability for a peer which sent the Update message to be parsed
         * \param [in]   afi           AFI
         * \param [in]   safi          SAFI
         *
         */
        bool getAddpathCapability(parse_bgp_lib::BGP_AFI afi, parse_bgp_lib::BGP_SAFI safi);



        /**
         * 4-octet capability for a peer
         *
         * \details
         * Enable 4-octet capability for a peer which sent the Update message to be parsed
         *
         */
        void enableFourOctetCapability();

        /**
         * 4-octet capability for a peer
         *
         * \details
         * Disable 4-octet capability for a peer which sent the Update message to be parsed
         *
         */
        void disableFourOctetCapability();

    private:
        //TODO: Remove
        bool debug;                           ///< debug flag to indicate debugging
        Logger *logger;                         ///< Logging class pointer
        char    asn_octet_size;

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
            BGP_SAFI_RT_CONSTRAINTS_INTERNAL,        // RFC4684
            BGP_SAFI_MAX_INTERNAL
        };

        /*
         * An array to track if AddPath is enabled for a AFI/SAFI, this should be populated
         */
        bool addPathCap[BGP_AFI_MAX_INTERNAL][BGP_SAFI_MAX_INTERNAL] = {{0}};


        /**
         * Get internal afi
         *
         * \details
         * Given the official AFI, get the lib internal AFI
         * \param [in]   afi           AFI
         *
         * \returns internal AFI
         */
        BGP_AFI_INTERNAL getInternalAfi(parse_bgp_lib::BGP_AFI oafi);


        /**
         * Get internal safi
         *
         * \details
         * Given the official SAFI, get the lib internal SAFI
         * \param [in]  safi           SAFI
         *
         * \returns internal SAFI
         */
        BGP_SAFI_INTERNAL getInternalSafi(parse_bgp_lib::BGP_SAFI osafi);

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

        /**
         * Parse attribute data based on attribute type
         *
         * \details
         *      Parses the attribute data based on the passed attribute type.
         *      Parsed_data will be updated based on the attribute data parsed.
         *
         * \param [in]   attr_type      Attribute type
         * \param [in]   attr_len       Length of the attribute data
         * \param [in]   data           Pointer to the attribute data
         * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
         */
        void parseAttrData(u_char attr_type, uint16_t attr_len, u_char *data, parsed_update &update);

        /**
         * Parse attribute AGGREGATOR data
         *
         * \param [in]   attr_len       Length of the attribute data
         * \param [in]   data           Pointer to the attribute data
         * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
         */
        void parseAttrDataAggregator(uint16_t attr_len, u_char *data, parsed_update &update);

        /**
         * Parse attribute AS_PATH data
         *
         * \param [in]   attr_len       Length of the attribute data
         * \param [in]   data           Pointer to the attribute data
         * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
         */
        void parseAttrDataAsPath(uint16_t attr_len, u_char *data, parsed_update &update);
    };

} /* namespace parse_bgp_lib */

#endif /* PARSE_BGP_LIB_H_ */
