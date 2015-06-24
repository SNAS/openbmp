/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef DBINTERFACE_HPP_
#define DBINTERFACE_HPP_

#include <sys/types.h>
#include <stdint.h>
#include <vector>
#include <list>
#include <string>
#include <cstdio>

/**
 * \class   DbInterface
 *
 * \brief   Abstract class for the database interface.
 * \details Internal memory schema and all expected methods are 
 *          defined here. 
 *      
 *          It is required that the implementing class follow the
 *          internal schema to store the data as needed.   The backend
 *          storage can be any database (SQL, NoSQL, object store, flat file, ...).
 */
class DbInterface {
public:

    /* ---------------------------------------------------------------------------
     * Database table schema
     * ---------------------------------------------------------------------------
     */

    /**
     * TABLE: stats_reports
     *
     * Stats Report schema
     */
    #define TBL_NAME_STATS_REPORT "stat_reports"
    struct tbl_stats_report {
        u_char          peer_hash_id[16];       ///< Hash ID of the bgp peer from bgp_peers table
        uint32_t        prefixes_rej;           ///< type=0 Prefixes rejected
        uint32_t        known_dup_prefixes;     ///< type=1 known duplicate prefixes
        uint32_t        known_dup_withdraws;    ///< type=2 known duplicate withdraws
        uint32_t        invalid_cluster_list;   ///< type=3 Updates invalid by cluster lists
        uint32_t        invalid_as_path_loop;   ///< type=4 Updates invalid by as_path loop
        uint32_t        invalid_originator_id;  ///< type=5 Invalid due to originator_id
        uint32_t        invalid_as_confed_loop; ///< type=6 Invalid due to as_confed loop
        uint64_t        routes_adj_rib_in;      ///< type=7 Number of routes in adj-rib-in
        uint64_t        routes_loc_rib;         ///< type=8 number of routes in loc-rib
        uint32_t        timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: peer_down_events
     *
     * Peer Down Events schema
     */
    #define TBL_NAME_PEER_DOWN "peer_down_events"
    struct tbl_peer_down_event {
        u_char          peer_hash_id[16];   ///< Hash ID of the bgp peer from bgp_peers table
        u_char          bmp_reason;         ///< BMP notify reason
        u_char          bgp_err_code;       ///< BGP notify error code
        u_char          bgp_err_subcode;    ///< BGP notify error sub code
        char            error_text[255];    ///< BGP error text string
        uint32_t        timestamp_secs;     ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: peer_up_events
     *
     * Peer Up Events schema
     *
     * \note    open_params are the decoded values in string/text format; e.g. "attr=value ..."
     *          Numeric values are converted to printed form.   The buffer itself is
     *          allocated by the caller and freed by the caller.
     */
    #define TBL_NAME_PEER_UP "peer_up_events"
    struct tbl_peer_up_event {
        u_char      peer_hash_id[16];       ///< Hash ID of the bgp peer from bgp_peers table
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

        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: routers
     *
     * Router table schema
     */
    #define TBL_NAME_ROUTERS "routers"
    struct tbl_router {
        u_char      hash_id[16];            ///< Router hash ID of name and src_addr
        u_char      name[255];              ///< BMP router sysName (initiation Type=2)
        u_char      descr[255];             ///< BMP router sysDescr (initiation Type=1)
        u_char      src_addr[40];           ///< BMP router source IP address in printed form
        uint32_t    asn;                    ///< BMP router ASN
        bool        isConnected;            ///< BMP router state as connected or not
        bool        isPassive;              ///< Default is passive (false), active is True
                                            ///<    Active means connection is initiated to BMP device
        uint16_t    term_reason_code;       ///< BMP termination reason code
        char        term_reason_text[255];  ///< BMP termination reason text decode string

        char        term_data[4096];        ///< Type=0 String termination info data
        char        initiate_data[4096];    ///< Type=0 String initiation info data

        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * Peer States for peer table
     */
    enum PEER_STATE {
        PEER_DOWN=0,
        PEER_UP=1,
        PEER_RECEIVING
    };

