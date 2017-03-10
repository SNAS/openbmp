/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_KAFKATOPICSELECTOR_H
#define OPENBMP_KAFKATOPICSELECTOR_H

#include <librdkafka/rdkafkacpp.h>
#include "Config.h"
#include "Logger.h"
#include "KafkaPeerPartitionerCallback.h"

class KafkaTopicSelector {
public:
    /**
     * MSGBUS_TOPIC_* defines the default topic names
     */
    #define MSGBUS_TOPIC_COLLECTOR              "openbmp.parsed.collector"
    #define MSGBUS_TOPIC_ROUTER                 "openbmp.parsed.router"
    #define MSGBUS_TOPIC_PEER                   "openbmp.parsed.peer"
    #define MSGBUS_TOPIC_BASE_ATTRIBUTE         "openbmp.parsed.base_attribute"
    #define MSGBUS_TOPIC_UNICAST_PREFIX         "openbmp.parsed.unicast_prefix"
    #define MSGBUS_TOPIC_L3VPN                  "openbmp.parsed.l3vpn"
    #define MSGBUS_TOPIC_EVPN                   "openbmp.parsed.evpn"
    #define MSGBUS_TOPIC_LS_NODE                "openbmp.parsed.ls_node"
    #define MSGBUS_TOPIC_LS_LINK                "openbmp.parsed.ls_link"
    #define MSGBUS_TOPIC_LS_PREFIX              "openbmp.parsed.ls_prefix"
    #define MSGBUS_TOPIC_BMP_STAT               "openbmp.parsed.bmp_stat"
    #define MSGBUS_TOPIC_BMP_RAW                "openbmp.bmp_raw"

    #define MSGBUS_TOPIC_UNICAST_PREFIX_TEMPLATED   "openbmp.parsed.unicast_prefix_templated"
    #define MSGBUS_TOPIC_LS_NODE_TEMPLATED          "openbmp.parsed.ls_node_templated"
    #define MSGBUS_TOPIC_LS_LINK_TEMPLATED          "openbmp.parsed.ls_link_templated"
    #define MSGBUS_TOPIC_LS_PREFIX_TEMPLATED          "openbmp.parsed.ls_prefix_templated"
    #define MSGBUS_TOPIC_L3VPN_TEMPLATED          "openbmp.parsed.l3vpn_templated"
    #define MSGBUS_TOPIC_EVPN_TEMPLATED          "openbmp.parsed.evpn_templated"

    /**
     * MSGBUS_TOPIC_VAR_* defines the topic var/key for the topic maps.
     *      This matches the config topic var name, which is the map key.
     */
    #define MSGBUS_TOPIC_VAR_COLLECTOR          "collector"
    #define MSGBUS_TOPIC_VAR_ROUTER             "router"
    #define MSGBUS_TOPIC_VAR_PEER               "peer"
    #define MSGBUS_TOPIC_VAR_BASE_ATTRIBUTE     "base_attribute"
    #define MSGBUS_TOPIC_VAR_UNICAST_PREFIX     "unicast_prefix"
    #define MSGBUS_TOPIC_VAR_L3VPN              "l3vpn"
    #define MSGBUS_TOPIC_VAR_EVPN               "evpn"
    #define MSGBUS_TOPIC_VAR_LS_NODE            "ls_node"
    #define MSGBUS_TOPIC_VAR_LS_LINK            "ls_link"
    #define MSGBUS_TOPIC_VAR_LS_PREFIX          "ls_prefix"
    #define MSGBUS_TOPIC_VAR_BMP_STAT           "bmp_stat"
    #define MSGBUS_TOPIC_VAR_BMP_RAW            "bmp_raw"

    #define MSGBUS_TOPIC_VAR_UNICAST_PREFIX_TEMPLATED   "unicast_prefix_templated"
    #define MSGBUS_TOPIC_VAR_LS_NODE_TEMPLATED          "ls_node_templated"
    #define MSGBUS_TOPIC_VAR_LS_LINK_TEMPLATED          "ls_link_templated"
    #define MSGBUS_TOPIC_VAR_LS_PREFIX_TEMPLATED        "ls_prefix_templated"
    #define MSGBUS_TOPIC_VAR_L3VPN_TEMPLATED            "l3vpn_templated"
    #define MSGBUS_TOPIC_VAR_EVPN_TEMPLATED             "evpn_templated"


    /*********************************************************************//**
     * Constructor for class
     *
     * \param [in] logPtr   Pointer to Logger instance
     * \param [in] cfg      Pointer to the config instance
     * \param [in] producer Pointer to the kafka producer
     ***********************************************************************/
    KafkaTopicSelector(Logger *logPtr, Config *cfg,  RdKafka::Producer *producer);

