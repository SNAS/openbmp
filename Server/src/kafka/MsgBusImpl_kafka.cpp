/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <cinttypes>

#include <librdkafka/rdkafkacpp.h>
#include <netdb.h>
#include <unistd.h>

#include <thread>
#include <arpa/inet.h>

#include "MsgBusImpl_kafka.h"
#include "KafkaEventCallback.h"
#include "KafkaDeliveryReportCallback.h"
#include "KafkaTopicSelector.h"

#include <boost/algorithm/string/replace.hpp>

#include <librdkafka/rdkafka.h>


#include "md5.h"

using namespace std;

/******************************************************************//**
 * \brief This function will initialize and connect to Kafka.
 *
 * \details It is expected that this class will start off with a new connection.
 *
 *  \param [in] logPtr      Pointer to Logger instance
 *  \param [in] cfg         Pointer to the config instance
 *  \param [in] c_hash_id   Collector Hash ID
 ********************************************************************/
msgBus_kafka::msgBus_kafka(Logger *logPtr, Config *cfg, u_char *c_hash_id) {
    logger = logPtr;

    producer_buf = new unsigned char[MSGBUS_WORKING_BUF_SIZE];
    prep_buf = new char[MSGBUS_WORKING_BUF_SIZE];

    hash_toStr(c_hash_id, collector_hash);

    isConnected = false;
    conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    disableDebug();

    // TODO: Init the topic selector class

    this->cfg           = cfg;

    // Make the connection to the server
    event_callback       = NULL;
    delivery_callback    = NULL;
    producer             = NULL;
    topicSel             = NULL;

    router_ip.assign("");
    bzero(router_hash, sizeof(router_hash));

    connect();
}

/**
 * Destructor
 */
msgBus_kafka::~msgBus_kafka() {

    SELF_DEBUG("Destory msgBus Kafka instance");

    // Disconnect/term the router if not already done
    bool router_defined = false;
    for (int i=0; i < sizeof(router_hash); i++) {
        if (router_hash[i] != 0) {
            router_defined = true;
            break;
        }
    }

    if (router_defined) {
         printf("Sending term\n");
        std::map<template_cfg::TEMPLATE_TOPICS, template_cfg::Template_cfg>::iterator it = template_map->template_map.find(
                template_cfg::BMP_ROUTER);
        if (it != template_map->template_map.end()) {

            parse_bgp_lib::parseBgpLib::router_map router;
            router[parse_bgp_lib::LIB_ROUTER_HASH_ID].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_HASH_ID];
            router[parse_bgp_lib::LIB_ROUTER_HASH_ID].value.push_back(parse_bgp_lib::hash_toStr(router_hash));

            router[parse_bgp_lib::LIB_ROUTER_IP].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_IP];
            router[parse_bgp_lib::LIB_ROUTER_IP].value.push_back(router_ip);

            router[parse_bgp_lib::LIB_ROUTER_TIMESTAMP].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_TIMESTAMP];
            string ts;
            parse_bgp_lib::getTimestamp(0, 0, ts);
            router[parse_bgp_lib::LIB_ROUTER_TIMESTAMP].value.push_back(ts);

            std::ostringstream numString;
            numString << 65533;
            router[parse_bgp_lib::LIB_ROUTER_TERM_REASON_CODE].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_TERM_REASON_CODE];
            router[parse_bgp_lib::LIB_ROUTER_TERM_REASON_CODE].value.push_back(numString.str());

                router[parse_bgp_lib::LIB_ROUTER_TERM_REASON_TEXT].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_TERM_REASON_TEXT];
                router[parse_bgp_lib::LIB_ROUTER_TERM_REASON_TEXT].value.push_back("Connection closed");
            update_Router(router, ROUTER_ACTION_TERM, it->second);
        }
        printf("Done sending term\n");
    }

    sleep(2);

    delete [] producer_buf;
    delete [] prep_buf;

    peer_list.clear();

    disconnect(500);

    delete conf;
}

/**
 * Disconnect from Kafka
 */
