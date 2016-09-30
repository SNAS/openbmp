/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef MSGBUSINTERFACE_HPP_
#define MSGBUSINTERFACE_HPP_

#include <sys/types.h>
#include <stdint.h>
#include <vector>
#include <list>
#include <string>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

/**
 * \class   MsgBusInterface
 *
 * \brief   Abstract class for the message bus interface.
 * \details Internal memory schema and all expected methods are
 *          defined here.
 *
 *          It is required that the implementing class follow the
 *          internal schema to store the data as needed.
 */
class MsgBusInterface {
public:

    /* ---------------------------------------------------------------------------
     * Msg data schema
     * ---------------------------------------------------------------------------
     */


    /**
     * OBJECT: collector
     *
     * Router table schema
     */
    struct obj_collector {
        u_char      hash_id[16];            ///< Hash ID for collector (is the unique ID)
        char        admin_id[64];           ///< Admin ID for collector
        u_char      descr[255];             ///< Description of collector

        char        routers[4096];          ///< List of connected routers hash ID's delimited by PIPE
        uint32_t    router_count;           ///< Count of active/connected routers
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
        uint32_t    timestamp_us;           ///< Timestamp microseconds
    };

    /// Collector action codes
    enum collector_action_code {
        COLLECTOR_ACTION_STARTED =0,
        COLLECTOR_ACTION_CHANGE,
        COLLECTOR_ACTION_HEARTBEAT,
        COLLECTOR_ACTION_STOPPED,
    };

    /**
     * OBJECT: routers
     *
     * Router table schema
     */
    struct obj_router {
        u_char      hash_id[16];            ///< Router hash ID of name and src_addr
        u_char      name[255];              ///< BMP router sysName (initiation Type=2)
        u_char      descr[255];             ///< BMP router sysDescr (initiation Type=1)
        u_char      ip_addr[46];            ///< BMP router source IP address in printed form
        char        bgp_id[16];             ///< BMP Router bgp-id
        uint32_t    asn;                    ///< BMP router ASN
        uint16_t    term_reason_code;       ///< BMP termination reason code
        char        term_reason_text[255];  ///< BMP termination reason text decode string

        char        term_data[4096];        ///< Type=0 String termination info data
        char        initiate_data[4096];    ///< Type=0 String initiation info data

        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
        uint32_t    timestamp_us;           ///< Timestamp microseconds
    };

    /// Router action codes
    enum router_action_code {
        ROUTER_ACTION_FIRST=0,
        ROUTER_ACTION_INIT,
        ROUTER_ACTION_TERM

    };

    /**
     * OBJECT: bgp_peers
     *
     * BGP peer table schema
     */
    struct obj_bgp_peer {
        u_char      hash_id[16];            ///< hash of router hash_id, peer_rd, peer_addr, and peer_bgp_id
        u_char      router_hash_id[16];     ///< Router hash ID

        char        peer_rd[32];            ///< Peer distinguisher ID (string/printed format)
        char        peer_addr[46];          ///< Peer IP address in printed form
        char        peer_bgp_id[16];        ///< Peer BGP ID in printed form
        uint32_t    peer_as;                ///< Peer ASN
        bool        isL3VPN;                ///< true if peer is L3VPN, otherwise it is Global
        bool        isPrePolicy;            ///< True if the routes are pre-policy, false if not
        bool        isAdjIn;                ///< True if the routes are Adj-Rib-In, false if not
        bool        isIPv4;                 ///< true if peer is IPv4 or false if IPv6
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
        uint32_t    timestamp_us;           ///< Timestamp microseconds
    };

    /**
     * OBJECT: peer_down_events
     *
     * Peer Down Events schema
     */
    struct obj_peer_down_event {
        u_char          bmp_reason;         ///< BMP notify reason
        u_char          bgp_err_code;       ///< BGP notify error code
        u_char          bgp_err_subcode;    ///< BGP notify error sub code
        char            error_text[255];    ///< BGP error text string
    };