    /**
     * TABLE: bgp_peers
     *
     * BGP peer table schema
     */
    #define TBL_NAME_BGP_PEERS "bgp_peers"
    struct tbl_bgp_peer {
        u_char      hash_id[16];            ///< hash of router hash_id, peer_rd, peer_addr, and peer_bgp_id
        u_char      router_hash_id[16];     ///< Router hash ID
        //u_char      name[255];              ///< BGP peer name

        char        peer_rd[32];            ///< Peer distinguisher ID (string/printed format)
        char        peer_addr[40];          ///< Peer IP address in printed form
        char        peer_bgp_id[16];        ///< Peer BGP ID in printed form
        uint32_t    peer_as;                ///< Peer ASN
        bool        isL3VPN;                ///< true if peer is L3VPN, otherwise it is Global
        bool        isPrePolicy;            ///< True if the routes are pre-policy, false if not
        bool        isIPv4;                 ///< true if peer is IPv4 or false if IPv6
        PEER_STATE  peer_state;             ///< Peer state
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: as_path_analysis
     *      AS Path analysis schema
     */
    #define TBL_NAME_AS_PATH_ANALYSIS "as_path_analysis"
    struct tbl_as_path_analysis {
        uint32_t    asn;                    ///< ASN being analyzed
        uint32_t    asn_left;               ///< ASN to the left of the ASN in the AS_PATH
        uint32_t    asn_right;              ///< ASN to the right of the ASN in the AS_PATH
        u_char      path_hash_id[16];       ///< Path attribute hash ID the contains the AS_PATH being analyzed
        u_char      peer_hash_id[16];       ///< Peer hash ID the contains the AS_PATH being analyzed

    };

    /**
     * TABLE: path_attrs
     *
     * Prefix Path attributes table schema
     */
    #define TBL_NAME_PATH_ATTRS "path_attrs"
    struct tbl_path_attr {

        /**
         * Path hash
         *
         * \par (Hash Items)
         *  peer_hash_id, as_path, as4_path, next_hop_v4,next_hop_v6, aggregtor_v4, aggreator_v6,
         *  origin, med, local_pref
         */
        u_char      hash_id[16];

        u_char      peer_hash_id[16];       ///< Peer table hash_id
        char        origin[16];             ///< bgp origin as string name

        /**
         * Caller allocated string as_path.
         */
        char        *as_path;
        size_t      as_path_sz;             ///< Size of the as_path in bytes (buffer size)

        uint16_t    as_path_count;          ///< Count of AS PATH's in the path (includes all in AS-SET)

        uint32_t    origin_as;              ///< Origin ASN
        bool        nexthop_isIPv4;         ///< True if IPv4, false if IPv6
        char        next_hop[40];           ///< Next-hop IP in printed form
        char        aggregator[40];         ///< Aggregator IP in printed form
        bool        atomic_agg;             ///< 0=false, 1=true for atomic_aggregate

        uint32_t    med;                    ///< bgp MED
        uint32_t    local_pref;             ///< bgp local pref

        /**
         * Caller allocated string standard community list.
         */
        char        *community_list;
        size_t      community_list_sz;       ///< Size of the community list in bytes (buffer size)

        /**
         * Caller allocated string extended community list.
         */
        char        *ext_community_list;
        size_t      ext_community_list_sz;   ///< Size of the community list in bytes (buffer size)

        /**
         * Caller allocated string cluster list.
         */
        char        *cluster_list;
        size_t      cluster_list_sz;        ///< Size of the cluster list in bytes (buffer size)


        char        originator_id[16];      ///< Originator ID in printed form
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC

    };

