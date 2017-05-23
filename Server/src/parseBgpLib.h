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
#include <bmp/BMPReader.h>
#include "Logger.h"
#include "md5.h"
#include <sys/time.h>

namespace parse_bgp_lib {

    #define BMP_PACKET_BUF_SIZE 68000   ///< Size of the BMP packet buffer (memory)
    #define BMP_ROUTER_DATA_SIZE 4096   ///< Size of the BMP packet buffer (memory)
    #define BMP_MSG_LEN 4          ///< BMP init message header length, does not count the info field


    /**
     * Defines the attribute types
     *
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum UPDATE_ATTR_TYPES {
        ATTR_TYPE_ORIGIN = 1,
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
        ATTR_TYPE_MP_REACH_NLRI = 14,
        ATTR_TYPE_MP_UNREACH_NLRI,
        ATTR_TYPE_EXT_COMMUNITY = 16,
        ATTR_TYPE_AS4_PATH = 17,
        ATTR_TYPE_AS4_AGGREGATOR = 18,

        ATTR_TYPE_AS_PATHLIMIT = 21,              // Deprecated - draft-ietf-idr-as-pathlimit, JunOS will send this

        ATTR_TYPE_IPV6_EXT_COMMUNITY = 25,
        ATTR_TYPE_AIGP,                         ///< RFC7311 - Accumulated IGP metric

        ATTR_TYPE_BGP_LS = 29,                    // BGP LS attribute draft-ietf-idr-ls-distribution

        ATTR_TYPE_BGP_LINK_STATE_OLD = 99,        // BGP link state Older
        ATTR_TYPE_BGP_ATTRIBUTE_SET = 128,

        /*
         * Below attribute types are for internal use only... These are derived/added based on other attributes
         */
                ATTR_TYPE_INTERNAL_AS_COUNT = 9000,        // AS path count - number of AS's
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
        BGP_AFI_L2VPN = 25,
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
        LIB_ATTR_AS_ORIGIN,
        LIB_ATTR_AS_PATH_SIZE,
        LIB_ATTR_NEXT_HOP,
        LIB_ATTR_NEXT_HOP_ISIPV4,
        LIB_ATTR_MED,
        LIB_ATTR_LOCAL_PREF,
        LIB_ATTR_ATOMIC_AGGREGATE,
        LIB_ATTR_AGGREGATOR,
        LIB_ATTR_COMMUNITIES,
        LIB_ATTR_ORIGINATOR_ID,
        LIB_ATTR_CLUSTER_LIST,
        LIB_ATTR_EXT_COMMUNITY,
        LIB_ATTR_IPV6_EXT_COMMUNITY,
        LIB_ATTR_BASE_ATTR_HASH,

        //Linkstate Node attributes
                LIB_ATTR_LS_MT_ID,
        LIB_ATTR_LS_FLAGS,
        LIB_ATTR_LS_NODE_NAME,
        LIB_ATTR_LS_ISIS_AREA_ID,
        LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4,
        LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6,
        LIB_ATTR_LS_LOCAL_ROUTER_ID,
        LIB_ATTR_LS_SR_CAPABILITIES_TLV,

        //Linkstate Link attributes
                LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4,
        LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6,
        LIB_ATTR_LS_REMOTE_ROUTER_ID,
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
            "asOrigin",
            "asPathSize",
            "nextHop",
            "nextHopIsIpv4",
            "med",
            "localPref",
            "atomicAggregate",
            "aggregator",
            "communities",
            "originator",
            "clusterList",
            "extendedCommunities",
            "ipv6ExtendedCommunities",
            "baseAttributeHash",

            //Linkstate Node attributes
            "linkstateMtId",
            "linkstateFlags",
            "linkstateNodeName",
            "linkstateIsisAreaId",
            "linkstateLocalRouterIdIpv4",
            "linkstateLocalRouterIdIpv6",
            "linkstateLocalRouterId",
            "linkstateSrCapabilitiesTlv",

            //Linkstate Link attributes
            "linkstateRemoteRouterIdIpv4",
            "linkstateRemoteRouterIdIpv6",
            "linkstateRemoteRouterId",
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
        LIB_NLRI_HASH,
        LIB_NLRI_IS_IPV4,

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