void msgBus_kafka::disconnect(int wait_ms) {

    if (isConnected) {
        int i = 0;
        while (producer->outq_len() > 0 and i < 8) {
            LOG_INFO("Waiting for producer to finish before disconnecting: outq=%d", producer->outq_len());
            producer->poll(500);
            i++;
        }
    }

    if (topicSel != NULL) delete topicSel;

    topicSel = NULL;

    if (producer != NULL) delete producer;
    producer = NULL;

    // suggested by librdkafka to free memory
    RdKafka::wait_destroyed(wait_ms);

    if (event_callback != NULL) delete event_callback;
    event_callback = NULL;

    if (delivery_callback != NULL) delete delivery_callback;
    delivery_callback = NULL;

    isConnected = false;
}

/**
 * Connects to Kafka broker
 */
void msgBus_kafka::connect() {
    string errstr;
    string value;
    std::ostringstream rx_bytes, tx_bytes, sess_timeout, socket_timeout;
    std::ostringstream q_buf_max_msgs, q_buf_max_ms, 
		msg_send_max_retry, retry_backoff_ms;

    disconnect();

    /*
     * Configure Kafka Producer (https://kafka.apache.org/08/configuration.html)
     */
    //TODO: Add config options to change these settings

    // Disable logging of connection close/idle timeouts caused by Kafka 0.9.x (connections.max.idle.ms)
    //    See https://github.com/edenhill/librdkafka/issues/437 for more details.
    // TODO: change this when librdkafka has better handling of the idle disconnects
    value = "false";
    if (conf->set("log.connection.close", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure log.connection.close=false: %s.", errstr.c_str());
    }

    // TODO: Add config for address family - default is any
    /*value = "v4";
    if (conf->set("broker.address.family", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker.address.family: %s.", errstr.c_str());
    }*/


    // Batch message number
    value = "1000";
    if (conf->set("batch.num.messages", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure batch.num.messages for kafka: %s.", errstr.c_str());
        throw "ERROR: Failed to configure kafka batch.num.messages";
    }

    // Batch message max wait time (in ms)
    value = "500";
    if (conf->set("queue.buffering.max.ms", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure queue.buffering.max.ms for kafka: %s.", errstr.c_str());
        throw "ERROR: Failed to configure kafka queue.buffer.max.ms";
    }


    // compression
    value = cfg->compression;
    if (conf->set("compression.codec", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure %s compression for kafka: %s.", value.c_str(), errstr.c_str());
        throw "ERROR: Failed to configure kafka compression";
    }

    // broker list
    if (conf->set("metadata.broker.list", cfg->kafka_brokers, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker list for kafka: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka broker list";
    }

    // Maximum transmit byte size
    tx_bytes << cfg->tx_max_bytes;
    if (conf->set("message.max.bytes", tx_bytes.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure transmit max message size for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure transmit max message size";
    } 
 
    // Maximum receive byte size
    rx_bytes << cfg->rx_max_bytes;
    if (conf->set("receive.message.max.bytes", rx_bytes.str(), 
                             errstr) != RdKafka::Conf::CONF_OK)
    {
       LOG_ERR("Failed to configure receive max message size for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure receive max message size";
    }

    // Client group session and failure detection timeout
    sess_timeout << cfg->session_timeout;
    if (conf->set("session.timeout.ms", sess_timeout.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure session timeout for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure session timeout ";
    } 
    
    // Timeout for network requests 
    socket_timeout << cfg->socket_timeout;
    if (conf->set("socket.timeout.ms", socket_timeout.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure socket timeout for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure socket timeout ";
    } 
    
    // Maximum number of messages allowed on the producer queue 
    q_buf_max_msgs << cfg->q_buf_max_msgs;
    if (conf->set("queue.buffering.max.messages", q_buf_max_msgs.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure max messages in buffer for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure max messages in buffer ";
    } 
    
    // Maximum time, in milliseconds, for buffering data on the producer queue 
    q_buf_max_ms << cfg->q_buf_max_ms;
    if (conf->set("queue.buffering.max.ms", q_buf_max_ms.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure max time to wait for buffering for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure max time for buffering ";
    } 
    
    // How many times to retry sending a failing MessageSet
    msg_send_max_retry << cfg->msg_send_max_retry;
    if (conf->set("message.send.max.retries", msg_send_max_retry.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure max retries for sending "
               "failed message for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure max retries for sending failed message";
    } 
    
    // Backoff time in ms before retrying a message send
    retry_backoff_ms << cfg->retry_backoff_ms;
    if (conf->set("retry.backoff.ms", retry_backoff_ms.str(), 
                             errstr) != RdKafka::Conf::CONF_OK) 
    {
       LOG_ERR("Failed to configure backoff time before retrying to send"
               "failed message for kafka: %s",
                               errstr.c_str());
       throw "ERROR: Failed to configure backoff time before resending"
             " failed messages ";
    } 
    
    // Register event callback
    event_callback = new KafkaEventCallback(&isConnected, logger);
    if (conf->set("event_cb", event_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka event callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka event callback";
    }

    // Register delivery report callback
    delivery_callback = new KafkaDeliveryReportCallback();

    if (conf->set("dr_cb", delivery_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka delivery report callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka delivery report callback";
    }


    // Create producer and connect
    producer = RdKafka::Producer::create(conf, errstr);
    if (producer == NULL) {
        LOG_ERR("rtr=%s: Failed to create producer: %s", router_ip.c_str(), errstr.c_str());
        throw "ERROR: Failed to create producer";
    }

    isConnected = true;

    producer->poll(1000);

    if (not isConnected) {
        LOG_ERR("rtr=%s: Failed to connect to Kafka, will try again in a few", router_ip.c_str());
        return;

    }

    /*
     * Initialize the topic selector/handler
     */
    try {
        topicSel = new KafkaTopicSelector(logger, cfg, producer);

    } catch (char const *str) {
        LOG_ERR("rtr=%s: Failed to create one or more topics, will try again in a few: err=%s", router_ip.c_str(), str);
        isConnected = false;
        return;
    }

    producer->poll(100);
}

/**
 * produce message to Kafka
 *
 * \param [in] topic_var     Topic var to use in KafkaTopicSelector::getTopic() MSGBUS_TOPIC_VAR_*
 * \param [in] msg           message to produce
 * \param [in] msg_size      Length in bytes of the message
 * \param [in] rows          Number of rows
 * \param [in] key           Hash key
 * \param [in] peer_group    Peer group name - empty/NULL if not set or used
 * \param [in] peer_asn      Peer ASN
 */
void msgBus_kafka::produce(const char *topic_var, char *msg, size_t msg_size, int rows, string key,
                           const string *peer_group, uint32_t peer_asn) {
    size_t len;
    RdKafka::Topic *topic = NULL;

    while (isConnected == false or topicSel == NULL) {
        // Do not attempt to reconnect if this is the main process (router ip is null)
        // Changed on 10/29/15 to support docker startup delay with kafka
        /*
        if (router_ip.size() <= 0) {
            return;
        }*/

        LOG_WARN("rtr=%s: Not connected to Kafka, attempting to reconnect", router_ip.c_str());
        connect();

        sleep(1);
    }

    char headers[256];
    len = snprintf(headers, sizeof(headers), "V: %s\nC_HASH_ID: %s\nL: %lu\nR: %d\n\n",
            MSGBUS_API_VERSION, collector_hash.c_str(), msg_size, rows);

    memcpy(producer_buf, headers, len);
    memcpy(producer_buf+len, msg, msg_size);


    topic = topicSel->getTopic(topic_var, &router_group_name, peer_group, peer_asn);
    if (topic != NULL) {
        SELF_DEBUG("rtr=%s: Producing message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
                   topic->name().c_str(), key.c_str(), msg_size);

        RdKafka::ErrorCode resp = producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                                                    RdKafka::Producer::RK_MSG_COPY,
                                                    producer_buf, msg_size + len,
                                                    (const std::string *) &key, NULL);
        if (resp != RdKafka::ERR_NO_ERROR)
            LOG_ERR("rtr=%s: Failed to produce message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());
    } else {
        LOG_NOTICE("rtr=%s: failed to produce message because topic couldn't be found: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
                   topic_var, key.c_str(), msg_size);
    }

    producer->poll(0);
}

void msgBus_kafka::update_Peer(parse_bgp_lib::parseBgpLib::router_map &router,
                                        parse_bgp_lib::parseBgpLib::peer_map &peer,
                                  peer_action_code code, template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;
    size_t written = 0;

    bool skip_if_in_cache = true;
    bool add_to_cache = true;

    string action = "first";

    // Determine the action and if cache should be used or not - don't want to do too much in this switch block
    switch (code) {
        case PEER_ACTION_FIRST :
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("first");
            break;

        case PEER_ACTION_UP :
            skip_if_in_cache = false;
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("up");
            break;

        case PEER_ACTION_DOWN:
            skip_if_in_cache = false;
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("down");
            add_to_cache = false;

            if (peer_list.find(peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()) != peer_list.end())
                peer_list.erase(peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front());

            break;
    }

    // Check if we have already processed this entry, if so return
    if (skip_if_in_cache and peer_list.find(peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()) != peer_list.end()) {
        return;
    }

    // Get the hostname using DNS
    string hostname;
    resolveIp(peer[parse_bgp_lib::LIB_PEER_ADDR].value.front(), hostname);

    peer[parse_bgp_lib::LIB_PEER_NAME].name = parse_bgp_lib::parse_bgp_lib_peer_names[parse_bgp_lib::LIB_PEER_NAME];
    peer[parse_bgp_lib::LIB_PEER_NAME].value.push_back(hostname);


    // Insert/Update map entry
    if (add_to_cache) {
        if (topicSel != NULL)
            topicSel->lookupPeerGroup(peer[parse_bgp_lib::LIB_PEER_NAME].value.front(),
                                      peer[parse_bgp_lib::LIB_PEER_ADDR].value.front(),
                                      strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16),
                                      peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()]);
    }

    switch (code) {
        case PEER_ACTION_FIRST :
            written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                           *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                           *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                           peer,
                                                           router,
                                                           *(parse_bgp_lib::parseBgpLib::collector_map *)NULL,
                                                           header,
                                                           *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);

            action.assign("first");
            break;

        case PEER_ACTION_UP : {
            if ((peer.find(parse_bgp_lib::LIB_PEER_INFO_DATA) != peer.end())) {
                boost::replace_all(peer[parse_bgp_lib::LIB_PEER_INFO_DATA].value.front(), "\n", "\\n");
                boost::replace_all(peer[parse_bgp_lib::LIB_PEER_INFO_DATA].value.front(), "\t", " ");
            }

            written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                           *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                           *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                           peer,
                                                           router,
                                                           *(parse_bgp_lib::parseBgpLib::collector_map *)NULL,
                                                           header,
                                                           *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);

            skip_if_in_cache = false;
            action.assign("up");
            break;
        }
        case PEER_ACTION_DOWN: {
            written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                           *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                           *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                           peer,
                                                           router,
                                                           *(parse_bgp_lib::parseBgpLib::collector_map *)NULL,
                                                           header,
                                                           *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
            skip_if_in_cache = false;
            action.assign("down");
            add_to_cache = false;

            if (peer_list.find(peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()) != peer_list.end())
                peer_list.erase(peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front());

            break;
        }
    }

    if (written) {
        produce(MSGBUS_TOPIC_VAR_PEER, prep_buf, written, 1, peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See Msvim ./tem gBusInterface.hpp for details
 */
void msgBus_kafka::update_baseAttribute(parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                                 parse_bgp_lib::parseBgpLib::peer_map &peer,
                                                 parse_bgp_lib::parseBgpLib::router_map &router,
                                                 base_attr_action_code code, template_cfg::Template_cfg &template_container) {

    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                          *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                          attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL,
                                                          header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_BASE_ATTRIBUTE, prep_buf, written, 1, peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}


/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_unicastPrefix(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                                 parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                                 parse_bgp_lib::parseBgpLib::peer_map &peer,
                                                 parse_bgp_lib::parseBgpLib::router_map &router,
                                                 unicast_prefix_action_code code,
                                                 template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case UNICAST_PREFIX_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case UNICAST_PREFIX_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_UNICAST_PREFIX, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_L3Vpn(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                                 parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                                 parse_bgp_lib::parseBgpLib::peer_map &peer,
                                                 parse_bgp_lib::parseBgpLib::router_map &router,
                                                 vpn_action_code code,
                                                 template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case VPN_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case VPN_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

   size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                         *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_L3VPN, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_eVpn(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                         parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                         parse_bgp_lib::parseBgpLib::peer_map &peer,
                                         parse_bgp_lib::parseBgpLib::router_map &router,
                                         vpn_action_code code,
                                         template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case VPN_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case VPN_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_EVPN, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_LsNode(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                          parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                          parse_bgp_lib::parseBgpLib::peer_map &peer,
                                          parse_bgp_lib::parseBgpLib::router_map &router,
                                          ls_action_code code,
                                          template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;

    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case LS_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case LS_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID];
    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].value.push_back((attrs.find(parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6) != attrs.end()) ?
                                                                      map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].value).c_str() : map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].value).c_str());

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_LS_NODE, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
* Abstract method Implementation - See MsgBusInterface.hpp for details
*/
void msgBus_kafka::update_LsLink(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                          parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                          parse_bgp_lib::parseBgpLib::peer_map &peer,
                                          parse_bgp_lib::parseBgpLib::router_map &router,
                                          ls_action_code code,
                                          template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;

    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case LS_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case LS_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID];
    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].value.push_back((attrs.find(parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6) != attrs.end()) ?
                                                                      map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].value).c_str() : map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].value).c_str());

    attrs[parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID];
    attrs[parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID].value.push_back((attrs.find(parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6) != attrs.end()) ?
                                                                      map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV6].value).c_str() : map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_REMOTE_ROUTER_ID_IPV4].value).c_str());


    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_LS_LINK, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_LsPrefix(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> &rib_list,
                                          parse_bgp_lib::parseBgpLib::attr_map &attrs,
                                          parse_bgp_lib::parseBgpLib::peer_map &peer,
                                          parse_bgp_lib::parseBgpLib::router_map &router,
                                          ls_action_code code,
                                          template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;

    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case LS_ACTION_ADD:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");
            break;
        case LS_ACTION_DEL:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("del");
            break;
    }

    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].name = parse_bgp_lib::parse_bgp_lib_attr_names[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID];
    attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID].value.push_back((attrs.find(parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6) != attrs.end()) ?
                                                                      map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV6].value).c_str() : map_string(attrs[parse_bgp_lib::LIB_ATTR_LS_LOCAL_ROUTER_ID_IPV4].value).c_str());

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE, rib_list, attrs, peer, router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);

    if (written) {
        produce(MSGBUS_TOPIC_VAR_LS_PREFIX, prep_buf, written, rib_list.size(), peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_Router(parse_bgp_lib::parseBgpLib::router_map &router,
                                          router_action_code code, template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    bool skip_if_defined = true;

    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    switch (code) {
        case ROUTER_ACTION_FIRST :
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("first");
            break;

        case ROUTER_ACTION_INIT :
            skip_if_defined = false;
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("init");
            break;

        case ROUTER_ACTION_TERM:
            skip_if_defined = false;
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("term");
            bzero(router_hash, sizeof(router_hash));
            break;
    }

    // Check if we have already processed this entry, if so return
    if (skip_if_defined) {
        for (int i=0; i < sizeof(router_hash); i++) {
            if (router_hash[i] != 0)
                return;
        }
    }

    if (code != ROUTER_ACTION_TERM) {
        memcpy(router_hash, router[parse_bgp_lib::LIB_ROUTER_HASH_ID].value.front().c_str(), sizeof(router_hash));
    }

    router_ip.assign(router[parse_bgp_lib::LIB_ROUTER_IP].value.front());

    parse_bgp_lib::parseBgpLib::router_map::iterator it = router.find(parse_bgp_lib::LIB_ROUTER_DESCR);
    if (it != router.end()) {
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_DESCR].value.front(), "\n", "\\n");
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_DESCR].value.front(), "\t", " ");
    }

    it = router.find(parse_bgp_lib::LIB_ROUTER_INITIATE_DATA);
    if (it != router.end()) {
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_INITIATE_DATA].value.front(), "\n", "\\n");
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_INITIATE_DATA].value.front(), "\t", " ");
    }

    it = router.find(parse_bgp_lib::LIB_ROUTER_TERM_DATA);
    if (it != router.end()) {
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_TERM_DATA].value.front(), "\n", "\\n");
        boost::replace_all(router[parse_bgp_lib::LIB_ROUTER_TERM_DATA].value.front(), "\t", " ");
    }

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                          *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                          *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                          *(parse_bgp_lib::parseBgpLib::peer_map *)NULL,
                                                          router,
                                                          *(parse_bgp_lib::parseBgpLib::collector_map *)NULL, header,
                                                          *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    // Get the hostname
    string hostname = "";
    it = router.find(parse_bgp_lib::LIB_ROUTER_IP);
    if ((it != router.end()) and router[parse_bgp_lib::LIB_ROUTER_IP].value.size()) {
        resolveIp(router[parse_bgp_lib::LIB_ROUTER_IP].value.front(), hostname);
    }

    router[parse_bgp_lib::LIB_ROUTER_NAME].name = parse_bgp_lib::parse_bgp_lib_router_names[parse_bgp_lib::LIB_ROUTER_NAME];
    if (router[parse_bgp_lib::LIB_ROUTER_NAME].value.size())
        router[parse_bgp_lib::LIB_ROUTER_NAME].value.front().assign(hostname);
    else
        router[parse_bgp_lib::LIB_ROUTER_NAME].value.push_back(hostname);

    if (topicSel != NULL) {
        topicSel->lookupRouterGroup(router[parse_bgp_lib::LIB_ROUTER_NAME].value.front(),
                                    router[parse_bgp_lib::LIB_ROUTER_IP].value.front(), router_group_name);
    }


    if (written) {
        produce(MSGBUS_TOPIC_VAR_ROUTER, prep_buf, written, 1, router[parse_bgp_lib::LIB_ROUTER_HASH_ID].value.front(),
                NULL, 0);
    }
}

