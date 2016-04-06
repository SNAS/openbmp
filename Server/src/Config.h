/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <list>
#include <map>
#include <yaml-cpp/yaml.h>
#include <boost/xpressive/xpressive.hpp>
#include <boost/exception/all.hpp>

using namespace boost::xpressive;

/**
 * \class   Config
 *
 * \brief   Configuration class for openbmpd
 * \details
 *      Parses the yaml configuration file and loads value in this class instance.
 */
class Config {
public:
    u_char      c_hash_id[16];            ///< Collector Hash ID (raw format)
    char        admin_id[64];             ///< Admin ID

    std::string kafka_brokers;            ///< metadata.broker.list
    uint16_t    bmp_port;                 ///< BMP listening port

    int         bmp_buffer_size;          ///< BMP buffer size in bytes (min is 2M max is 128M)
    bool        svr_ipv4;                 ///< Indicates if server should listen for IPv4 connections
    bool        svr_ipv6;                 ///< Indicates if server should listen for IPv6 connections

    bool        debug_general;
    bool        debug_bgp;
    bool        debug_bmp;
    bool        debug_msgbus;

    int         heartbeat_interval;      ///< Heartbeat interval in seconds for collector updates
    int   	tx_max_bytes;            ///< Maximum transmit message size
    int 	rx_max_bytes;            ///< Maximum receive  message size

    /**
     * matching structs and maps
     */
    struct match_type_regex {
        boost::xpressive::sregex  regexp;    ///< Compiled regular expression
    };

    struct match_type_ip {
        bool        isIPv4;                                     ///< Indicates if IPv4 or IPv6
        uint32_t    prefix[4]  __attribute__ ((aligned));       ///< IP/network prefix to match (host bits are zeroed)
        uint8_t     bits;                                       ///< bits to match
    };

    /**
     * Matching router group map - used to regex/ip match the router to group name
     */
    std::map<std::string, std::list<match_type_regex>> match_router_group_by_name;
    typedef std::map<std::string, std::list<match_type_regex>>::iterator match_router_group_by_name_iter;

    std::map<std::string, std::list<match_type_ip>> match_router_group_by_ip;
    typedef std::map<std::string, std::list<match_type_ip>>::iterator match_router_group_by_ip_iter;


    /**
     * Matching peer group map - used to regex/ip match the peer to group name
     */
    std::map<std::string, std::list<match_type_regex>> match_peer_group_by_name;
    typedef std::map<std::string, std::list<match_type_regex>>::iterator match_peer_group_by_name_iter;

    std::map<std::string,  std::list<match_type_ip>> match_peer_group_by_ip;
    typedef std::map<std::string, std::list<match_type_ip>>::iterator match_peer_group_by_ip_iter;

    std::map<std::string,  std::list<uint32_t>> match_peer_group_by_asn;
    typedef std::map<std::string, std::list<uint32_t>>::iterator match_peer_group_by_asn_iter;

    /**
     * kafka topic variables
     */
    std::map<std::string, std::string> topic_vars_map;
    typedef std::map<std::string, std::string>::iterator topic_vars_map_iter;

    /**
     * kafka topic names
     */
    std::map<std::string, std::string> topic_names_map;
    typedef std::map<std::string, std::string>::iterator topic_names_map_iter;

    /*********************************************************************//**
     * Constructor for class
     ***********************************************************************/
    Config();

    /*********************************************************************//**
     * Load configuration from file in YAML format
     *
     * \param [in] cfg_filename     Yaml configuration filename
     ***********************************************************************/
    void load(const char *cfg_filename);

private:
    /**
     * Parse the base configuration
     *
     * \param [in] node     Reference to the yaml NODE
     */
    void parseBase(const YAML::Node &node);

    /**
     * Parse the debug configuration
     *
     * \param [in] node     Reference to the yaml NODE
     */
    void parseDebug(const YAML::Node &node);

    /**
     * Parse the kafka configuration
     *
     * \param [in] node     Reference to the yaml NODE
     */
    void parseKafka(const YAML::Node &node);

    /**
     * Parse the kafka topics configuration
     *
     * \param [in] node     Reference to the yaml NODE
     */
    void parseTopics(const YAML::Node &node);

    /**
     * Parse the mapping configuration
     *
     * \param [in] node     Reference to the yaml NODE
     */
    void parseMapping(const YAML::Node &node);

    /**
     * Parse matching prefix_range list and update the provided map with compiled expressions
     *
     * \param [in]  node     prefix_range list node - should be of type sequence
     * \param [in]  name     group name, used as the map key
     * \param [out] map      Reference to the map that will be updated with ip addresses
     */
    void parsePrefixList(const YAML::Node &node, std::string name, std::map<std::string, std::list<match_type_ip>> &map);


    /**
     * Parse matching regexp list and update the provided map with compiled expressions
     *
     * \param [in]  node     regex list node - should be of type sequence
     * \param [in]  name     group name, used as the map key
     * \param [out] map      Reference to the map that will be updated with the compiled expressions
     */
    void parseRegexpList(const YAML::Node &node, std::string name, std::map<std::string, std::list<match_type_regex>> &map);

    /**
     * print warning message for parsing node
     *
     * \param [in] msg      Warning message
     * \param [in] node     Offending node that caused the warning
     */
    void printWarning(const std::string msg, const YAML::Node &node);

    /**
     * Perform topic name substitutions based on topic variables
     */
    void topicSubstitutions();

};


#endif /* CONFIG_H_ */
