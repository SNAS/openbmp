/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
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

#include <boost/algorithm/string/replace.hpp>

#include "md5.h"

using namespace std;

/******************************************************************//**
 * \brief This function will initialize and connect to MySQL.
 *
 * \details It is expected that this class will start off with a new connection.
 *
 *  \param [in] logPtr      Pointer to Logger instance
 *  \param [in] brokerList  Comma delimited list of brokers (e.g. localhost:9092,host2:9092)
 *  \param [in] c_hash_id   Collector Hash ID
 ********************************************************************/
msgBus_kafka::msgBus_kafka(Logger *logPtr, string brokerList, u_char *c_hash_id) {
    logger = logPtr;

    producer_buf = new unsigned char[MSGBUS_WORKING_BUF_SIZE];
    prep_buf = new char[MSGBUS_WORKING_BUF_SIZE];

    hash_toStr(c_hash_id, collector_hash);

    isConnected = false;
    conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

    disableDebug();

    initTopicMap();

    router_seq          = 0L;
    collector_seq       = 0L;
    peer_seq            = 0L;
    base_attr_seq       = 0L;
    unicast_prefix_seq  = 0L;
    ls_node_seq         = 0L;
    ls_link_seq         = 0L;
    ls_prefix_seq       = 0L;
    bmp_stat_seq        = 0L;

    broker_list = brokerList;

    // Make the connection to the server
    event_callback      = NULL;
    delivery_callback   = NULL;
    producer            = NULL;

    router_ip.assign("");
    connect();
}

/**
 * Destructor
 */
msgBus_kafka::~msgBus_kafka() {

    delete [] producer_buf;
    delete [] prep_buf;

    SELF_DEBUG("Destory msgBus Kafka instance");

    router_list.clear();
    peer_list.clear();

    producer->poll(700);

    // Free vars
    delete conf;
    delete tconf;

    // Free topic pointers
    for (topic_map::iterator it = topic.begin(); it != topic.end(); it++) {
        if (it->second) {
            delete it->second;
            it->second = NULL;
        }
    }

    if (producer != NULL) delete producer;

    if (event_callback != NULL) delete event_callback;
    if (delivery_callback != NULL) delete delivery_callback;

}

/**
 * Connects to Kafka broker
 */