    /**
     * TABLE: rib
     *
     * Prefix rib table schema
     */
    #define TBL_NAME_RIB "rib"
    struct tbl_rib {
        u_char      hash_id[16];            ///< hash of attr hash prefix, and prefix len
        u_char      path_attr_hash_id[16];  ///< path attrs hash_id
        u_char      peer_hash_id[16];       ///< BGP peer hash ID, need it here for withdraw routes support
        u_char      isIPv4;                 ///< 0 if IPv6, 1 if IPv4
        uint32_t    origin_as;              ///< Origin ASN
        char        prefix[40];             ///< IPv4/IPv6 prefix in printed form
        u_char      prefix_len;             ///< Length of prefix in bits
        uint8_t     prefix_bin[16];         ///< Prefix in binary form
        uint8_t     prefix_bcast_bin[16];   ///< Broadcast address/last address in binary form
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: path_attr_log
     *
     * Path attribute history log table schema
     */
    #define TBL_NAME_PATH_ATTR_LOG "path_attr_log"
    struct tbl_path_attr_log {
        u_char      rib_hash_id[16];        ///< rib table hash ID
        u_char      path_attr_hash_id[16];  ///< Path attribute hash ID
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: withdrawn_log
     *
     * Prefix withdrawn log history log table schema
     */
    #define TBL_NAME_WITHDRAWN_LOG "withdrawn_log"
    struct tbl_withdrawn_log {
        uint64_t    id;                     ///< ID of the withdraw log - DB auto increment
        u_char      peer_hash_id[16];       ///< bgp peer table hash ID
        char        prefix[40];             ///< IPv4/IPv6 prefix in printed form
        u_char      prefix_len;             ///< Length of prefix in bits
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: ls_node
     *
     * BGP-LS Node table schema
     */
    #define TBL_NAME_LS_NODE "ls_nodes"
    struct tbl_ls_node {
        u_char      hash_id[16];            ///< hash id for the entry
        u_char      path_atrr_hash_id[16];  ///< Path attribute hash ID
        u_char      peer_hash_id[16];       ///< Peer table hash_id
        uint64_t    id;                     ///< Routing universe identifier
        bool        isIPv4;                 ///< True if interface/neighbor is IPv4, false otherwise
        uint32_t    asn;                    ///< BGP ASN
        uint32_t    bgp_ls_id;              ///< BGP-LS Identifier
        uint8_t     igp_router_id[8];       ///< IGP router ID
        uint8_t     ospf_area_Id[4];        ///< OSPF area ID
        char        protocol[32];           ///< String representation of the protocol name
        uint8_t     router_id[16];          ///< IPv4 or IPv6 router ID
        uint8_t     isis_area_id[8];        ///< IS-IS area ID
        char        flags[32];              ///< String representation of the flag bits
        char        name[255];              ///< Name of router
        uint32_t    mt_id;                  ///< Multi-Topology ID
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: ls_link
     *
     * BGP-LS Link table schema
     */
    #define TBL_NAME_LS_LINK "ls_links"
    struct tbl_ls_link {
        u_char      hash_id[16];            ///< hash id for the entry
        u_char      path_atrr_hash_id[16];  ///< Path attribute hash ID
        u_char      peer_hash_id[16];       ///< Peer table hash_id
        uint64_t    id;                     ///< Routing universe identifier
        uint32_t    mt_id;                  ///< Multi-Topology ID
        char        protocol[32];           ///< String representation of the protocol name
        uint8_t     intf_addr[16];          ///< Interface binary address
        uint8_t     nei_addr[16];           ///< Neighbor binary address
        uint32_t    local_link_id;          ///< Local Link ID (IS-IS)
        uint32_t    remote_link_id;         ///< Remote Link ID (IS-IS)
        bool        isIPv4;                 ///< True if interface/neighbor is IPv4, false otherwise
        u_char      local_node_hash_id[16]; ///< Local node hash ID
        u_char      remote_node_hash_id[16]; ///< Remove node hash ID
        uint8_t     admin_group[4];         ///< Admin group
        double      max_link_bw;            ///< Maximum link bandwidth
        double      max_resv_bw;            ///< Maximum reserved bandwidth
        uint8_t     unreserved_bw[32];      ///< Binary string for unreserved bandwidth, a set of 8 uint32_t values

        uint32_t    te_def_metric;          ///< Default TE metric
        char        protection_type[60];    ///< String representation for the protection types
        char        mpls_proto_mask[32];    ///< Either LDP or RSVP-TE
        uint32_t    igp_metric;             ///< IGP metric
        char        srlg[128];              ///< String representation of the shared risk link group values
        char        name[255];              ///< Name of router
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /**
     * TABLE: ls_prefix
     *
     * BGP-LS Prefix table schema
     */
    #define TBL_NAME_LS_PREFIX "ls_prefixes"
    struct tbl_ls_prefix {
        u_char      hash_id[16];            ///< hash for the entry
        u_char      path_atrr_hash_id[16];  ///< Path attribute hash ID
        u_char      peer_hash_id[16];       ///< Peer table hash_id
        uint64_t    id;                     ///< Routing universe identifier
        char        protocol[32];           ///< String representation of the protocol name
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
        uint32_t    timestamp_secs;         ///< Timestamp in seconds since EPOC
    };