    /*********************************************************************//**
     * Destructor for class
     ***********************************************************************/
    ~KafkaTopicSelector();

    /*********************************************************************//**
     * Gets topic pointer by topic var name, router and peer group.  If the topic doesn't exist, a new entry
     *      will be initialized.
     *
     * \param [in]  topic_var       MSGBUS_TOPIC_VAR_<name>
     * \param [in]  router_group    Router group - empty/NULL means no router group
     * \param [in]  peer_group      Peer group - empty/NULL means no peer group
     * \param [in]  peer_asn        Peer asn (remote asn)
     *
     * \return (RdKafka::Topic *) pointer or NULL if error
     ***********************************************************************/
    RdKafka::Topic * getTopic(const std::string &topic_var, const std::string *router_group,
                              const std::string *peer_group,
                              uint32_t peer_asn);

    /*********************************************************************//**
     * Lookup router group
     *
     * \param [in]  hostname          hostname/fqdn of the router
     * \param [in]  ip_addr           IP address of the peer (printed form)
     * \param [out] router_group_name Reference to string where router group will be updated
     *
     * \return bool true if matched, false if no matched peer group
     ***********************************************************************/
    void lookupRouterGroup(std::string hostname, std::string ip_addr, std::string &router_group_name);

    /*********************************************************************//**
     * Lookup peer group
     *
     * \param [in]  hostname        hostname/fqdn of the peer
     * \param [in]  ip_addr         IP address of the peer (printed form)
     * \param [in]  peer_asn        Peer ASN
     * \param [out] peer_group_name Reference to string where peer group will be updated
     *
     * \return bool true if matched, false if no matched peer group
     ***********************************************************************/
    void lookupPeerGroup(std::string hostname, std::string ip_addr, uint32_t peer_asn,
                         std::string &peer_group_name);


private:
    Config          *cfg;                       ///< Configuration instance
    Logger          *logger;                    ///< Logging class pointer
    bool            debug;                      ///< debug flag to indicate debugging


    RdKafka::Producer *producer;                ///< Kafka Producer instance
    RdKafka::Conf     *tconf;                   ///< rdkafka topic level configuration

    ///< Partition callback for peer
    KafkaPeerPartitionerCallback *peer_partitioner_callback;

    /**
     * Topic name to rdkafka pointer map (key=Name, value=topic pointer)
     *
     *      Key will be MSGBUS_TOPIC_VAR_<topic>_<router_group>_<peer_group>[_<peer_asn>]
     *          Keys will not contain the optional values unless topic_flags_map includes them.
     *
     *      If router_group or peer group is empty, the key will include them as empty values.
     *      Eg. unicast_prefix___ (both router and peer groups are empty)
     *          unicast_prefix_routergrp1__ (peer group is empty but router group is defined)
     *          unicast_prefix__peergrp1_ (router group is empty but peer group is defined)
     *          unicast_prefix_routergrp1_peergroup1_ (both router and peer groups are defiend)
     */
    typedef std::map<std::string, RdKafka::Topic *> topic_map;
    std::map<std::string, RdKafka::Topic*> topic;


    /**
     * Topic flags and map define various flags per topic var
     */
    struct topic_flags {
        bool include_peerAsn;           ///< Indicates if peer ASN should be included in the topic key
    };
    std::map<std::string, topic_flags> topic_flags_map;     ///< Map key is one of MSGBUS_TOPIC_VAR_<topic>

    /**
     * Free allocated topic map pointers
     */
    void freeTopicMap();

    /**
     * Initialize topic
     *      Producer must be initialized and connected prior to calling this method.
     *      Topic map will be updated.
     *
     * \param [in]  topic_var       MSGBUS_TOPIC_VAR_<name>
     * \param [in]  router_group    Router group - empty/NULL means no router group
     * \param [in]  peer_group      Peer group - empty/NULL means no peer group
     * \param [in]  peer_asn        Peer asn (remote asn)
     *
     * \return  (RdKafka::Topic *) pointer or NULL if error
     */
    RdKafka::Topic * initTopic(const std::string &topic_var,
                               const std::string *router_group, const std::string *peer_group,
                               uint32_t peer_asn);

    /**
     * Get the topic map key name
     *
     * \param [in]  topic_var       MSGBUS_TOPIC_VAR_<name>
     * \param [in]  router_group    Router group - empty/NULL means no router group
     * \param [in]  peer_group      Peer group - empty/NULL means no peer group
     * \param [in]  peer_asn        Peer asn (remote asn)
     *
     * \return string value of the topic key to be used with the topic map
     */
    std::string getTopicKey(const std::string &topic_var,
                            const std::string *router_group, const std::string *peer_group,
                            uint32_t peer_asn);


};


#endif //OPENBMP_KAFKATOPICSELECTOR_H
