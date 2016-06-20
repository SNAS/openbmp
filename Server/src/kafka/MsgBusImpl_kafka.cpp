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
#include <algorithm>
#include <string>

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
 * \brief This function will initialize and connect to MySQL.
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

    router_seq          = 0L;
    collector_seq       = 0L;
    peer_seq            = 0L;
    base_attr_seq       = 0L;
    unicast_prefix_seq  = 0L;
    ls_node_seq         = 0L;
    ls_link_seq         = 0L;
    ls_prefix_seq       = 0L;
    bmp_stat_seq        = 0L;

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
    MsgBusInterface::obj_router r_object;
    bool router_defined = false;
    for (int i=0; i < sizeof(router_hash); i++) {
        if (router_hash[i] != 0) {
            router_defined = true;
            break;
        }
    }

    if (router_defined) {
        bzero(&r_object, sizeof(r_object));
        memcpy(r_object.hash_id, router_hash, sizeof(r_object.hash_id));
        snprintf((char *)r_object.ip_addr, sizeof(r_object.ip_addr), "%s", router_ip.c_str());
        r_object.term_reason_code = 65533;
        snprintf(r_object.term_reason_text, sizeof(r_object.term_reason_text),
                 "Connection closed");

        printf("Sending term\n");
        update_Router(r_object, msgBus_kafka::ROUTER_ACTION_TERM);
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

    SELF_DEBUG("rtr=%s: Producing message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
               topic_var, key.c_str(), msg_size);

    char headers[256];
    len = snprintf(headers, sizeof(headers), "V: %s\nC_HASH_ID: %s\nL: %lu\nR: %d\n\n",
            MSGBUS_API_VERSION, collector_hash.c_str(), msg_size, rows);

    memcpy(producer_buf, headers, len);
    memcpy(producer_buf+len, msg, msg_size);


    topic = topicSel->getTopic(topic_var, &router_group_name, peer_group, peer_asn);
    if (topic != NULL) {
        RdKafka::ErrorCode resp = producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                                                    RdKafka::Producer::RK_MSG_COPY,
                                                    producer_buf, msg_size + len,
                                                    (const std::string *) &key, NULL);
        if (resp != RdKafka::ERR_NO_ERROR)
            LOG_ERR("rtr=%s: Failed to produce message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());
    }

    producer->poll(0);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_Collector(obj_collector &c_object, collector_action_code action_code) {
    char buf[4096]; // Misc working buffer

    string ts;
    getTimestamp(c_object.timestamp_secs, c_object.timestamp_us, ts);

    char *action = const_cast<char *>("change");

    switch (action_code) {
        case COLLECTOR_ACTION_STARTED:
            action = const_cast<char *>("started");
            break;
        case COLLECTOR_ACTION_CHANGE:
            action = const_cast<char *>("change");
            break;
        case COLLECTOR_ACTION_HEARTBEAT:
            action = const_cast<char *>("heartbeat");
            break;
        case COLLECTOR_ACTION_STOPPED:
            action = const_cast<char *>("stopped");
            break;
    }

    snprintf(buf, sizeof(buf),
             "%s\t%" PRIu64 "\t%s\t%s\t%s\t%u\t%s\n",
             action, collector_seq, c_object.admin_id, collector_hash.c_str(),
             c_object.routers, c_object.router_count, ts.c_str());

    produce(MSGBUS_TOPIC_VAR_COLLECTOR, buf, strlen(buf), 1, collector_hash, NULL, 0);

    collector_seq++;
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_Router(obj_router &r_object, router_action_code code) {
    char buf[4096]; // Misc working buffer

    // Convert binary hash to string
    string r_hash_str;
    hash_toStr(r_object.hash_id, r_hash_str);

    bool skip_if_defined = true;

    string action = "first";

    switch (code) {
        case ROUTER_ACTION_FIRST :
            action.assign("first");
            break;

        case ROUTER_ACTION_INIT :
            skip_if_defined = false;
            action.assign("init");
            break;

        case ROUTER_ACTION_TERM:
            skip_if_defined = false;
            action.assign("term");
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

    if (code != ROUTER_ACTION_TERM)
        memcpy(router_hash, r_object.hash_id, sizeof(router_hash));

    router_ip.assign((char *)r_object.ip_addr);                     // Update router IP for logging

    string descr((char *)r_object.descr);
    boost::replace_all(descr, "\n", "\\n");
    boost::replace_all(descr, "\t", " ");

    string initData(r_object.initiate_data);
    boost::replace_all(initData, "\n", "\\n");
    boost::replace_all(initData, "\t", " ");

    string termData(r_object.term_data);
    boost::replace_all(termData, "\n", "\\n");
    boost::replace_all(termData, "\t", " ");

    string ts;
    getTimestamp(r_object.timestamp_secs, r_object.timestamp_us, ts);

    // Get the hostname
    string hostname = "";
    if (strlen((char *)r_object.name) <= 0) {
        resolveIp((char *) r_object.ip_addr, hostname);
        snprintf((char *)r_object.name, sizeof(r_object.name)-1, "%s", hostname.c_str());
    }

    if (topicSel != NULL)
        topicSel->lookupRouterGroup((char *)r_object.name, (char *)r_object.ip_addr, router_group_name);

    size_t size = snprintf(buf, sizeof(buf),
             "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%" PRIu16 "\t%s\t%s\t%s\t%s\n", action.c_str(),
             router_seq, r_object.name, r_hash_str.c_str(), r_object.ip_addr, descr.c_str(),
             r_object.term_reason_code, r_object.term_reason_text,
             initData.c_str(), termData.c_str(), ts.c_str());

    produce(MSGBUS_TOPIC_VAR_ROUTER, buf, size, 1, r_hash_str, NULL, 0);

    router_seq++;
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_Peer(obj_bgp_peer &peer, obj_peer_up_event *up, obj_peer_down_event *down, peer_action_code code) {

    char buf[4096]; // Misc working buffer

    string r_hash_str;
    hash_toStr(peer.router_hash_id, r_hash_str);

    // Generate the hash
    MD5 hash;

    hash.update((unsigned char *) peer.peer_addr,
                strlen(peer.peer_addr));
    hash.update((unsigned char *) peer.peer_rd, strlen(peer.peer_rd));
    hash.update((unsigned char *)r_hash_str.c_str(), r_hash_str.length());

    /* TODO: Uncomment once this is fixed in XR
     * Disable hashing the bgp peer ID since XR has an issue where it sends 0.0.0.0 on subsequent PEER_UP's
     *    This will be fixed in XR, but for now we can disable hashing on it.
     *
    hash.update((unsigned char *) p_object.peer_bgp_id,
            strlen(p_object.peer_bgp_id));
    */

    hash.finalize();

    // Save the hash
    unsigned char *hash_raw = hash.raw_digest();
    memcpy(peer.hash_id, hash_raw, 16);
    delete[] hash_raw;

    // Convert binary hash to string
    string p_hash_str;
    hash_toStr(peer.hash_id, p_hash_str);

    bool skip_if_in_cache = true;
    bool add_to_cache = true;

    string action = "first";

    // Determine the action and if cache should be used or not - don't want to do too much in this switch block
    switch (code) {
        case PEER_ACTION_FIRST :
            action.assign("first");
            break;

        case PEER_ACTION_UP :
            skip_if_in_cache = false;
            action.assign("up");
            break;

        case PEER_ACTION_DOWN:
            skip_if_in_cache = false;
            action.assign("down");
            add_to_cache = false;

            if (peer_list.find(p_hash_str) != peer_list.end())
                peer_list.erase(p_hash_str);

            break;
    }

    // Check if we have already processed this entry, if so return
    if (skip_if_in_cache and peer_list.find(p_hash_str) != peer_list.end()) {
        return;
    }

    // Get the hostname using DNS
    string hostname;
    resolveIp(peer.peer_addr, hostname);

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    // Insert/Update map entry
    if (add_to_cache) {
        if (topicSel != NULL)
            topicSel->lookupPeerGroup(hostname, peer.peer_addr, peer.peer_as, peer_list[p_hash_str]);
    }

    switch (code) {
        case PEER_ACTION_FIRST :
            snprintf(buf, sizeof(buf),
                     "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t%d\t%d\t%d\n",
                     action.c_str(), peer_seq, p_hash_str.c_str(), r_hash_str.c_str(), hostname.c_str(),
                     peer.peer_bgp_id,router_ip.c_str(), ts.c_str(), peer.peer_as, peer.peer_addr,peer.peer_rd,
                     peer.isL3VPN, peer.isPrePolicy, peer.isIPv4);
            action.assign("first");
            break;

        case PEER_ACTION_UP : {
            if (up == NULL)
                return;

            string infoData(up->info_data);
            if (up->info_data[0] != 0) {
                boost::replace_all(infoData, "\n", "\\n");
                boost::replace_all(infoData, "\t", " ");
            }

            snprintf(buf, sizeof(buf),
                     "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%" PRIu16 "\t%" PRIu32 "\t%s\t%" PRIu16
                             "\t%s\t%s\t%s\t%s\t%" PRIu16 "\t%" PRIu16 "\t\t\t\t\t%d\t%d\t%d\n",
                     action.c_str(), peer_seq, p_hash_str.c_str(), r_hash_str.c_str(), hostname.c_str(),
                     peer.peer_bgp_id, router_ip.c_str(), ts.c_str(), peer.peer_as, peer.peer_addr, peer.peer_rd,

                    /* Peer UP specific fields */
                     up->remote_port, up->local_asn, up->local_ip, up->local_port, up->local_bgp_id, infoData.c_str(), up->sent_cap,
                     up->recv_cap, up->remote_hold_time, up->local_hold_time,

            peer.isL3VPN, peer.isPrePolicy, peer.isIPv4);

            skip_if_in_cache = false;
            action.assign("up");
            break;
        }
        case PEER_ACTION_DOWN: {
            if (down == NULL)
                return;

            snprintf(buf, sizeof(buf),
                     "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t\t\t\t\t\t\t\t\t\t\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
                     action.c_str(), peer_seq, p_hash_str.c_str(), r_hash_str.c_str(), hostname.c_str(),
                     peer.peer_bgp_id, router_ip.c_str(), ts.c_str(), peer.peer_as, peer.peer_addr, peer.peer_rd,

                     /* Peer DOWN specific fields */
                     down->bmp_reason, down->bgp_err_code, down->bgp_err_subcode, down->error_text,

                     peer.isL3VPN, peer.isPrePolicy, peer.isIPv4);

            skip_if_in_cache = false;
            action.assign("down");
            add_to_cache = false;

            if (peer_list.find(p_hash_str) != peer_list.end())
                peer_list.erase(p_hash_str);

            break;
        }
    }

    produce(MSGBUS_TOPIC_VAR_PEER, buf, strlen(buf), 1, p_hash_str, &peer_list[p_hash_str], peer.peer_as);

    peer_seq++;
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_baseAttribute(obj_bgp_peer &peer, obj_path_attr &attr, base_attr_action_code code) {

    prep_buf[0] = 0;
    size_t  buf_len;                    // size of the message in buf

    string path_hash_str;
    string p_hash_str;
    string r_hash_str;
    hash_toStr(peer.hash_id, p_hash_str);
    hash_toStr(peer.router_hash_id, r_hash_str);


    // Generate the hash
    MD5 hash;

    //hash.update(path_object.peer_hash_id, HASH_SIZE);
    hash.update((unsigned char *) attr.as_path.c_str(), attr.as_path.length());
    hash.update((unsigned char *) attr.next_hop,
                strlen(attr.next_hop));
    hash.update((unsigned char *) attr.aggregator,
                strlen(attr.aggregator));
    hash.update((unsigned char *) attr.origin,
                strlen(attr.origin));
    hash.update((unsigned char *) &attr.med, sizeof(attr.med));
    hash.update((unsigned char *) &attr.local_pref,
                sizeof(attr.local_pref));

    hash.update((unsigned char *) attr.community_list.c_str(), attr.community_list.length());
    hash.update((unsigned char *) attr.ext_community_list.c_str(), attr.ext_community_list.length());
    hash.update((unsigned char *) p_hash_str.c_str(), p_hash_str.length());

    hash.finalize();

    // Save the hash
    unsigned char *hash_raw = hash.raw_digest();
    memcpy(attr.hash_id, hash_raw, 16);
    delete[] hash_raw;

    hash_toStr(attr.hash_id, path_hash_str);

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    buf_len =
            snprintf(prep_buf, MSGBUS_WORKING_BUF_SIZE,
                     "add\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIu16 "\t%" PRIu32
                             "\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%s\t%s\t%d\t%d\t%s\n",
                     base_attr_seq, path_hash_str.c_str(), r_hash_str.c_str(), router_ip.c_str(), p_hash_str.c_str(),
                     peer.peer_addr,peer.peer_as, ts.c_str(),
                     attr.origin, attr.as_path.c_str(), attr.as_path_count, attr.origin_as, attr.next_hop, attr.med,
                     attr.local_pref, attr.aggregator, attr.community_list.c_str(), attr.ext_community_list.c_str(), attr.cluster_list.c_str(),
                     attr.atomic_agg, attr.nexthop_isIPv4, attr.originator_id);

    produce(MSGBUS_TOPIC_VAR_BASE_ATTRIBUTE, prep_buf, buf_len, 1, p_hash_str, &peer_list[p_hash_str], peer.peer_as);

    ++base_attr_seq;
}


/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_unicastPrefix(obj_bgp_peer &peer, std::vector<obj_rib> &rib,
                                        obj_path_attr *attr, unicast_prefix_action_code code) {
    //bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);
    prep_buf[0] = 0;

    char    buf2[80000];                         // Second working buffer
    size_t  buf_len = 0;                         // query buffer length

    string rib_hash_str;
    string path_hash_str;
    string p_hash_str;
    string r_hash_str;

    hash_toStr(peer.router_hash_id, r_hash_str);

    if (attr != NULL)
        hash_toStr(attr->hash_id, path_hash_str);

    hash_toStr(peer.hash_id, p_hash_str);

    string action = "add";
    switch (code) {
        case UNICAST_PREFIX_ACTION_ADD:
            action = "add";
            break;
        case UNICAST_PREFIX_ACTION_DEL:
            action = "del";
            break;
    }

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    // Loop through the vector array of rib entries
    for (size_t i = 0; i < rib.size(); i++) {

        // Generate the hash
        MD5 hash;

        hash.update((unsigned char *) rib[i].prefix, strlen(rib[i].prefix));
        hash.update(&rib[i].prefix_len, sizeof(rib[i].prefix_len));
        hash.update((unsigned char *) p_hash_str.c_str(), p_hash_str.length());

        // Add path ID to hash only if exists
        if (rib[i].path_id > 0)
            hash.update((unsigned char *)&rib[i].path_id, sizeof(rib[i].path_id));

        /*
         * Add constant "1" to hash if labels are present
         *      Withdrawn and updated NLRI's do not carry the original label, therefore we cannot
         *      hash on the label string.  Instead, we has on a constant value of 1.
         */
        if (rib[i].labels[0] != 0) {
            buf2[0] = 1;
            hash.update((unsigned char *) buf2, 1);
            buf2[0] = 0;
        }

        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(rib[i].hash_id, hash_raw, 16);
        delete[] hash_raw;

        // Build the query
        hash_toStr(rib[i].hash_id, rib_hash_str);

        switch (code) {

            case UNICAST_PREFIX_ACTION_ADD:
                if (attr == NULL)
                    return;

                buf_len += snprintf(buf2, sizeof(buf2),
                                    "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%d\t%d\t%s\t%s\t%" PRIu16
                                            "\t%" PRIu32 "\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%" PRIu32 "\t%s\n",
                                    action.c_str(), unicast_prefix_seq, rib_hash_str.c_str(), r_hash_str.c_str(),
                                    router_ip.c_str(),path_hash_str.c_str(), p_hash_str.c_str(),
                                    peer.peer_addr, peer.peer_as, ts.c_str(), rib[i].prefix, rib[i].prefix_len,
                                    rib[i].isIPv4, attr->origin,
                                    attr->as_path.c_str(), attr->as_path_count, attr->origin_as, attr->next_hop, attr->med, attr->local_pref,
                                    attr->aggregator,
                                    attr->community_list.c_str(), attr->ext_community_list.c_str(), attr->cluster_list.c_str(),
                                    attr->atomic_agg, attr->nexthop_isIPv4,
                                    attr->originator_id, rib[i].path_id, rib[i].labels);
                break;

            case UNICAST_PREFIX_ACTION_DEL:
                buf_len += snprintf(buf2, sizeof(buf2),
                                    "%s\t%" PRIu64 "\t%s\t%s\t%s\t\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%d\t%d\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t%" PRIu32 "\t%s\n",
                                    action.c_str(), unicast_prefix_seq, rib_hash_str.c_str(), r_hash_str.c_str(),
                                    router_ip.c_str(), p_hash_str.c_str(),
                                    peer.peer_addr, peer.peer_as, ts.c_str(), rib[i].prefix, rib[i].prefix_len,
                                    rib[i].isIPv4, rib[i].path_id, rib[i].labels);
                break;
        }

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++unicast_prefix_seq;
    }


    produce(MSGBUS_TOPIC_VAR_UNICAST_PREFIX, prep_buf, strlen(prep_buf), rib.size(), p_hash_str,
            &peer_list[p_hash_str], peer.peer_as);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::add_StatReport(obj_bgp_peer &peer, obj_stats_report &stats) {
    char buf[4096];                 // Misc working buffer

    // Build the query
    string p_hash_str;
    string r_hash_str;
    hash_toStr(peer.hash_id, p_hash_str);
    hash_toStr(peer.router_hash_id, r_hash_str);

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    snprintf(buf, sizeof(buf),
             "add\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32
                     "\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu64 "\t%" PRIu64 "\n",
             bmp_stat_seq, r_hash_str.c_str(), router_ip.c_str(),p_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
             stats.prefixes_rej,stats.known_dup_prefixes, stats.known_dup_withdraws, stats.invalid_cluster_list,
             stats.invalid_as_path_loop, stats.invalid_originator_id, stats.invalid_as_confed_loop,
             stats.routes_adj_rib_in, stats.routes_loc_rib);


    produce(MSGBUS_TOPIC_VAR_BMP_STAT, buf, strlen(buf), 1, p_hash_str, &peer_list[p_hash_str], peer.peer_as);
    ++bmp_stat_seq;
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_LsNode(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_node> &nodes,
                                  ls_action_code code) {
    bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);

    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length
    int     i;

    string hash_str;
    string r_hash_str;
    string path_hash_str;
    string peer_hash_str;

    hash_toStr(peer.router_hash_id, r_hash_str);
    hash_toStr(attr.hash_id, path_hash_str);
    hash_toStr(peer.hash_id, peer_hash_str);

    string action = "add";
    switch (code) {
        case LS_ACTION_ADD:
            action = "add";
            break;
        case LS_ACTION_DEL:
            action = "del";
            break;
    }

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    char igp_router_id[46];
    char router_id[46];
    char ospf_area_id[16] = {0};
    char isis_area_id[32] = {0};
    char dr[16];

    // Loop through the vector array of entries
    int rows = 0;
    for (std::list<MsgBusInterface::obj_ls_node>::iterator it = nodes.begin();
            it != nodes.end(); it++) {
        ++rows;
        MsgBusInterface::obj_ls_node &node = (*it);

        hash_toStr(node.hash_id, hash_str);

        if (node.isIPv4) {
            inet_ntop(PF_INET, node.router_id, router_id, sizeof(router_id));
        } else {
            inet_ntop(PF_INET6, node.router_id, router_id, sizeof(router_id));
        }

        if (!strcmp(node.protocol, "OSPFv3") or !strcmp(node.protocol, "OSPFv2") ) {
            bzero(isis_area_id, sizeof(isis_area_id));
            bzero(igp_router_id, sizeof(igp_router_id));

            // The first 4 octets are the router ID and the second 4 are the DR or ZERO if no DR
            inet_ntop(PF_INET, node.igp_router_id, igp_router_id, sizeof(igp_router_id));

            string hostname;
            resolveIp(igp_router_id, hostname);
            strncpy(node.name, hostname.c_str(), sizeof(node.name));

            if ((uint32_t) *(node.igp_router_id+4) != 0) {
                inet_ntop(PF_INET, node.igp_router_id+4, dr, sizeof(dr));
                strncat(igp_router_id, "[", 1);
                strncat(igp_router_id, dr, sizeof(dr));
                strncat(igp_router_id, "]", 1);
                LOG_INFO("igp router id includes DR: %s %s", igp_router_id, dr);
            }

            inet_ntop(PF_INET, node.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));

        } else {
            bzero(ospf_area_id, sizeof(ospf_area_id));

            snprintf(igp_router_id, sizeof(igp_router_id),
                     "%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX",
                     node.igp_router_id[0], node.igp_router_id[1], node.igp_router_id[2], node.igp_router_id[3],
                     node.igp_router_id[4], node.igp_router_id[5], node.igp_router_id[6], node.igp_router_id[7]);

            if (node.isis_area_id[8] <= sizeof(node.isis_area_id))
                for (i=0; i < node.isis_area_id[8]; i++) {
                    snprintf(buf2, sizeof(buf2), "%02hhX", node.isis_area_id[i]);
                    strcat(isis_area_id, buf2);

                    if (i == 0)
                        strcat(isis_area_id, ".");
                }
        }

        buf_len += snprintf(buf2, sizeof(buf2),
                        "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIx64 "\t%" PRIx32 "\t%s"
                                "\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\n",
                        action.c_str(),ls_node_seq, hash_str.c_str(),path_hash_str.c_str(), r_hash_str.c_str(),
                        router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                        igp_router_id, router_id, node.id, node.bgp_ls_id,node.mt_id, ospf_area_id, isis_area_id,
                        node.protocol, node.flags, attr.as_path.c_str(), attr.local_pref, attr.med, attr.next_hop, node.name);

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_node_seq;
    }


    produce(MSGBUS_TOPIC_VAR_LS_NODE, prep_buf, buf_len, rows, peer_hash_str, &peer_list[peer_hash_str], peer.peer_as);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_LsLink(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_link> &links,
                                 ls_action_code code) {
    bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);

    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length
    int     i;

    string hash_str;
    string r_hash_str;
    string path_hash_str;
    string peer_hash_str;

    hash_toStr(peer.router_hash_id, r_hash_str);
    hash_toStr(attr.hash_id, path_hash_str);
    hash_toStr(peer.hash_id, peer_hash_str);

    string action = "add";
    switch (code) {
        case LS_ACTION_ADD:
            action = "add";
            break;
        case LS_ACTION_DEL:
            action = "del";
            break;
    }

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    string local_node_hash_id;
    string remote_node_hash_id;

    char intf_ip[46];
    char nei_ip[46];
    char igp_router_id[46];
    char router_id[46];
    char ospf_area_id[17] = {0};
    char isis_area_id[33] = {0};
    char dr[16];

    // Loop through the vector array of entries
    int rows = 0;
    for (std::list<MsgBusInterface::obj_ls_link>::iterator it = links.begin();
         it != links.end(); it++) {

        ++rows;
        MsgBusInterface::obj_ls_link &link = (*it);

        MD5 hash;

        hash.update(link.intf_addr, sizeof(link.intf_addr));
        hash.update(link.nei_addr, sizeof(link.nei_addr));
        hash.update((unsigned char *)&link.id, sizeof(link.id));
        hash.update(link.local_node_hash_id, sizeof(link.local_node_hash_id));
        hash.update(link.remote_node_hash_id, sizeof(link.remote_node_hash_id));
        hash.update((unsigned char *)&link.local_link_id, sizeof(link.local_link_id));
        hash.update((unsigned char *)&link.remote_link_id, sizeof(link.remote_link_id));
        hash.update((unsigned char *)peer_hash_str.c_str(), peer_hash_str.length());
        hash.update((unsigned char *)&link.mt_id, sizeof(link.mt_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(link.hash_id, hash_bin, 16);
        delete[] hash_bin;

        hash_toStr(link.hash_id, hash_str);
        hash_toStr(link.local_node_hash_id, local_node_hash_id);
        hash_toStr(link.remote_node_hash_id, remote_node_hash_id);

        if (link.isIPv4) {
            inet_ntop(PF_INET, link.intf_addr, intf_ip, sizeof(intf_ip));
            inet_ntop(PF_INET, link.nei_addr, nei_ip, sizeof(nei_ip));
            inet_ntop(PF_INET, link.router_id, router_id, sizeof(router_id));
        } else {
            inet_ntop(PF_INET6, link.intf_addr, intf_ip, sizeof(intf_ip));
            inet_ntop(PF_INET6, link.nei_addr, nei_ip, sizeof(nei_ip));
            inet_ntop(PF_INET6, link.router_id, router_id, sizeof(router_id));
        }

        if (!strcmp(link.protocol, "OSPFv3") or !strcmp(link.protocol, "OSPFv2") ) {
            bzero(isis_area_id, sizeof(isis_area_id));

            inet_ntop(PF_INET, link.igp_router_id, igp_router_id, sizeof(igp_router_id));

            if ((uint32_t) *(link.igp_router_id+4) != 0) {
                inet_ntop(PF_INET, link.igp_router_id+4, dr, sizeof(dr));
                strncat(igp_router_id, "[", 1);
                strncat(igp_router_id, dr, sizeof(dr));
                strncat(igp_router_id, "]", 1);
            }

            inet_ntop(PF_INET, link.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));
        } else {
            bzero(ospf_area_id, sizeof(ospf_area_id));

            snprintf(igp_router_id, sizeof(igp_router_id),
                     "%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX",
                     link.igp_router_id[0], link.igp_router_id[1], link.igp_router_id[2], link.igp_router_id[3],
                     link.igp_router_id[4], link.igp_router_id[5], link.igp_router_id[6], link.igp_router_id[7]);

            if (link.isis_area_id[8] <= sizeof(link.isis_area_id)) {
                for (i = 0; i < link.isis_area_id[8]; i++) {
                    snprintf(buf2, sizeof(buf2), "%02hhX", link.isis_area_id[i]);
                    strcat(isis_area_id, buf2);

                    if (i == 0)
                        strcat(isis_area_id, ".");
                }
            }
        }

        buf_len += snprintf(buf2, sizeof(buf2),
                "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIx64 "\t%" PRIx32 "\t%s\t%s\t%s\t%s\t%"
                        PRIu32 "\t%" PRIu32 "\t%s\t%" PRIx32 "\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%" PRIu32 "\t%" PRIu32
                        "\t%" PRIu32 "\t%" PRIu32 "\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%s\t%s\t%s\n",
                            action.c_str(), ls_link_seq, hash_str.c_str(), path_hash_str.c_str(),r_hash_str.c_str(),
                            router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                            igp_router_id, router_id, link.id, link.bgp_ls_id, ospf_area_id,
                            isis_area_id, link.protocol, attr.as_path.c_str(), attr.local_pref, attr.med, attr.next_hop,
                            link.mt_id, link.local_link_id, link.remote_link_id, intf_ip, nei_ip, link.igp_metric,
                            link.admin_group, link.max_link_bw, link.max_resv_bw, link.unreserved_bw, link.te_def_metric,
                            link.protection_type, link.mpls_proto_mask, link.srlg, link.name, remote_node_hash_id.c_str(),
                            local_node_hash_id.c_str());

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_link_seq;
    }

    produce(MSGBUS_TOPIC_VAR_LS_LINK, prep_buf, strlen(prep_buf), rows, peer_hash_str,
            &peer_list[peer_hash_str], peer.peer_as);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::update_LsPrefix(obj_bgp_peer &peer, obj_path_attr &attr, std::list<MsgBusInterface::obj_ls_prefix> &prefixes,
                                   ls_action_code code) {
    bzero(prep_buf, MSGBUS_WORKING_BUF_SIZE);

    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length
    int     i;

    string hash_str;
    string r_hash_str;
    string path_hash_str;
    string peer_hash_str;

    hash_toStr(peer.router_hash_id, r_hash_str);
    hash_toStr(attr.hash_id, path_hash_str);
    hash_toStr(peer.hash_id, peer_hash_str);

    string action = "add";
    switch (code) {
        case LS_ACTION_ADD:
            action = "add";
            break;
        case LS_ACTION_DEL:
            action = "del";
            break;
    }

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    string local_node_hash_id;

    char intf_ip[46];
    char nei_ip[46];
    char igp_router_id[46];
    char router_id[46];
    char ospf_fwd_addr[46];
    char prefix_ip[46];
    char ospf_area_id[16] = {0};
    char isis_area_id[32] = {0};
    char dr[16];

    // Loop through the vector array of entries
    int rows = 0;
    for (std::list<MsgBusInterface::obj_ls_prefix>::iterator it = prefixes.begin();
         it != prefixes.end(); it++) {

        ++rows;
        MsgBusInterface::obj_ls_prefix &prefix = (*it);

        MD5 hash;

        hash.update(prefix.prefix_bin, sizeof(prefix.prefix_bin));
        hash.update(&prefix.prefix_len, 1);
        hash.update((unsigned char *)&prefix.id, sizeof(prefix.id));
        hash.update(prefix.local_node_hash_id, sizeof(prefix.local_node_hash_id));
        hash.update((unsigned char *)prefix.ospf_route_type, sizeof(prefix.ospf_route_type));
        hash.update((unsigned char *)&prefix.mt_id, sizeof(prefix.mt_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(prefix.hash_id, hash_bin, 16);
        delete[] hash_bin;

        // Build the query
        hash_toStr(prefix.hash_id, hash_str);
        hash_toStr(prefix.local_node_hash_id, local_node_hash_id);

        if (prefix.isIPv4) {
            inet_ntop(PF_INET, prefix.intf_addr, intf_ip, sizeof(intf_ip));
            inet_ntop(PF_INET, prefix.nei_addr, nei_ip, sizeof(nei_ip));
            inet_ntop(PF_INET, prefix.ospf_fwd_addr, ospf_fwd_addr, sizeof(ospf_fwd_addr));
            inet_ntop(PF_INET, prefix.prefix_bin, prefix_ip, sizeof(prefix_ip));
            inet_ntop(PF_INET, prefix.router_id, router_id, sizeof(router_id));
        } else {
            inet_ntop(PF_INET6, prefix.intf_addr, intf_ip, sizeof(intf_ip));
            inet_ntop(PF_INET6, prefix.nei_addr, nei_ip, sizeof(nei_ip));
            inet_ntop(PF_INET6, prefix.router_id, router_id, sizeof(router_id));
            inet_ntop(PF_INET6, prefix.ospf_fwd_addr, ospf_fwd_addr, sizeof(ospf_fwd_addr));
            inet_ntop(PF_INET6, prefix.prefix_bin, prefix_ip, sizeof(prefix_ip));
        }

        if (!strcmp(prefix.protocol, "OSPFv3") or !strcmp(prefix.protocol, "OSPFv2") ) {
            bzero(isis_area_id, sizeof(isis_area_id));

            inet_ntop(PF_INET, prefix.igp_router_id, igp_router_id, sizeof(igp_router_id));

            if ((uint32_t) *(prefix.igp_router_id+4) != 0) {
                inet_ntop(PF_INET, prefix.igp_router_id+4, dr, sizeof(dr));
                strncat(igp_router_id, "[", 1);
                strncat(igp_router_id, dr, sizeof(dr));
                strncat(igp_router_id, "]", 1);
            }


            inet_ntop(PF_INET, prefix.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));
        } else {
            bzero(ospf_area_id, sizeof(ospf_area_id));

            snprintf(igp_router_id, sizeof(igp_router_id),
                     "%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX",
                     prefix.igp_router_id[0], prefix.igp_router_id[1], prefix.igp_router_id[2], prefix.igp_router_id[3],
                     prefix.igp_router_id[4], prefix.igp_router_id[5], prefix.igp_router_id[6], prefix.igp_router_id[7]);

            if (prefix.isis_area_id[8] <= sizeof(prefix.isis_area_id))
                for (i=0; i < prefix.isis_area_id[8]; i++) {
                    snprintf(buf2, sizeof(buf2), "%02hhX", prefix.isis_area_id[i]);
                    strcat(isis_area_id, buf2);

                    if (i == 0)
                        strcat(isis_area_id, ".");
                }
        }


        buf_len += snprintf(buf2, sizeof(buf2),
                "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIx64 "\t%" PRIx32
                        "\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%" PRIx32 "\t%s\t%s\t%" PRIu32 "\t%" PRIx64
                            "\t%s\t%" PRIu32 "\t%s\t%d\n",
                            action.c_str(), ls_prefix_seq, hash_str.c_str(), path_hash_str.c_str(), r_hash_str.c_str(),
                            router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                            igp_router_id, router_id, prefix.id, prefix.bgp_ls_id, ospf_area_id, isis_area_id,
                            prefix.protocol, attr.as_path.c_str(), attr.local_pref, attr.med, attr.next_hop, local_node_hash_id.c_str(),
                            prefix.mt_id, prefix.ospf_route_type, prefix.igp_flags, prefix.route_tag, prefix.ext_route_tag,
                            ospf_fwd_addr, prefix.metric, prefix_ip, prefix.prefix_len);

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_prefix_seq;
    }

    produce(MSGBUS_TOPIC_VAR_LS_PREFIX, prep_buf, strlen(prep_buf), rows, peer_hash_str,
            &peer_list[peer_hash_str], peer.peer_as);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::send_bmp_raw(u_char *r_hash, obj_bgp_peer &peer, u_char *data, size_t data_len) {
    string r_hash_str;
    string p_hash_str;
    RdKafka::Topic *topic = NULL;

    hash_toStr(peer.hash_id, p_hash_str);
    hash_toStr(r_hash, r_hash_str);

    SELF_DEBUG("rtr=%s: Producing bmp raw message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
               MSGBUS_TOPIC_VAR_BMP_RAW, r_hash_str.c_str(), data_len);

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
        RdKafka::ErrorCode resp = producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                                                    RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                                                    producer_buf, data_len + hdr_len,
                                                    (const std::string *)&r_hash_str, NULL);

        if (resp != RdKafka::ERR_NO_ERROR)
            LOG_ERR("rtr=%s: Failed to produce bmp raw message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());
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