        LIB_NLRI_LS_LINK_LOCAL_ID,
        LIB_NLRI_LS_LINK_REMOTE_ID,
        LIB_NLRI_LS_INTF_ADDR,
        LIB_NLRI_LS_NEIGHBOR_ADDR,
        LIB_NLRI_LS_MT_ID,

        LIB_NLRI_LS_OSPF_ROUTE_TYPE,
        LIB_NLRI_LS_IP_REACH_PREFIX,
        LIB_NLRI_LS_IP_REACH_PREFIX_LENGTH,
        LIB_NLRI_LS_IP_REACH_PREFIX_BCAST,

        LIB_NLRI_LS_LOCAL_NODE_HASH,
        LIB_NLRI_LS_REMOTE_NODE_HASH,
        LIB_NLRI_LS_LINK_HASH,
        LIB_NLRI_LS_PREFIX_HASH,

        LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD,
        LIB_NLRI_VPN_RD_ASSIGNED_NUMBER,
        LIB_NLRI_VPN_RD_TYPE,
        LIB_NLRI_VPN_RD,

        LIB_NLRI_EVPN_ETHERNET_SEGMENT_ID,
        LIB_NLRI_EVPN_ETHERNET_TAG_ID_HEX,
        LIB_NLRI_EVPN_MAC_LEN,
        LIB_NLRI_EVPN_MAC,
        LIB_NLRI_EVPN_IP_LEN,
        LIB_NLRI_EVPN_IP,
        LIB_NLRI_EVPN_MPLS_LABEL1,
        LIB_NLRI_EVPN_MPLS_LABEL2,
        LIB_NLRI_ORIGINATING_ROUTER_IP_LEN,
        LIB_NLRI_ORIGINATING_ROUTER_IP,

        LIB_NLRI_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_NLRI_MAX> parse_bgp_lib_nlri_names = {
            std::string("prefix"),
            "prefixBinary",
            "prefixLength",
            "pathId",
            "labels",
            "nlriHash",
            "isIpv4",

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

            "linkstateLinkLocalId",
            "linkstateLinkRemoteId",
            "linkstateInterfaceAddr",
            "linkstateNeighborAddr",
            "linkstateMtId",

            "linkstateOspfRouteType",
            "linkstateIpReachPrefix",
            "linkstateIpReachPrefixLength",
            "linkstateIpReachPrefixBcast",

            "linkstateLocalNodeHash",
            "linkstateRemoteNodeHash",
            "linkstateLinkHash",
            "linkstatePrefixHash",

            "vpnRdAdministratorSubfield",
            "vpnRdAssignedNumber",
            "vpnRdType",
            "vpnRd",

            "evpnEthernetSegmentId",
            "evpnEthernetTagIdHex",
            "evpnMacLen",
            "evpnMac",
            "evpnIpLen",
            "evpnIp",
            "evpnMplsLabel1",
            "evpnMplsLabel2",
            "evpnOriginatingRouterIpLen",
            "evpnOriginatingRouterIp"
    };


    enum BGP_LIB_PEER {
        LIB_PEER_HASH_ID,
        LIB_PEER_RD,
        LIB_PEER_ADDR,
        LIB_PEER_BGP_ID,
        LIB_PEER_AS,
        LIB_PEER_ISL3VPN,
        LIB_PEER_ISPREPOLICY,
        LIB_PEER_ISADJIN,
        LIB_PEER_ISIPV4,
        LIB_PEER_TIMESTAMP_SECS,
        LIB_PEER_TIMESTAMP_USECS,
        LIB_PEER_TIMESTAMP,
        LIB_PEER_NAME,

        LIB_PEER_BMP_REASON,
        LIB_PEER_BGP_ERR_CODE,
        LIB_PEER_BGP_ERR_SUBCODE,
        LIB_PEER_ERROR_TEXT,