    /* ---------------------------------------------------------------------------
     * Abstract methods
     * ---------------------------------------------------------------------------
     */
    virtual ~DbInterface() { };

    /*****************************************************************//**
     * \brief       Add/Update a BGP peer entry
     * 
     * \details     Will add a new BGP peer if one doesn't already
     *              exist.  If one exits, the current entry will
     *              be updated.
     * 
     * \param[in,out]   peer    BGP peer entry
     *
     * \returns     The peer.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     ****************************************************************/
    virtual void add_Peer(tbl_bgp_peer &peer) = 0;

    /*****************************************************************//**
     * \brief       Update a peer entry
     *
     * \details     Will update a peer entry (does not add an entry)
     *
     * \param[in]   peer      BGP peer entry  (hash must already be defined)
     *
     * \returns     True if updated, False if not.
     *****************************************************************/
    virtual bool update_Peer(struct tbl_bgp_peer &peer) = 0;

    /*****************************************************************//**
     * \brief       Add/Update a router entry
     *
     * \details     Will add a new router or update an existing
     *              router.
     *
     * \param[in,out]   router          Router entry
     * \param[in]       incConnectcount Indicates true to update the connection count
     *                                  Default is true, to update the count.
     *
     * \returns     The router.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_Router(struct tbl_router &r_entry, bool incConnectCount = true) = 0;

    /*****************************************************************//**
     * \brief       Update a router entry
     *
     * \details     Will update a router entry (does not add a entry)
     *
     * \param[in]   router      Router entry  (hash must already be defined)
     *
     * \returns     True if updated, False if not.
     *****************************************************************/
    virtual bool update_Router(struct tbl_router &r_entry) = 0;

    /*****************************************************************//**
     * \brief       Indicates/updates a router entry for disconnected state
     *
     * \details     Updates the router entry to be in disconnected state and
     *              purges memory for it's cached state.
     *
     * \param[in]   router      Router entry  (hash must already be defined)
     *
     * \returns     True if updated, False if not.
     *****************************************************************/
    virtual bool disconnect_Router(struct tbl_router &r_entry) = 0;

    /*****************************************************************//**
     * \brief       Add/Update RIB entries
     *
     * \details     Will add new RIB prefixies or update an existing
     *              ones if they already exist.
     *
     * \param[in,out]  rib      List of one or more RIB entries
     *
     * \returns     The rib.hash_id will be updated based on the
     *              supplied data for each entry.
     * 
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_Rib(std::vector<tbl_rib> &rib) = 0;

    /*****************************************************************//**
     * \brief       Delete RIB entries
     *
     * \details     Will delete RIB prefixes.
     *
     * \param[in,out]   rib      List of one or more RIB entries
     *
     * \returns     The rib.hash_id will be updated based on the
     *              supplied data.
     * 
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void delete_Rib(std::vector<tbl_rib> &rib) = 0;

    /*****************************************************************//**
     * \brief       Add/Update path entries
     *
     * \details     Will add a new path entry or update existing.
     *
     * \param[in,out]   path      Path entry
     *
     * \returns     The path.hash_id will be updated based on the
     *              supplied data.
     * 
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_PathAttrs(tbl_path_attr &path) = 0;

    /**
     * \brief       Add/Update AS path analysis entry
     *
     * \details     Will add a new AS entry or update an existing one in the analysis table
     *
     * \param[in,out]   record      AS analysis entry
     */
    virtual void add_AsPathAnalysis(tbl_as_path_analysis &record) = 0;

    /*****************************************************************//**
     * \brief       Add a stats report entry
     *
     * \details     Will add a new stats report entry.
     *
     * \param[in,out]   stats      Stats report entry
     *****************************************************************/
    virtual void add_StatReport(tbl_stats_report &stats) = 0;