void msgBus_kafka::update_Collector(parse_bgp_lib::parseBgpLib::collector_map &collector,
                               collector_action_code action_code, template_cfg::Template_cfg &template_container) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;
    parse_bgp_lib::parseBgpLib::header_map header;

    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];

    switch (action_code) {
        case COLLECTOR_ACTION_STARTED:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("started");
            break;
        case COLLECTOR_ACTION_CHANGE:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("change");
            break;
        case COLLECTOR_ACTION_HEARTBEAT:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("heartbeat");
            break;
        case COLLECTOR_ACTION_STOPPED:
            header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("stopped");
            break;
    }

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                          *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                          *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                          *(parse_bgp_lib::parseBgpLib::peer_map *)NULL,
                                                          *(parse_bgp_lib::parseBgpLib::router_map *)NULL, collector,
                                                          header,  *(parse_bgp_lib::parseBgpLib::stat_map *)NULL);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_COLLECTOR, prep_buf, written, 1, collector[parse_bgp_lib::LIB_COLLECTOR_HASH_ID].value.front(),
                    NULL, 0);
    }
}

void msgBus_kafka::add_StatReport(parse_bgp_lib::parseBgpLib::peer_map &peer,
                             parse_bgp_lib::parseBgpLib::router_map &router,
                             parse_bgp_lib::parseBgpLib::stat_map stats, template_cfg::Template_cfg &template_container) {
    prep_buf[0] = 0;

    parse_bgp_lib::parseBgpLib::header_map header;
    header[parse_bgp_lib::LIB_HEADER_ACTION].name = parse_bgp_lib::parse_bgp_lib_header_names[parse_bgp_lib::LIB_HEADER_ACTION];
    header[parse_bgp_lib::LIB_HEADER_ACTION].value.push_back("add");

    size_t written = template_container.execute_container(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                                                  *(std::vector<parse_bgp_lib::parseBgpLib::parse_bgp_lib_nlri> *)NULL,
                                                  *(parse_bgp_lib::parseBgpLib::attr_map *)NULL,
                                                  peer,
                                                  router,
                                                  *(parse_bgp_lib::parseBgpLib::collector_map *)NULL,
                                                  header,
                                                  stats);
    if (written) {
        produce(MSGBUS_TOPIC_VAR_BMP_STAT, prep_buf, written, 1, peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front(),
                &peer_list[peer[parse_bgp_lib::LIB_PEER_HASH_ID].value.front()], strtoll(peer[parse_bgp_lib::LIB_PEER_AS].value.front().c_str(), NULL, 16));
    }
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 *
 * TODO: Consolidate this to single produce method
 */
void msgBus_kafka::send_bmp_raw(u_char *r_hash, obj_bgp_peer &peer, u_char *data, size_t data_len) {
    string r_hash_str;
    string p_hash_str;
    RdKafka::Topic *topic = NULL;

    hash_toStr(peer.hash_id, p_hash_str);
    hash_toStr(r_hash, r_hash_str);

    if (data_len == 0)
        return;

    while (isConnected == false) {
        LOG_WARN("rtr=%s: Not connected to Kafka, attempting to reconnect", router_ip.c_str());
        connect();

        sleep(2);
    }

    char headers[256];
    size_t hdr_len = snprintf(headers, sizeof(headers), "V: %s\nC_HASH_ID: %s\nR_HASH: %s\nR_IP: %s\nL: %lu\n\n",
             MSGBUS_API_VERSION, collector_hash.c_str(), r_hash_str.c_str(), router_ip.c_str(), data_len);

    memcpy(producer_buf, headers, hdr_len);
    memcpy(producer_buf+hdr_len, data, data_len);

    topic = topicSel->getTopic(MSGBUS_TOPIC_VAR_BMP_RAW, &router_group_name, &peer_list[p_hash_str], peer.peer_as);
    if (topic != NULL) {
        SELF_DEBUG("rtr=%s: Producing bmp raw message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
                   topic->name().c_str(), r_hash_str.c_str(), data_len);

        RdKafka::ErrorCode resp = producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                                                    RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                                                    producer_buf, data_len + hdr_len,
                                                    (const std::string *)&r_hash_str, NULL);

        if (resp != RdKafka::ERR_NO_ERROR)
            LOG_ERR("rtr=%s: Failed to produce bmp raw message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());
    }
    else {
        SELF_DEBUG("rtr=%s: failed to produce bmp raw message because topic couldn't be found: topic=%s key=%s, msg size = %lu",
                   router_ip.c_str(), MSGBUS_TOPIC_VAR_BMP_RAW, r_hash_str.c_str(), data_len);
    }

    producer->poll(0);
}

/**
* \brief Method to resolve the IP address to a hostname
*
*  \param [in]   name      String name (ip address)
*  \param [out]  hostname  String reference for hostname
*
*  \returns true if error, false if no error
*/
bool msgBus_kafka::resolveIp(string name, string &hostname) {
    addrinfo *ai;
    char host[255];

    if (!getaddrinfo(name.c_str(), NULL, NULL, &ai)) {

        if (!getnameinfo(ai->ai_addr,ai->ai_addrlen, host, sizeof(host), NULL, 0, NI_NAMEREQD)) {
            hostname.assign(host);
            LOG_INFO("resolve: %s to %s", name.c_str(), hostname.c_str());
        }

        freeaddrinfo(ai);
        return false;
    }

    return true;
}

/*
 * Enable/disable debugs
 */
void msgBus_kafka::enableDebug() {
    string value = "all";
    string errstr;

    if (conf->set("debug", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to enable debug on kafka producer confg: %s", errstr.c_str());
    }

    debug = true;

}
void msgBus_kafka::disableDebug() {
    string errstr;
    string value = "";

    if (conf)
        conf->set("debug", value, errstr);

    debug = false;
}