        LIB_PEER_INFO_DATA,
        LIB_PEER_LOCAL_IP,
        LIB_PEER_LOCAL_PORT,
        LIB_PEER_LOCAL_ASN,
        LIB_PEER_LOCAL_HOLD_TIME,
        LIB_PEER_LOCAL_BGP_ID,
        LIB_PEER_REMOTE_ASN,
        LIB_PEER_REMOTE_PORT,
        LIB_PEER_REMOTE_HOLD_TIME,
        LIB_PEER_REMOTE_BGP_ID,
        LIB_PEER_SENT_CAP,
        LIB_PEER_RECV_CAP,

        LIB_PEER_MAX

    };

    const std::array<std::string, parse_bgp_lib::LIB_PEER_MAX> parse_bgp_lib_peer_names = {
            std::string("peerHashId"),
            "peerRd",
            "peerAddr",
            "peerBgpId",
            "peerAs",
            "peerIsL3vpn",
            "peerIsPrepolicy",
            "peerIsAdjin",
            "peerIsIpv4",
            "peerTimestampSecs",
            "peerTimestampMicrosecs",
            "peerTimestamp",
            "peerName",

            "peerBmpReason",
            "peerBgpErrCode",
            "peerBgpErrSubcode",
            "peerErrorText",
            "peerInfoData",
            "peerLocalIp",
            "peerLocalPort",
            "peerLocalAsn",
            "peerLocalHoldTime",
            "peerLocalBgpId",
            "peerRemoteAsn",
            "peerRemotePort",
            "peerRemoteHoldTime",
            "peerRemoteBgpId",
            "peerSentCap",
            "peerRecvCap"
    };

    enum BGP_LIB_ROUTER {
        LIB_ROUTER_HASH_ID,
        LIB_ROUTER_NAME,
        LIB_ROUTER_DESCR,
        LIB_ROUTER_IP,
        LIB_ROUTER_BGP_ID,
        LIB_ROUTER_ASN,
        LIB_ROUTER_TERM_REASON_CODE,
        LIB_ROUTER_TERM_REASON_TEXT,
        LIB_ROUTER_TERM_DATA,
        LIB_ROUTER_INITIATE_DATA,
        LIB_ROUTER_TIMESTAMP_SECS,
        LIB_ROUTER_TIMESTAMP_USECS,
        LIB_ROUTER_TIMESTAMP,
        LIB_ROUTER_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_ROUTER_MAX> parse_bgp_lib_router_names = {
            std::string("routerHashId"),
            "routerName",
            "routerDescr",
            "routerIp",
            "routerBgpId",
            "routerAsn",
            "routerTermReasonCode",
            "routerTermReasonText",
            "routerTermData",
            "routerInitiateData",
            "routerTimestampSecs",
            "routerTimestampMicrosecs",
            "routerTimestamp"
    };

    enum BGP_LIB_COLLECTOR {
        LIB_COLLECTOR_HASH_ID,
        LIB_COLLECTOR_ADMIN_ID,
        LIB_COLLECTOR_DESCR,
        LIB_COLLECTOR_ROUTERS,
        LIB_COLLECTOR_ROUTER_COUNT,
        LIB_COLLECTOR_TIMESTAMP_SECS,
        LIB_COLLECTOR_TIMESTAMP_USECS,
        LIB_COLLECTOR_TIMESTAMP,
        LIB_COLLECTOR_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_COLLECTOR_MAX> parse_bgp_lib_collector_names = {
            std::string("collectorHashId"),
            "collectorAdminId",
            "collectorDescr",
            "collectorRouters",
            "collectorRouterCount",
            "collectorTimestampSecs",
            "collectorTimestampMicrosecs",
            "collectorTimestamp"
    };

    enum BGP_LIB_HEADER {
        LIB_HEADER_ACTION,
        LIB_HEADER_SEQUENCE_NUMBER,
        LIB_HEADER_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_HEADER_MAX> parse_bgp_lib_header_names = {
            std::string("action"),
            "sequenceNumber"
    };

    enum BGP_LIB_STATS {
        LIB_STATS_PREFIXES_REJ,
        LIB_STATS_KNOWN_DUP_PREFIXES,
        LIB_STATS_KNOWN_DUP_WITHDRAWS,
        LIB_STATS_INVALID_CLUSTER_LIST,
        LIB_STATS_INVALID_AS_PATH_LOOP,
        LIB_STATS_INVALID_ORIGINATOR_ID,
        LIB_STATS_INVALID_AS_CONFED_LOOP,
        LIB_STATS_ROUTES_ADJ_RIB_IN,
        LIB_STATS_ROUTES_LOC_RIB,
        LIB_STATS_MAX
    };

