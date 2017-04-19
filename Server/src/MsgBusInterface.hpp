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

#include "parseBgpLib.h"
#include "template_cfg.h"

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

    /// Base attribute action codes
    enum base_attr_action_code {
        BASE_ATTR_ACTION_ADD=0
    };

    /// Unicast prefix action codes
    enum unicast_prefix_action_code {
        UNICAST_PREFIX_ACTION_ADD=0,
        UNICAST_PREFIX_ACTION_DEL,
    };

    /// Vpn action codes
    enum vpn_action_code {
        VPN_ACTION_ADD=0,
        VPN_ACTION_DEL,
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

    /// LS action code (node, link, and prefix)
    enum ls_action_code {
        LS_ACTION_ADD=0,
        LS_ACTION_DEL
    };

    /* ---------------------------------------------------------------------------
     * Abstract methods
     * ---------------------------------------------------------------------------
     */
    virtual ~MsgBusInterface() { };

    /*****************************************************************//**
     * \brief       Add/Update a collector object templated
     *
     * \details     Will generate a message for a new/updated collector based on template.
     *
     * \param[in,out]   collector       Router object
     * \param[in]       code            Action code for collector update
     * \param[in]       template Template
     *
    *****************************************************************/
    virtual void update_Collector(parse_bgp_lib::parseBgpLib::collector_map &collector,
                                        collector_action_code action_code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update a router object templated
     *
     * \details     Will generate a message to add a new router or update an existing
     *              router.
     *
     * \param[in,out]   router          Router object
     * \param[in]       code            Action code for router update
     * \param[in]       template Template
     *
     * \returns     The router.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_Router(parse_bgp_lib::parseBgpLib::router_map &router,
                               router_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update a peer object templated
     *
     * \details     Will generate a message to add a new router or update an existing
     *              router.
     *
     * \param[in,out]   peer          Peer object
     * \param[in]       code            Action code for router update
     * \param[in]       template Template
     *
     * \returns     The router.hash_id will be updated based on the
     *              supplied data.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_Peer(parse_bgp_lib::parseBgpLib::router_map &router,
                                      parse_bgp_lib::parseBgpLib::peer_map &peer,
                                      peer_action_code code, template_cfg::Template_cfg &template_container) = 0;


    /*****************************************************************//**
     * \brief       Add/Update  base path attributes templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_baseAttribute(parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                      parse_bgp_lib::parseBgpLib::peer_map &peer,
                                      parse_bgp_lib::parseBgpLib::router_map &router,
                                       base_attr_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update RIB objects templated
     *
     * \details     Will generate a message to add new RIB prefixes
     *
     * \param[in]       peer    Peer object
     * \param[in,out]   rib     List of one or more RIB entries
     * \param[in]       attr    Path attribute object (can be null if n/a)
     * \param[in]       code    Unicast prefix action code
     * \param[in]       template Template
     *
     * \returns     The rib.hash_id will be updated based on the
     *              supplied data for each object.
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void update_unicastPrefix(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                      parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                               parse_bgp_lib::parseBgpLib::peer_map &peer,
                                               parse_bgp_lib::parseBgpLib::router_map &router,
                                               unicast_prefix_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add Stats object templated
     *
     * \details     Will generate a message to add new RIB prefixes
     *
     * \param[in]       peer    Peer object
     * \param[in]       router  Router object
     * \param[in]       stats   Stats object
     * \param[in]       template Template
     *
     * \note        Caller must free any allocated memory, which is
     *              safe to do so when this method returns.
     *****************************************************************/
    virtual void add_StatReport(parse_bgp_lib::parseBgpLib::peer_map &peer,
                                         parse_bgp_lib::parseBgpLib::router_map &router,
                                         parse_bgp_lib::parseBgpLib::stat_map stats, template_cfg::Template_cfg &template_container) = 0;

 /*****************************************************************//**
     * \brief       Add/Update BGP-LS nodes templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_LsNode(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &ls_node_list,
                                        parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                        parse_bgp_lib::parseBgpLib::peer_map &peer,
                                        parse_bgp_lib::parseBgpLib::router_map &router,
                                        ls_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS links templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_LsLink(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &ls_link_list,
                                        parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                        parse_bgp_lib::parseBgpLib::peer_map &peer,
                                        parse_bgp_lib::parseBgpLib::router_map &router,
                                        ls_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update BGP-LS prefixes templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_LsPrefix(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &ls_prefix_list,
                                        parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                        parse_bgp_lib::parseBgpLib::peer_map &peer,
                                        parse_bgp_lib::parseBgpLib::router_map &router,
                                        ls_action_code code, template_cfg::Template_cfg &template_container) = 0;

    /*****************************************************************//**
     * \brief       Add/Update L3 VPN templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_L3Vpn(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &l3Vpn_list,
                                          parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                          parse_bgp_lib::parseBgpLib::peer_map &peer,
                                          parse_bgp_lib::parseBgpLib::router_map &router,
                                          vpn_action_code code, template_cfg::Template_cfg &template_container) = 0;


    /*****************************************************************//**
     * \brief       Add/Update  eVPN templated
     *
     * \details     Will generate a message to add/update BGP-LS nodes.
     *
     * \param[in]   peer       Peer object
     * \param[in]   nodes      List of one or more node tables
     * \param[in]   attr       Path attribute object
     * \param[in]   code       Linkstate action code
     *****************************************************************/
    virtual void update_eVpn(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &eVpn_list,
                                       parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                       parse_bgp_lib::parseBgpLib::peer_map &peer,
                                       parse_bgp_lib::parseBgpLib::router_map &router,
                                       vpn_action_code code, template_cfg::Template_cfg &template_container) = 0;

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

    Template_map *template_map;

protected:

private:

};

/**
 * \brief       Simple concatanation function
 *
 * \details     Converts a string array to a concatanated string
 *              printing or storing in the DB.
 *
 * \param[in]   lib_data      nlri or attr map value
 */
static std::string map_string(std::list<std::string> &lib_data) {
    string s = "";

    if (lib_data.empty())
        return s;

    if (lib_data.size() <= 1)
        return lib_data.front();
    std::list<std::string>::iterator last_value = lib_data.end();
    last_value--;

    for (std::list<std::string>::iterator it = lib_data.begin(); it != lib_data.end(); it++) {
        s += *it;
        if (it != last_value) {
            s += std::string(", ");
        }
    }
    return s;
}


#endif