    /*****************************************************************//**
     * \brief       Add peer down event
     *
     * \details     Will add a new peer down event entry
     *
     * \param[in,out]   down_event      Peer down entry
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_PeerDownEvent(tbl_peer_down_event &down_event) = 0;

    /*****************************************************************//**
     * \brief       Add peer up event
     *
     * \details     Will add a new peer up event entry
     *
     * \param[in,out]   up_event      Peer up entry
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_PeerUpEvent(tbl_peer_up_event &up_event) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS nodes
     *
     * \details     Will add/update BGP-LS nodes.
     *
     * \param[in]   nodes      List of one or more node tables
     *****************************************************************/
    virtual void add_LsNodes(std::list<DbInterface::tbl_ls_node> &nodes) = 0;

    /*****************************************************************//**
     * \brief       Delete BGP-LS nodes
     *
     * \details     Will delete BGP-LS nodes, including associated links and prefixes.
     *
     * \param[in]   nodes      List of one or more node tables
     *****************************************************************/
    virtual void del_LsNodes(std::list<DbInterface::tbl_ls_node> &nodes) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS links
     *
     * \details     Will add/update BGP-LS links.
     *
     * \param[in]   links      List of one or more link tables
     *
     * \returns     The hash_id will be updated based on the
     *              supplied data for each entry.
     *****************************************************************/
    virtual void add_LsLinks(std::list<DbInterface::tbl_ls_link> &links) = 0;

    /*****************************************************************//**
     * \brief       Delete BGP-LS links
     *
     * \details     Will delete BGP-LS links.
     *
     * \param[in]   links      List of one or more link tables
     *****************************************************************/
    virtual void del_LsLinks(std::list<DbInterface::tbl_ls_link> &links) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS prefixes
     *
     * \details     Will add/update BGP-LS prefixes.
     *
     * \param[in/out]  prefixes      List of one or more node tables
     *
     * \returns     The hash_id will be updated based on the
     *              supplied data for each entry.
     *****************************************************************/
    virtual void add_LsPrefixes(std::list<DbInterface::tbl_ls_prefix> &prefixes) = 0;

    /*****************************************************************//**
     * \brief       Delete BGP-LS prefixes
     *
     * \details     Will delete BGP-LS prefixes.
     *
     * \param[in/out]   prefixes      List of one or more prefixes tables
     *****************************************************************/
    virtual void del_LsPrefixes(std::list<DbInterface::tbl_ls_prefix> &prefixes) = 0;


    /*****************************************************************//**
     * \brief       Start transaction
     *
     * \details     See https://dev.mysql.com/doc/refman/5.6/en/commit.html or
     *              http://www.postgresql.org/docs/9.4/static/sql-start-transaction.html
     *              for more details.
     *
     *              Make sure to call "commitTransaction()" method when ready to commit.
     *****************************************************************/
    virtual void startTransaction() = 0;

    /*****************************************************************//**
     * \brief       Commit transaction
     *
     * \details     See https://dev.mysql.com/doc/refman/5.6/en/commit.html or
     *              http://www.postgresql.org/docs/9.4/static/sql-start-transaction.html
     *              for more details.
     *****************************************************************/
    virtual void commitTransaction() = 0;

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
    void hash_toStr(const u_char *hash_bin, std::string &hash_str){

        int i;
        char s[33];

        for (i=0; i<16; i++)
            sprintf(s+i*2, "%02x", hash_bin[i]);
        
        s[32]='\0';

        hash_str = s;
    }

    /**
     * \brief   set maximum blob text field size
     *
     * \details The maximum blob text field size is used for items such as 
     *          as_path, community string lists, and initial/termination data items.
     */
    void setMaxBlobSize (uint16_t size) {
        max_blob_field_size = size;
    }

    /**
     * \brief   get maximum blob text field size
     *
     * \details The maximum blob text field size is used for items such as
     *          as_path, community string lists, and initial/termination data items.
     */
    uint16_t getMaxBlobSize () {
        return max_blob_field_size;
    }

protected:
    uint16_t    max_blob_field_size;    ///< Maximum blob text field size

private:

};

#endif