    const std::array<std::string, parse_bgp_lib::LIB_STATS_MAX> parse_bgp_lib_stats_names = {
            std::string("statsPrefixesRej"),
            "statsKnownDupPrefixes",
            "statsKnownDupWithdraws",
            "statsInvalidClusterList",
            "statsInvalidAsPathLoop",
            "statsInvalidOriginatorId",
            "statsInvalidAsConfedLoop",
            "statsRoutesAdjIn",
            "statsRoutesLocRib"
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
    inline std::string parse_mac(u_char *data_pointer) {
        u_char *pointer = data_pointer;

        std::ostringstream mac_stringstream;

        for (int i = 0; i < 6; ++i) {
            if (i != 0) mac_stringstream << ':';
            mac_stringstream.width(2);
            mac_stringstream.fill('0');
            mac_stringstream << std::hex << (int) (pointer[i]);
        }

        return mac_stringstream.str();
    }


    /**
     * \brief       binary hash to printed string format
     *
     * \details     Converts a hash unsigned char bytes to HEX string for
     *              printing or storing in the DB.
     *
     * \param[in]   hash_bin      16 byte binary/unsigned value
     */

    static std::string hash_toStr(const u_char *hash_bin) {

        int i;
        char s[33];

        for (i = 0; i < 16; i++)
            sprintf(s + i * 2, "%02x", hash_bin[i]);

        s[32] = '\0';

        return string(s);
    }

    /*********************************************************************//**
     * Simple function to update the hash. This function will take a list of
     * strings, concat them and update the hash
     *
     * @param [in] var   Variable containing data to hash
     * @param [in/out]   hash  The hash to update
     *********************************************************************/
    static void update_hash(std::list<std::string> *value, MD5 *hash) {
        string hash_string;
        std::list<std::string>::iterator last_value = value->end();
        last_value--;

        for (std::list<std::string>::iterator it = value->begin(); it != value->end(); it++) {
            hash_string += *it;
            if (it != last_value) {
                hash_string += std::string(" ");
            }
        }
        hash->update((unsigned char *) hash_string.c_str(), hash_string.length());
    }


    /****************************************************************//**
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
 * \brief       Time in seconds to printed string format
 *
 * \param[in]   time_secs     Time since epoch in seconds
 * \param[in]   time_us       Microseconds to add to timestamp
 * \param[out]  ts_str        Reference to storage of string value
 *
 */
    static void getTimestamp(uint32_t time_secs, uint32_t time_us, std::string &ts_str){
        char buf[48];
        timeval tv;
        std::time_t secs;
        uint32_t us;
        tm *p_tm;

        if (time_secs <= 1000) {
            gettimeofday(&tv, NULL);
            secs = tv.tv_sec;
            us = tv.tv_usec;

        } else {
            secs = time_secs;
            us = time_us;
        }

        p_tm = std::gmtime(&secs);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", p_tm);
        ts_str = buf;

        sprintf(buf, ".%06u", us);
        ts_str.append(buf);
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
        struct parse_bgp_lib_peer_hdr {
            unsigned char peer_type;           ///< 1 byte
            unsigned char peer_flags;          ///< 1 byte

            unsigned char peer_dist_id[8];     ///< 8 byte peer route distinguisher
            unsigned char peer_addr[16];       ///< 16 bytes
            unsigned char peer_as[4];          ///< 4 byte
            unsigned char peer_bgp_id[4];      ///< 4 byte peer bgp id
            uint32_t      ts_secs;             ///< 4 byte timestamp in seconds
            uint32_t      ts_usecs;            ///< 4 byte timestamp microseconds

        } __attribute__ ((__packed__));

        /**
         * BMP Init message
         */
        struct parse_bgp_lib_bmp_msg_v3 {
            uint16_t        type;              ///< 2 bytes - Information type
            uint16_t        len;               ///< 2 bytes - Length of the information that follows

            char           *info;              ///< Information - variable

        } __attribute__ ((__packed__));


        /*********************************************************************//**
     * Constructors for class
     ***********************************************************************/
        parseBgpLib(Logger *logPtr, bool enable_debug, BMPReader::peer_info *p_info);

        parseBgpLib(Logger *logPtr, bool enable_debug);

        virtual ~parseBgpLib();

        /*
         * Internal structure consisting of
         * iana or standard codepoint, included well known BGP attribute types and other types defined in an ietf draft
         * eg., BGP-LS TLV types carried over from IGP
         */
        struct parse_bgp_lib_data {
            uint32_t official_type;
            std::string name;
            std::list<std::string> value;
        };

        typedef std::map<parse_bgp_lib::BGP_LIB_ATTRS, parse_bgp_lib_data> attr_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_NLRI, parse_bgp_lib_data> nlri_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_PEER, parse_bgp_lib_data> peer_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_ROUTER, parse_bgp_lib_data> router_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_COLLECTOR, parse_bgp_lib_data> collector_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_HEADER, parse_bgp_lib_data> header_map;
        typedef std::map<parse_bgp_lib::BGP_LIB_STATS, parse_bgp_lib_data> stat_map;


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
            peer_map peer;
            router_map router;
        };


        /**
         * Parses the BMP router init message
         *
         * \details
         * Parse BMP Router Init message
         * \param [in]  bmp_data        Buffer containing the data
         * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
         *
         */
        void parseBmpInitMsg(int sock, u_char *bmp_data, size_t bmp_data_len, parsed_update &update);

        /**
          * Parses the BMP router Term message
          *
          * \details
          * Parse BMP Router Init message
          * \param [in]  bmp_data        Buffer containing the data
          * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
          *
          */
        void parseBmpTermMsg(int sock, u_char *bmp_data, size_t bmp_data_len, parsed_update &update);


        /**
         * Parses the BMP peer header message
         *
         * \details
         * Parse BMP Peer header
         * \param [in]   peer_hdr       Struct peer_hdr
         * \param [in]  parsed_update   Reference to parsed_update; will be updated with all parsed data
         *
         */
        void parseBmpPeer(int sock, parse_bgp_lib_peer_hdr &peer_hdr, parsed_update &update);

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
         * Set the peer Info
         *
         * \details
         * Set the peer info used late to parse the update message and for logging
         * \param [in]   p_info        peer info
         *
         */
        void setPeerInfo(BMPReader::peer_info *peer_info);


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

        BMPReader::peer_info *p_info;   ///< Persistent Peer information
        string debug_prepend_string; ///< debug print string added to all log messages

    private:
        //TODO: Remove
        bool debug;                             ///< debug flag to indicate debugging
        Logger *logger;                         ///< Logging class pointer
        bool four_octet_asn; ///< Indicates true if 4 octets or false if 2
        char asn_octet_size;

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

        /**
         * BMP Initiation Message Types
         */
        enum BMP_INIT_TYPES { INIT_TYPE_FREE_FORM_STRING=0, INIT_TYPE_SYSDESCR, INIT_TYPE_SYSNAME,
            INIT_TYPE_ROUTER_BGP_ID=65531 };

        /**
         * BMP Termination Message Types
         */
        enum BMP_TERM_TYPES { TERM_TYPE_FREE_FORM_STRING=0, TERM_TYPE_REASON };

        /**
         * BMP Termination Message reasons for type=1
         */
        enum BMP_TERM_TYPE1_REASON { TERM_REASON_ADMIN_CLOSE=0, TERM_REASON_UNSPECIFIED, TERM_REASON_OUT_OF_RESOURCES,
            TERM_REASON_REDUNDANT_CONN,
            TERM_REASON_OPENBMP_CONN_CLOSED=65533, TERM_REASON_OPENBMP_CONN_ERR=65534 };




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
        void parseAttrData(u_char attr_type, uint16_t attr_len, u_char *data, parsed_update &update, MD5 &hash);

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
}/* namespace parse_bgp_lib */

#endif /* PARSE_BGP_LIB_H_ */