    /**
     * OBJECT: peer_up_events
     *
     * Peer Up Events schema
     *
     * \note    open_params are the decoded values in string/text format; e.g. "attr=value ..."
     *          Numeric values are converted to printed form.   The buffer itself is
     *          allocated by the caller and freed by the caller.
     */
    struct obj_peer_up_event {
        char        info_data[4096];        ///< Inforamtional data for peer
        char        local_ip[40];           ///< IPv4 or IPv6 printed IP address
        uint16_t    local_port;             ///< Local port number
        uint32_t    local_asn;              ///< Local ASN for peer
        uint16_t    local_hold_time;        ///< BGP hold time
        char        local_bgp_id[16];       ///< Local BGP ID in printed form
        uint32_t    remote_asn;             ///< Remote ASN for peer
        uint16_t    remote_port;            ///< Remote port number
        uint16_t    remote_hold_time;       ///< BGP hold time
        char        remote_bgp_id[16];      ///< Remote Peer BGP ID in printed form

        char        sent_cap[4096];         ///< Received Open param capabilities
        char        recv_cap[4096];         ///< Received Open param capabilities
    };


    /// Peer action codes
    enum peer_action_code {
        PEER_ACTION_FIRST=0,
        PEER_ACTION_UP,
        PEER_ACTION_DOWN
    };

    /**
     * OBJECT: path_attrs
     *
     * Prefix Path attributes table schema
     */
    struct obj_path_attr {

        /**
         * Path hash
         */
        u_char      hash_id[16];
        char        origin[16];             ///< bgp origin as string name

        /**
         * as_path.
         */
        std::string as_path;

        uint16_t    as_path_count;          ///< Count of AS PATH's in the path (includes all in AS-SET)

        uint32_t    origin_as;              ///< Origin ASN
        bool        nexthop_isIPv4;         ///< True if IPv4, false if IPv6
        char        next_hop[40];           ///< Next-hop IP in printed form
        char        aggregator[40];         ///< Aggregator IP in printed form
        bool        atomic_agg;             ///< 0=false, 1=true for atomic_aggregate

        uint32_t    med;                    ///< bgp MED
        uint32_t    local_pref;             ///< bgp local pref

        /**
         * standard community list.
         */
        std::string community_list;

        /**
         * extended community list.
         */
        std::string  ext_community_list;

        /**
         * cluster list.
         */
        std::string cluster_list;

        char        originator_id[16];      ///< Originator ID in printed form
    };

    /// Base attribute action codes
    enum base_attr_action_code {
        BASE_ATTR_ACTION_ADD=0
    };

    /**
     * OBJECT: rib
     *
     * Prefix rib table schema
     */
    struct obj_rib {
        u_char      hash_id[16];            ///< hash of attr hash prefix, and prefix len
        u_char      path_attr_hash_id[16];  ///< path attrs hash_id
        u_char      peer_hash_id[16];       ///< BGP peer hash ID, need it here for withdraw routes support
        u_char      isIPv4;                 ///< 0 if IPv6, 1 if IPv4
        char        prefix[46];             ///< IPv4/IPv6 prefix in printed form
        u_char      prefix_len;             ///< Length of prefix in bits
        uint8_t     prefix_bin[16];         ///< Prefix in binary form
        uint8_t     prefix_bcast_bin[16];   ///< Broadcast address/last address in binary form
        uint32_t    path_id;                ///< Add path ID - zero if not used
        char        labels[255];            ///< Labels delimited by comma
    };

    /// Unicast prefix action codes
    enum unicast_prefix_action_code {
        UNICAST_PREFIX_ACTION_ADD=0,
        UNICAST_PREFIX_ACTION_DEL,
    };

