/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_CONFIG_H
#define OPENBMP_CONFIG_H

#include <string>
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <openssl/md5.h>
#include <boost/xpressive/xpressive.hpp>
#include <boost/exception/all.hpp>


using namespace std;

class Config {
public:
    /*********************************************************************//**
     * Singleton class
     ***********************************************************************/
    // initialize singleton logger
    static Config* init();
    // get logger
    static Config* get_config();
    // delete methods that cause problems to the singleton
    Config(Config const&) = delete;
    void operator=(Config const&)  = delete;
    ~Config();

    /* Method to load variables from a config file */
    void load(const char *config_filename);

    /* Config Variables */
    bool daemon;             ///run the program foreground
    const char *cfg_filename; // Configuration file name to load/read
    string log_filename; // Output file to log messages to
    string debug_filename; // Debug file to log messages to
    string pid_filename; // PID file to record the daemon pid

    // Collector Hash ID (raw format); MD5 digest length is 16 bytes
    unsigned char collector_hash_id[MD5_DIGEST_LENGTH];
    // Collector name
    unsigned char collector_name[64];
    // Collector group
    string collector_group;
    // Collector hearbeat interval
    int heartbeat_interval = 10;

    uint16_t bmp_port;               // BMP listening port
    string bind_ipv4;                // IP to listen on for IPv4
    string bind_ipv6;                // IP to listen on for IPv6

    int bmp_ring_buffer_size = 5;      // BMP buffer size in bytes (min is 2M max is 128M)
    int max_rib_waiting_workers = 10;
    float max_cpu_utilization = 0.9;     // CPU utilization cap of the program

    bool svr_ipv4;                 // Indicates if server should listen for IPv4 connections
    bool svr_ipv6;                 // Indicates if server should listen for IPv6 connections

    bool debug_all;
    bool debug_collector;
    bool debug_worker;
    bool debug_encapsulator;
    bool debug_message_bus;

    /*
     * kafka topic templates
     */
    string topic_template_collector;
    string topic_template_router;
    string topic_template_bmp_raw;

    /**
     * matching structs and maps
     */
    struct match_type_regex {
        boost::xpressive::sregex  regexp;    ///< Compiled regular expression
    };

    struct match_type_ip {
        bool        is_ipv4;                                     ///< Indicates if IPv4 or IPv6
        uint32_t    prefix[4]  __attribute__ ((aligned));       ///< IP/network prefix to match (host bits are zeroed)
        uint8_t     bits;                                       ///< bits to match
    };

    /*
     * router_group and peer_group match conditions
     * the match conditions are defined in the openbmp.conf
     * peer-related information will require bmp peer header parsing
     */
    map<string, list<match_type_regex>> match_router_group_by_name;
    map<string, list<match_type_ip>> match_router_group_by_ip;
    map<string, list<match_type_regex>> match_peer_group_by_name;
    map<string, list<match_type_ip>> match_peer_group_by_ip;
    map<string, list<uint32_t>> match_peer_group_by_asn;

    /*
     * librdkafka passthrough configurations
     */
    map<string, string> librdkafka_passthrough_configs;


private:
    /* private constructor */
    Config();
    static Config* singleton_instance;

    static void print_warning(string msg, const YAML::Node &node);

    /*
     * a set of parsing methods to parse different sections of openbmp config file.
     */
    void parse_base(const YAML::Node &node);

    void parse_debug(const YAML::Node &node);

    void parse_librdkafka_config(const YAML::Node &node);

    void parse_kafka_topic_template(const YAML::Node &node);

    void parse_grouping(const YAML::Node &node);

    void parse_prefix_list(const YAML::Node &node, string name,
                                   map<string, list<match_type_ip>> &map);

    void parse_regexp_list(const YAML::Node &node, string name,
                                   map<string, list<match_type_regex>> &map);

};

#endif //OPENBMP_CONFIG_H