void msgBus_kafka::connect() {
    string errstr;
    string value;

    // Free topic pointers
    for (topic_map::iterator it = topic.begin(); it != topic.end(); it++) {
        if (it->second != NULL) {
            delete it->second;
            it->second = NULL;
        }
    }

    if (producer != NULL) delete producer;
    producer                 = NULL;

    /*
     * Configure Kafka Producer (https://kafka.apache.org/08/configuration.html)
     */
    //TODO: Add config options to change these settings

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
    value = "snappy";
    if (conf->set("compression.codec", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure Snappy compression for kafka: %s.", errstr.c_str());
        throw "ERROR: Failed to configure kafka compression to Snappy";
    }

    // broker list
    if (conf->set("metadata.broker.list", broker_list, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker list for kafka: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka broker list";
    }

    // Register event callback
    if (event_callback != NULL) delete event_callback;
    event_callback = new KafkaEventCallback(&isConnected, logger);
    if (conf->set("event_cb", event_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka event callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka event callback";
    }

    // Register delivery report callback
    if (delivery_callback != NULL) delete delivery_callback;
    delivery_callback = new KafkaDeliveryReportCallback();

    if (conf->set("dr_cb", delivery_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka delivery report callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka delivery report callback";
    }

    // Create producer and connect
    producer = RdKafka::Producer::create(conf, errstr);
    if (!producer) {
        LOG_ERR("rtr=%s: Failed to create producer: %s", router_ip.c_str(), errstr.c_str());
        throw "ERROR: Failed to create producer";
    }

    isConnected = true;

    producer->poll(200);

    if (not isConnected) {
        LOG_ERR("rtr=%s: Failed to connect to Kafka, will try again in a few", router_ip.c_str());
        delete producer;
        producer = NULL;
        return;

    }

    /*
     * Create the topics
     */
    for (topic_map::iterator it = topic.begin(); it != topic.end(); it++) {
        it->second = RdKafka::Topic::create(producer, it->first.c_str(), tconf, errstr);
        if (it->second == NULL) {
            LOG_ERR("rtr=%s: Failed to create '%s' topic: %s", router_ip.c_str(), it->first.c_str(), errstr.c_str());
            throw "ERROR: Failed to create topic";
        }
    }

    producer->poll(100);
}

/**
 * produce message to Kafka
 *
 * \param [in] topic]        Topic name to lookup in the topic map
 * \param [in] msg           message to produce
 * \param [in] msg_size      Length in bytes of the message
 * \param [in] rows          Number of rows
 * \param [in] key           Hash key
 */
void msgBus_kafka::produce(const char *topic_name, char *msg, size_t msg_size, int rows, string key) {

    while (isConnected == false or topic[topic_name] == NULL) {

        // Do not attempt to reconnect if this is the main process (router ip is null)
        if (router_ip.size() <= 1)
            break;

        LOG_WARN("rtr=%s: Not connected to Kafka, attempting to reconnect", router_ip.c_str());
        connect();

        sleep(2);
    }

    SELF_DEBUG("rtr=%s: Producing message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
               topic_name, key.c_str(), msg_size);

    char headers[128];
    snprintf(headers, sizeof(headers), "V: 1\nC_HASH_ID: %s\nL: %lu\nR: %d\n\n",
            collector_hash.c_str(), msg_size, rows);

    memcpy(producer_buf, headers, sizeof(headers));
    memcpy(producer_buf+strlen(headers), msg, msg_size);

    RdKafka::ErrorCode resp = producer->produce(topic[topic_name], 0,
                                                RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                                                producer_buf, msg_size + strlen(headers),
                                                (const std::string *)&key, NULL);

    if (resp != RdKafka::ERR_NO_ERROR)
        LOG_ERR("rtr=%s: Failed to produce message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());

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

    produce(MSGBUS_TOPIC_COLLECTOR, buf, strlen(buf), 1, collector_hash);

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

    bool skip_if_in_cache = true;
    bool add_to_cache = true;

    string action = "first";

    switch (code) {
        case ROUTER_ACTION_FIRST :
            action.assign("first");
            break;

        case ROUTER_ACTION_INIT :
            skip_if_in_cache = false;
            action.assign("init");
            break;

        case ROUTER_ACTION_TERM:
            skip_if_in_cache = false;
            action.assign("term");
            add_to_cache = false;

            if (router_list.find(r_hash_str) != router_list.end())
                router_list.erase(r_hash_str);

            break;
    }


    // Check if we have already processed this entry, if so update it an return
    if (skip_if_in_cache and router_list.find(r_hash_str) != router_list.end()) {
        router_list[r_hash_str] = time(NULL);
        return;
    }

    router_ip.assign((char *)r_object.ip_addr);                     // Update router IP for logging

    // Insert/Update map entry
    if (add_to_cache)
        router_list[r_hash_str] = time(NULL);

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
    if (strlen((char *)r_object.name) <= 0) {
        string hostname;
        resolveIp((char *) r_object.ip_addr, hostname);
        snprintf((char *)r_object.name, sizeof(r_object.name), "%s", hostname.c_str());
    }

    snprintf(buf, sizeof(buf),
             "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%" PRIu16 "\t%s\t%s\t%s\t%s\n", action.c_str(),
             router_seq, r_object.name, r_hash_str.c_str(), r_object.ip_addr, descr.c_str(),
             r_object.term_reason_code, r_object.term_reason_text,
             initData.c_str(), termData.c_str(), ts.c_str());

    produce(MSGBUS_TOPIC_ROUTER, buf, strlen(buf), 1, r_hash_str);

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

    // Check if we have already processed this entry, if so update it an return
    if (skip_if_in_cache and peer_list.find(p_hash_str) != peer_list.end()) {
        peer_list[p_hash_str] = time(NULL);
        return;
    }

    // Get the hostname using DNS
    string hostname;
    resolveIp(peer.peer_addr, hostname);

    string ts;
    getTimestamp(peer.timestamp_secs, peer.timestamp_us, ts);

    // Insert/Update map entry
    if (add_to_cache)
        peer_list[p_hash_str] = time(NULL);

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

    produce(MSGBUS_TOPIC_PEER, buf, strlen(buf), 1, p_hash_str);

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
    hash.update((unsigned char *) attr.as_path,
                strlen(attr.as_path));
    hash.update((unsigned char *) attr.next_hop,
                strlen(attr.next_hop));
    hash.update((unsigned char *) attr.aggregator,
                strlen(attr.aggregator));
    hash.update((unsigned char *) attr.origin,
                strlen(attr.origin));
    hash.update((unsigned char *) &attr.med, sizeof(attr.med));
    hash.update((unsigned char *) &attr.local_pref,
                sizeof(attr.local_pref));

    hash.update((unsigned char *) attr.community_list, strlen(attr.community_list));
    hash.update((unsigned char *) attr.ext_community_list, strlen(attr.ext_community_list));
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
                     attr.origin, attr.as_path, attr.as_path_count, attr.origin_as, attr.next_hop, attr.med,
                     attr.local_pref, attr.aggregator, attr.community_list, attr.ext_community_list, attr.cluster_list,
                     attr.atomic_agg, attr.nexthop_isIPv4, attr.originator_id);

    produce(MSGBUS_TOPIC_BASE_ATTRIBUTE, prep_buf, buf_len, 1, p_hash_str);

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
                                            "\t%" PRIu32 "\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%s\t%s\t%d\t%d\t%s\n",
                                    action.c_str(), unicast_prefix_seq, rib_hash_str.c_str(), r_hash_str.c_str(),
                                    router_ip.c_str(),path_hash_str.c_str(), p_hash_str.c_str(),
                                    peer.peer_addr, peer.peer_as, ts.c_str(), rib[i].prefix, rib[i].prefix_len,
                                    rib[i].isIPv4, attr->origin,
                                    attr->as_path, attr->as_path_count, attr->origin_as, attr->next_hop, attr->med, attr->local_pref,
                                    attr->aggregator,
                                    attr->community_list, attr->ext_community_list, attr->cluster_list,
                                    attr->atomic_agg, attr->nexthop_isIPv4,
                                    attr->originator_id);
                break;

            case UNICAST_PREFIX_ACTION_DEL:
                buf_len += snprintf(buf2, sizeof(buf2),
                                    "%s\t%" PRIu64 "\t%s\t%s\t%s\t\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%d\t%d\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n",
                                    action.c_str(), unicast_prefix_seq, rib_hash_str.c_str(), r_hash_str.c_str(),
                                    router_ip.c_str(), p_hash_str.c_str(),
                                    peer.peer_addr, peer.peer_as, ts.c_str(), rib[i].prefix, rib[i].prefix_len,
                                    rib[i].isIPv4);
                break;
        }

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++unicast_prefix_seq;
    }


    produce(MSGBUS_TOPIC_UNICAST_PREFIX, prep_buf, strlen(prep_buf), rib.size(), p_hash_str);
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


    produce(MSGBUS_TOPIC_BMP_STAT, buf, strlen(buf), 1, p_hash_str);
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
            inet_ntop(PF_INET, node.igp_router_id, igp_router_id, sizeof(igp_router_id));
            inet_ntop(PF_INET, node.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));
        } else {
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
                        "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIx64 "\t%" PRIx32 "\t%" PRIu32
                                "\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\n",
                        action.c_str(),ls_node_seq, hash_str.c_str(),path_hash_str.c_str(), r_hash_str.c_str(),
                        router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                        igp_router_id, router_id, node.id, node.bgp_ls_id,node.mt_id, ospf_area_id, isis_area_id,
                        node.protocol, node.flags, attr.as_path, attr.local_pref, attr.med, attr.next_hop, node.name);

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_node_seq;
    }


    produce(MSGBUS_TOPIC_LS_NODE, prep_buf, buf_len, rows, peer_hash_str);
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
    char ospf_area_id[16] = {0};
    char isis_area_id[32] = {0};

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
            inet_ntop(PF_INET, link.igp_router_id, igp_router_id, sizeof(igp_router_id));
            inet_ntop(PF_INET, link.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));
        } else {
            snprintf(igp_router_id, sizeof(igp_router_id),
                     "%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX.%02hhX%02hhX",
                     link.igp_router_id[0], link.igp_router_id[1], link.igp_router_id[2], link.igp_router_id[3],
                     link.igp_router_id[4], link.igp_router_id[5], link.igp_router_id[6], link.igp_router_id[7]);

            if (link.isis_area_id[8] <= sizeof(link.isis_area_id))
                for (i=0; i < link.isis_area_id[8]; i++) {
                    snprintf(buf2, sizeof(buf2), "%02hhX", link.isis_area_id[i]);
                    strcat(isis_area_id, buf2);

                    if (i == 0)
                        strcat(isis_area_id, ".");
                }
        }

        buf_len += snprintf(buf2, sizeof(buf2),
                "%s\t%" PRIu64 "\t%s\t%s\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%" PRIx64 "\t%" PRIx32 "\t%s\t%s\t%s\t%s\t%"
                        PRIu32 "\t%" PRIu32 "\t%s\t%" PRIx32 "\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%" PRIu32 "\t%" PRIu32
                        "\t%f\t%f\t%s\t%" PRIu32 "\t%s\t%s\t%s\t%s\t%s\t%s\n",
                            action.c_str(), ls_link_seq, hash_str.c_str(), path_hash_str.c_str(),r_hash_str.c_str(),
                            router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                            igp_router_id, router_id, link.id, link.bgp_ls_id, ospf_area_id,
                            isis_area_id, link.protocol, attr.as_path, attr.local_pref, attr.med, attr.next_hop,
                            link.mt_id, link.local_link_id, link.remote_link_id, intf_ip, nei_ip, link.igp_metric,
                            link.admin_group, link.max_link_bw, link.max_resv_bw, link.unreserved_bw, link.te_def_metric,
                            link.protection_type, link.mpls_proto_mask, link.srlg, link.name, remote_node_hash_id.c_str(),
                            local_node_hash_id.c_str());

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_link_seq;
    }

    produce(MSGBUS_TOPIC_LS_LINK, prep_buf, strlen(prep_buf), rows, peer_hash_str);
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
            inet_ntop(PF_INET, prefix.igp_router_id, igp_router_id, sizeof(igp_router_id));
            inet_ntop(PF_INET, prefix.ospf_area_Id, ospf_area_id, sizeof(ospf_area_id));
        } else {
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
                        "\t%s\t%s\t%s\t%s\t%" PRIu32 "\t%" PRIu32 "\t%s\t%s\t%" PRIu32 "\t%s\t%s\t%" PRIu32 "\t%" PRIx64
                            "\t%s\t%" PRIu32 "\t%s\t%d\n",
                            action.c_str(), ls_prefix_seq, hash_str.c_str(), path_hash_str.c_str(), r_hash_str.c_str(),
                            router_ip.c_str(), peer_hash_str.c_str(), peer.peer_addr, peer.peer_as, ts.c_str(),
                            igp_router_id, router_id, prefix.id, prefix.bgp_ls_id, ospf_area_id, isis_area_id,
                            prefix.protocol, attr.as_path, attr.local_pref, attr.med, attr.next_hop, local_node_hash_id.c_str(),
                            prefix.mt_id, prefix.ospf_route_type, prefix.igp_flags, prefix.route_tag, prefix.ext_route_tag,
                            ospf_fwd_addr, prefix.metric, prefix_ip, prefix.prefix_len);

        // Cat the entry to the query buff
        if (buf_len < MSGBUS_WORKING_BUF_SIZE /* size of buf */)
            strcat(prep_buf, buf2);

        ++ls_prefix_seq;
    }

    produce(MSGBUS_TOPIC_LS_PREFIX, prep_buf, strlen(prep_buf), rows, peer_hash_str);
}