    /**
     * OBJECT: stats_reports
     *
     * Stats Report schema
     */
    struct obj_stats_report {
        uint32_t        prefixes_rej;           ///< type=0 Prefixes rejected
        uint32_t        known_dup_prefixes;     ///< type=1 known duplicate prefixes
        uint32_t        known_dup_withdraws;    ///< type=2 known duplicate withdraws
        uint32_t        invalid_cluster_list;   ///< type=3 Updates invalid by cluster lists
        uint32_t        invalid_as_path_loop;   ///< type=4 Updates invalid by as_path loop
        uint32_t        invalid_originator_id;  ///< type=5 Invalid due to originator_id
        uint32_t        invalid_as_confed_loop; ///< type=6 Invalid due to as_confed loop
        uint64_t        routes_adj_rib_in;      ///< type=7 Number of routes in adj-rib-in
        uint64_t        routes_loc_rib;         ///< type=8 number of routes in loc-rib
    };

    /**
     * OBJECT: ls_node
     *
     * BGP-LS Node table schema
     */
    struct obj_ls_node {
        u_char      hash_id[16];                ///< hash id for the entry
        uint64_t    id;                         ///< Routing universe identifier
        bool        isIPv4;                     ///< True if interface/neighbor is IPv4, false otherwise
        uint32_t    asn;                        ///< BGP ASN
        uint32_t    bgp_ls_id;                  ///< BGP-LS Identifier
        uint8_t     igp_router_id[8];           ///< IGP router ID
        uint8_t     ospf_area_Id[4];            ///< OSPF area ID
        char        protocol[32];               ///< String representation of the protocol name
        uint8_t     router_id[16];              ///< IPv4 or IPv6 router ID
        uint8_t     isis_area_id[9];            ///< IS-IS area ID
        char        flags[32];                  ///< String representation of the flag bits
        char        name[255];                  ///< Name of router
        char        mt_id[255];                 ///< Multi-Topology ID
        char        sr_capabilities_tlv[255];   ///< SR Capabilities TLV
    };

    /// LS action code (node, link, and prefix)
    enum ls_action_code {
        LS_ACTION_ADD=0,
        LS_ACTION_DEL
    };

    /**
     * OBJECT: ls_link
     *
     * BGP-LS Link table schema
     */
    struct obj_ls_link {
        u_char      hash_id[16];                ///< hash id for the entry
        uint64_t    id;                         ///< Routing universe identifier
        uint32_t    mt_id;                      ///< Multi-Topology ID

        uint32_t    bgp_ls_id;                  ///< BGP-LS Identifier
        uint8_t     igp_router_id[8];           ///< IGP router ID (local)
        uint8_t     remote_igp_router_id[8];    ///< IGP router ID (remote)
        uint8_t     ospf_area_Id[4];            ///< OSPF area ID
        uint8_t     router_id[16];              ///< IPv4 or IPv6 router ID (local)
        uint8_t     remote_router_id[16];       ///< IPv4 or IPv6 router ID (remote)

        uint32_t    local_node_asn;             ///< Local node asn
        uint32_t    remote_node_asn;            ///< Remote node asn
        uint32_t    local_bgp_router_id;        ///< Local BGP router id (draft-ietf-idr-bgpls-segment-routing-epe)
        uint32_t    remote_bgp_router_id;       ///< Remote BGP router id (draft-ietf-idr-bgpls-segment-routing-epe)

        uint8_t     isis_area_id[9];            ///< IS-IS area ID

        char        protocol[32];               ///< String representation of the protocol name
        uint8_t     intf_addr[16];              ///< Interface binary address
        uint8_t     nei_addr[16];               ///< Neighbor binary address
        uint32_t    local_link_id;              ///< Local Link ID (IS-IS)
        uint32_t    remote_link_id;             ///< Remote Link ID (IS-IS)
        bool        isIPv4;                     ///< True if interface/neighbor is IPv4, false otherwise
        u_char      local_node_hash_id[16];     ///< Local node hash ID
        u_char      remote_node_hash_id[16];    ///< Remove node hash ID
        uint32_t    admin_group;                ///< Admin group
        uint32_t    max_link_bw;                ///< Maximum link bandwidth
        uint32_t    max_resv_bw;                ///< Maximum reserved bandwidth
        char        unreserved_bw[100];         ///< string for unreserved bandwidth, a set of 8 uint32_t values

