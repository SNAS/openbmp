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
#include "KafkaPeerPartitionerCallback.h"

/**
 * \class   mysqlMBP
 *
 * \brief   Mysql database implementation
 * \details Enables a DB backend using mysql 5.5 or greater.
  */
class msgBus_kafka: public MsgBusInterface {
public:
    #define MSGBUS_WORKING_BUF_SIZE 1800000

    #define MSGBUS_TOPIC_COLLECTOR              "openbmp.parsed.collector"
    #define MSGBUS_TOPIC_ROUTER                 "openbmp.parsed.router"
    #define MSGBUS_TOPIC_PEER                   "openbmp.parsed.peer"
    #define MSGBUS_TOPIC_BASE_ATTRIBUTE         "openbmp.parsed.base_attribute"
    #define MSGBUS_TOPIC_UNICAST_PREFIX         "openbmp.parsed.unicast_prefix"
    #define MSGBUS_TOPIC_LS_NODE                "openbmp.parsed.ls_node"
    #define MSGBUS_TOPIC_LS_LINK                "openbmp.parsed.ls_link"
    #define MSGBUS_TOPIC_LS_PREFIX              "openbmp.parsed.ls_prefix"
    #define MSGBUS_TOPIC_BMP_STAT               "openbmp.parsed.bmp_stat"
    #define MSGBUS_TOPIC_BMP_RAW                "openbmp.bmp_raw"

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

    void send_bmp_raw(u_char *r_hash, u_char *data, size_t data_len);

    // Debug methods
    void enableDebug();
    void disableDebug();

private:
    char            *prep_buf;                  ///< Large working buffer for message preparation
    unsigned char   *producer_buf;              ///< Producer message buffer
    bool            debug;                      ///< debug flag to indicate debugging
    Logger          *logger;                    ///< Logging class pointer

    std::string     collector_hash;             ///< collector hash string value

    uint64_t        router_seq;                 ///< Router add/del sequence
    uint64_t        collector_seq;              ///< Collector add/del sequence
    uint64_t        peer_seq ;                  ///< Peer add/del sequence
    uint64_t        base_attr_seq;              ///< Base attribute sequence
    uint64_t        unicast_prefix_seq;         ///< Unicast prefix sequence
    uint64_t        bmp_stat_seq;               ///< BMP stats sequence
    uint64_t        ls_node_seq;                ///< LS node sequence
    uint64_t        ls_link_seq;                ///< LS link sequence
    uint64_t        ls_prefix_seq;              ///< LS prefix sequence

    std::string     broker_list;                ///< Broker list in the format of <host>:<port>[,...]

    /**
     * Kafka Configuration object (global and topic level)
     */
    RdKafka::Conf   *conf;
    RdKafka::Conf   *tconf;

    RdKafka::Producer *producer;                ///< Kafka Producer instance

    /**
     * Callback handlers
     */
    KafkaEventCallback              *event_callback;
    KafkaDeliveryReportCallback     *delivery_callback;
    KafkaPeerPartitionerCallback    *peer_partitioner_callback;


    bool isConnected;                           ///< Indicates if Kafka is connected or not

    // array of hashes
    std::map<std::string, time_t> peer_list;
    typedef std::map<std::string, time_t>::iterator peer_list_iter;

    std::string router_ip;                      ///< Router IP in printed format
    u_char      router_hash[16];                ///< Router Hash in binary format

    /**
     * Topic name to handle pointer map (key=Name, value=topic pointer)
     */
    typedef std::map<std::string, RdKafka::Topic *> topic_map;

    void initTopicMap() {
        topic = {
                { const_cast<char *>(MSGBUS_TOPIC_COLLECTOR),        (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_ROUTER),           (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_PEER),             (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_BASE_ATTRIBUTE),   (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_UNICAST_PREFIX),   (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_LS_NODE),          (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_LS_LINK),          (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_LS_PREFIX),        (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_BMP_STAT),         (RdKafka::Topic *)NULL},
                { const_cast<char *>(MSGBUS_TOPIC_BMP_RAW),          (RdKafka::Topic *)NULL}
        };
    };

    std::map<std::string, RdKafka::Topic*> topic;

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
     * \param [in] rows          Number of rows in data
     * \param [in] key           Hash key
     */
    void produce(const char *topic_name, char *msg, size_t msg_size, int rows, std::string key);

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