/**
 * Abstract method Implementation - See MsgBusInterface.hpp for details
 */
void msgBus_kafka::send_bmp_raw(u_char *r_hash, u_char *data, size_t data_len) {
    string r_hash_str;
    hash_toStr(r_hash, r_hash_str);

    SELF_DEBUG("rtr=%s: Producing bmp raw message: topic=%s key=%s, msg size = %lu", router_ip.c_str(),
               MSGBUS_TOPIC_BMP_RAW, r_hash_str.c_str(), data_len);

    if (data_len == 0)
        return;

    while (isConnected == false or topic[MSGBUS_TOPIC_BMP_RAW] == NULL) {
        LOG_WARN("rtr=%s: Not connected to Kafka, attempting to reconnect", router_ip.c_str());
        connect();

        sleep(2);
    }

    char headers[128];
    snprintf(headers, sizeof(headers), "V: 1\nC_HASH_ID: %s\nR_HASH: %s\nR_IP: %s\nL: %lu\n\n",
             collector_hash.c_str(), r_hash_str.c_str(), router_ip.c_str(), data_len);

    memcpy(producer_buf, headers, sizeof(headers));
    memcpy(producer_buf+strlen(headers), data, data_len);

    RdKafka::ErrorCode resp = producer->produce(topic[MSGBUS_TOPIC_BMP_RAW], 0,
                                                RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                                                producer_buf, data_len + strlen(headers),
                                                (const std::string *)&r_hash_str, NULL);

    if (resp != RdKafka::ERR_NO_ERROR)
        LOG_ERR("rtr=%s: Failed to produce bmp raw message: %s", router_ip.c_str(), RdKafka::err2str(resp).c_str());

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

    if (!getaddrinfo(name.c_str(), NULL, NULL, &ai) and
            !getnameinfo(ai->ai_addr,ai->ai_addrlen, host, sizeof(host), NULL, 0, NI_NAMEREQD)) {

        hostname.assign(host);
        LOG_INFO("resovle: %s to %s", name.c_str(), hostname.c_str());

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