        uint32_t    te_def_metric;              ///< Default TE metric
        char        protection_type[60];        ///< String representation for the protection types
        char        mpls_proto_mask[32];        ///< Either LDP or RSVP-TE
        uint32_t    igp_metric;                 ///< IGP metric
        char        srlg[128];                  ///< String representation of the shared risk link group values
        char        name[255];                  ///< Name of router
        char        peer_node_sid[128];         ///< Peer node side (draft-ietf-idr-bgpls-segment-routing-epe)
        char        peer_adj_sid[128];          ///< Peer Adjency Segment Identifier
    };

    /**
     * OBJECT: ls_prefix
     *
     * BGP-LS Prefix table schema
     */
    struct obj_ls_prefix {
        u_char      hash_id[16];            ///< hash for the entry
        uint64_t    id;                     ///< Routing universe identifier
        char        protocol[32];           ///< String representation of the protocol name

        uint32_t    bgp_ls_id;              ///< BGP-LS Identifier
        uint8_t     igp_router_id[8];       ///< IGP router ID
        uint8_t     ospf_area_Id[4];        ///< OSPF area ID
        uint8_t     router_id[16];          ///< IPv4 or IPv6 router ID
        uint8_t     isis_area_id[9];        ///< IS-IS area ID
        uint8_t     intf_addr[16];          ///< Interface binary address
        uint8_t     nei_addr[16];           ///< Neighbor binary address

        u_char      local_node_hash_id[16]; ///< Local node hash ID
        uint32_t    mt_id;                  ///< Multi-Topology ID
        uint32_t    metric;                 ///< Prefix metric
        bool        isIPv4;                 ///< True if interface/neighbor is IPv4, false otherwise
        u_char      prefix_len;             ///< Length of prefix in bits
        char        ospf_route_type[32];    ///< String representation of the OSPF route type
        uint8_t     prefix_bin[16];         ///< Prefix in binary form
        uint8_t     prefix_bcast_bin[16];   ///< Broadcast address/last address in binary form
        char        igp_flags[32];          ///< String representation of the IGP flags
        uint32_t    route_tag;              ///< Route tag
        uint64_t    ext_route_tag;          ///< Extended route tag
        uint8_t     ospf_fwd_addr[16];      ///< IPv4/IPv6 OSPF forwarding address
    };

    /* ---------------------------------------------------------------------------
     * Abstract methods
     * ---------------------------------------------------------------------------
     */
    virtual ~MsgBusInterface() { };

