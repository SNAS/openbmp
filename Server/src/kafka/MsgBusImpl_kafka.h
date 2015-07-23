/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef MSGBUSIMPL_KAFKA_H_
#define MSGBUSIMPL_KAFKA_H_

#define HASH_SIZE 16

#include "MsgBusInterface.hpp"
#include "Logger.h"
#include <string>
#include <map>
#include <vector>
#include <ctime>

#include <librdkafka/rdkafkacpp.h>

#include <thread>
#include "safeQueue.hpp"
#include "KafkaEventCallback.h"
#include "KafkaDeliveryReportCallback.h"

/**
 * \class   mysqlMBP
 *
 * \brief   Mysql database implementation
 * \details Enables a DB backend using mysql 5.5 or greater.
  */
class msgBus_kafka: public MsgBusInterface {
public:
    /**
     * Topic name to handle pointer map (key=Name, value=topic pointer)
     */
    typedef std::map<std::string, RdKafka::Topic *> topic_map;
    std::map<std::string, RdKafka::Topic*> topic = {
            #define MSGBUS_TOPIC_COLLECTOR              "openbmp.parsed.collector"
            { MSGBUS_TOPIC_COLLECTOR,        NULL},

            #define MSGBUS_TOPIC_ROUTER                 "openbmp.parsed.router"
            { MSGBUS_TOPIC_ROUTER,           NULL},

            #define MSGBUS_TOPIC_PEER                   "openbmp.parsed.peer"
            { MSGBUS_TOPIC_PEER,             NULL},

            #define MSGBUS_TOPIC_BASE_ATTRIBUTE         "openbmp.parsed.base_attribute"
            { MSGBUS_TOPIC_BASE_ATTRIBUTE,   NULL},

            #define MSGBUS_TOPIC_UNICAST_PREFIX         "openbmp.parsed.unicast_prefix"
            { MSGBUS_TOPIC_UNICAST_PREFIX,   NULL},

            #define MSGBUS_TOPIC_LS_NODE                "openbmp.parsed.ls_node"
            { MSGBUS_TOPIC_LS_NODE,          NULL},

            #define MSGBUS_TOPIC_LS_LINK                "openbmp.parsed.ls_link"
            { MSGBUS_TOPIC_LS_LINK,          NULL},

            #define MSGBUS_TOPIC_LS_PREFIX              "openbmp.parsed.ls_prefix"
            { MSGBUS_TOPIC_LS_PREFIX,        NULL},

            #define MSGBUS_TOPIC_BMP_STAT               "openbmp.parsed.bmp_stat"
            { MSGBUS_TOPIC_BMP_STAT,         NULL}
    };

    /******************************************************************//**
     * \brief This function will initialize and connect to MySQL.  
     *
     * \details It is expected that this class will start off with a new connection.
     *
     *  \param [in] logPtr      Pointer to Logger instance
     *  \param [in] brokerList  Comma delimited list of brokers (e.g. localhost:9092,host2:9092)
     *  \param [in] c_hash_id   Collector Hash ID
     ********************************************************************/
    msgBus_kafka(Logger *logPtr, std::string brokerList, u_char *c_hash_id);
    ~msgBus_kafka();

    /*
     * abstract methods implemented
     * See MsgBusInterface.hpp for method details
     */
    void update_Collector(struct obj_collector &c_obj, collector_action_code action_code);
    void update_Router(struct obj_router &r_entry, router_action_code code);
    void update_Peer(obj_bgp_peer &peer, obj_peer_up_event *up, obj_peer_down_event *down, peer_action_code code);
    void update_baseAttribute(obj_bgp_peer &peer, obj_path_attr &attr, base_attr_action_code code);
    void update_unicastPrefix(obj_bgp_peer &peer, std::vector<obj_rib> &rib, obj_path_attr *attr, unicast_prefix_action_code code);
    void add_StatReport(obj_bgp_peer &peer, obj_stats_report &stats);

    void update_LsNode(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_node> &nodes,
                     ls_action_code code);
    void update_LsLink(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_link> &links,
                     ls_action_code code);
    void update_LsPrefix(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_prefix> &prefixes,
                      ls_action_code code);

    // Debug methods
    void enableDebug();
    void disableDebug();

private:
    bool            debug;                      ///< debug flag to indicate debugging
    Logger          *logger;                    ///< Logging class pointer

    std::string     collector_hash;             ///< collector hash string value

    uint64_t        router_seq  = 0L;           ///< Router add/del sequence
    uint64_t        collector_seq = 0L;         ///< Collector add/del sequence
    uint64_t        peer_seq = 0L;              ///< Peer add/del sequence
    uint64_t        base_attr_seq = 0L;         ///< Base attribute sequence
    uint64_t        unicast_prefix_seq = 0L;    ///< Unicast prefix sequence
    uint64_t        bmp_stat_seq = 0L;          ///< BMP stats sequence
    uint64_t        ls_node_seq = 0L;           ///< LS node sequence
    uint64_t        ls_link_seq = 0L;           ///< LS link sequence
    uint64_t        ls_prefix_seq = 0L;         ///< LS prefix sequence

    std::string     broker_list;                ///< Broker list in the format of <host>:<port>[,...]

    /**
     * Kafka Configuration object (global and topic level)
     */
    RdKafka::Conf   *conf       = NULL;
    RdKafka::Conf   *tconf      = NULL;

    RdKafka::Producer *producer = NULL;                ///< Kafka Producer instance

    /**
     * Callback handlers
     */
    KafkaEventCallback              *event_callback      = NULL;
    KafkaDeliveryReportCallback     *delivery_callback   = NULL;


    bool isConnected;                           ///< Indicates if Kafka is connected or not

    // array of hashes
    typedef std::map<std::string, time_t>::iterator router_list_iter;
    std::map<std::string, time_t> router_list;
    std::map<std::string, time_t> peer_list;
    typedef std::map<std::string, time_t>::iterator peer_list_iter;

    std::string router_ip;                      ///< Router IP in printed format, used for logging

    /**
     * Connects to kafka broker
     */
    void connect();

    /**
     * produce message to Kafka
     *
     * \param [in] topic]        Topic name to lookup in the topic map
     * \param [in] msg           message to produce
     * \param [in] msg_size      Length in bytes of the message
     * \param [in] key           Hash key
     */
    void produce(const char *topic_name, char *msg, size_t msg_size, std::string key);

    /**
    * \brief Method to resolve the IP address to a hostname
    *
    *  \param [in]   name      String name (ip address)
    *  \param [out]  hostname  String reference for hostname
    *
    *  \returns true if error, false if no error
    */
    bool resolveIp(std::string name, std::string &hostname);


};

#endif /* MSGBUSIMPL_KAFKA_H_ */