    /*****************************************************************//**
     * \brief       Add/Update/del a collector object
     *
     * \details     Will generate a message for a new/updated collector.
     *
     * \param[in,out]   collector       Collector object
     *
     * \returns     collector.hash_id will be updated based on the supplied data.
     */
    virtual void update_Collector(struct obj_collector &c_obj, collector_action_code action_code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update a router object
     *
     * \details     Will generate a message to add a new router or update an existing
     *              router.
     *
     * \param[in,out]   router          Router object
     * \param[in]       code            Action code for router update
     *
     * \returns     The router.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_Router(struct obj_router &r_object, router_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update a BGP peer object
     *
     * \details     Will generate a message to add a new BGP peer
     *
     * \param[in,out] peer            BGP peer object
     * \param[in]     peer_up_event   Peer up event struct (null if not used)
     * \param[in]     peer_down_event Peer down event struct (null if not used)
     * \param[in]     code            Action code for router update
     *
     * \returns     The peer.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     ****************************************************************/
    virtual void update_Peer(obj_bgp_peer &peer, obj_peer_up_event *up, obj_peer_down_event *down, peer_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update base path attributes
     *
     * \details     Will generate a message to add a new path object.
     *
     * \param[in]       peer      Peer object
     * \param[in,out]   attr      Path attribute object
     * \param[in]       code      Base attribute action code
     *
     * \returns     The path.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_baseAttribute(obj_bgp_peer &peer, obj_path_attr &attr, base_attr_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update RIB objects
     *
     * \details     Will generate a message to add new RIB prefixes
     *
     * \param[in]       peer    Peer object
     * \param[in,out]   rib     List of one or more RIB entries
     * \param[in]       attr    Path attribute object (can be null if n/a)
     * \param[in]       code    unicast prefix action code
     *
     * \returns     The rib.hash_id will be updated based on the
     *              supplied data for each object.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_unicastPrefix(obj_bgp_peer &peer, std::vector<obj_rib> &rib, obj_path_attr *attr,
                                      unicast_prefix_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add a stats report object
     *
     * \details     Will generate a message to add a new stats report object.
     *
     * \param[in,out]   stats      Stats report object
     *****************************************************************/
    virtual void add_StatReport(obj_bgp_peer &peer, obj_stats_report &stats) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS nodes
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   attr       Path attribute object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_LsNode(obj_bgp_peer &peer, obj_path_attr &attr,
                                std::list<MsgBusInterface::obj_ls_node> &nodes,
                                ls_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS links
     *
     * \details     Will generate a message to add/update BGP-LS links.
     *
     * \param[in]   peer       Peer object
     * \param[in]   attr       Path attribute object
     * \param[in]   links      List of one or more link tables
     * \param[in]   code       Linkstate action code
     *
     * \returns     The hash_id will be updated based on the
     *              supplied data for each object.
     *****************************************************************/
    virtual void update_LsLink(obj_bgp_peer &peer, obj_path_attr &attr,
                             std::list<MsgBusInterface::obj_ls_link> &links,
                             ls_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS prefixes
     *
     * \details     Will generate a message to add/update BGP-LS prefixes.
     *
     * \param[in]      peer       Peer object
     * \param[in]      attr       Path attribute object
     * \param[in/out]  prefixes   List of one or more node tables
     * \param[in]      code       Linkstate action code
     *
     * \returns     The hash_id will be updated based on the
     *              supplied data for each object.
     *****************************************************************/
    virtual void update_LsPrefix(obj_bgp_peer &peer, obj_path_attr &attr,
                                std::list<MsgBusInterface::obj_ls_prefix> &prefixes,
                                ls_action_code code) = 0;

    /*****************************************************************//**
     * \brief       Send BMP packet
     *
     * \details     Will generate a message to send the BMP packet data/feed
     *
     * \param[in]    r_hash     Router hash
     * \param[in]    peer       Peer object
     * \param[in]    data       Packet raw data
     * \param[in]    data_len   Length in bytes for the raw data
     *
     * \returns     The hash_id will be updated based on the
     *              supplied data for each object.
     *****************************************************************/
    virtual void send_bmp_raw(u_char *r_hash, obj_bgp_peer &peer, u_char *data, size_t data_len) = 0;


    /* ---------------------------------------------------------------------------
     * Commonly used methods
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief       binary hash to printed string format
     *
     * \details     Converts a hash unsigned char bytes to HEX string for
     *              printing or storing in the DB.
     *
     * \param[in]   hash_bin      16 byte binary/unsigned value
     * \param[out]  hash_str      Reference to storage of string value
     *
     */
    static void hash_toStr(const u_char *hash_bin, std::string &hash_str){

        int i;
        char s[33];

        for (i=0; i<16; i++)
            sprintf(s+i*2, "%02x", hash_bin[i]);

        s[32]='\0';

        hash_str = s;
    }

    /**
     * \brief       Time in seconds to printed string format
     *
     * \param[in]   time_secs     Time since epoch in seconds
     * \param[in]   time_us       Microseconds to add to timestamp
     * \param[out]  ts_str        Reference to storage of string value
     *
     */
    void getTimestamp(uint32_t time_secs, uint32_t time_us, std::string &ts_str){
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

protected:

private:

};

#endif
