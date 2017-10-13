/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <arpa/inet.h>
#include <boost/algorithm/string/replace.hpp>

#include "KafkaTopicSelector.h"
#include "kafka/MsgBusImpl_kafka.h"

/*********************************************************************//**
 * Constructor for class
 *
 * \param [in] logPtr   Pointer to Logger instance
 * \param [in] cfg      Pointer to the config instance
 * \param [in] producer Pointer to the kafka producer
 ***********************************************************************/
KafkaTopicSelector::KafkaTopicSelector(Logger *logPtr, Config *cfg,  RdKafka::Producer *producer) {
    logger = logPtr;
    this->cfg = cfg;

    if (cfg->debug_msgbus)
        debug = true;
    else
        debug = false;


    this->producer = producer;

    peer_partitioner_callback = new KafkaPeerPartitionerCallback();
    tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

}

/*********************************************************************//**
 * Destructor for class
 ***********************************************************************/
KafkaTopicSelector::~KafkaTopicSelector() {
    SELF_DEBUG("Destory KafkaTopicSeletor");

    freeTopicMap();

    if (peer_partitioner_callback != NULL)
        delete peer_partitioner_callback;

    delete tconf;

}

/*********************************************************************//**
 * Gets topic pointer by topic var name, router and peer group.
 *     If the topic doesn't exist, a new entry will be initialized.
 *
 * \param [in]  topic_var       MSGBUS_TOPIC_VAR_<name>
 * \param [in]  router_group    Router group - empty/NULL means no router group
 * \param [in]  peer_group      Peer group - empty/NULL means no peer group
 * \param [in]  peer_asn        Peer asn (remote asn)
 *
 * \return (RdKafka::Topic *) pointer or NULL if error
 ***********************************************************************/

RdKafka::Topic * KafkaTopicSelector::getTopic(const std::string &topic_var,
                                              const std::string *router_group, const std::string *peer_group,
                                              uint32_t peer_asn) {

    // Update the topic key based on the peer_group/router_group
    std::string topic_key = getTopicKey(topic_var, router_group, peer_group, peer_asn);

    topic_map::iterator t_it;

    if ( (t_it=topic.find(topic_key)) != topic.end()) {
        return t_it->second;                                              // Return the existing initialized topic
    }
    else {
        SELF_DEBUG("Requesting to create topic for key=%s", topic_key.c_str());
        return initTopic(topic_var, router_group, peer_group, peer_asn);  // create and return newly created topic
    }

    return NULL;
}

/*********************************************************************//**
 * Check if a topic is enabled
 *
 * \param [in]  topic_var       MSGBUS_TOPIC_VAR_<name>
 *
 * \return bool true if the topic is enabled, false otherwise
***********************************************************************/
bool KafkaTopicSelector::topicEnabled(const std::string &topic_var) {
    return this->cfg->topic_names_map[topic_var].length() > 0;
}

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
void KafkaTopicSelector::lookupPeerGroup(std::string hostname, std::string ip_addr, uint32_t peer_asn,
                                         std::string &peer_group_name) {

    peer_group_name = "";

    /*
     * Match against hostname regexp
     */
    if (hostname.size() > 0) {

        // Loop through all groups and their regular expressions
        for (Config::match_peer_group_by_name_iter it = cfg->match_peer_group_by_name.begin();
            it != cfg->match_peer_group_by_name.end(); ++it) {

            // loop through all regexps to see if there is a match
            for (std::list<Config::match_type_regex>::iterator lit = it->second.begin();
                    lit != it->second.end(); ++lit) {
                if (regex_search(hostname, lit->regexp)) {
                    SELF_DEBUG("Regexp matched hostname %s to peer group '%s'",
                                hostname.c_str(), it->first.c_str());
                    peer_group_name = it->first;
                    return;
                }
            }
        }
    }

    /*
     * Match against prefix ranges
     */
    bool isIPv4 = ip_addr.find_first_of(':') == std::string::npos ? true : false;
    uint8_t bits;

    uint32_t prefix[4]  __attribute__ ((aligned));
    bzero(prefix,sizeof(prefix));

    inet_pton(isIPv4 ? AF_INET : AF_INET6, ip_addr.c_str(), prefix);

    // Loop through all groups and their regular expressions
    for (Config::match_peer_group_by_ip_iter it = cfg->match_peer_group_by_ip.begin();
         it != cfg->match_peer_group_by_ip.end(); ++it) {

        // loop through all prefix ranges to see if there is a match
        for (std::list<Config::match_type_ip>::iterator lit = it->second.begin();
             lit != it->second.end(); ++lit) {

            if (lit->isIPv4 == isIPv4) { // IPv4
                bits = 32 - lit->bits;

                // Big endian
                prefix[0] <<= bits;
                prefix[0] >>= bits;

                if (prefix[0] == lit->prefix[0]) {
                    SELF_DEBUG("IP %s matched peer group %s", ip_addr.c_str(), it->first.c_str());
                    peer_group_name = it->first;
                    return;
                }
            } else { // IPv6
                uint8_t end_idx = lit->bits / 32;
                bits = lit->bits - (32 * end_idx);

                if (bits == 0)
                    end_idx--;

                if (end_idx < 4 and bits < 32) {    // end_idx should be less than 4 and bits less than 32

                    // Big endian
                    prefix[end_idx] <<= bits;
                    prefix[end_idx] >>= bits;
                }

                if (prefix[0] == lit->prefix[0] and prefix[1] == lit->prefix[1]
                        and prefix[2] == lit->prefix[2] and prefix[3] == lit->prefix[3]) {

                    SELF_DEBUG("IP %s matched peer group %s", ip_addr.c_str(), it->first.c_str());
                    peer_group_name = it->first;
                    return;
                }
            }
        }
    }

    /*
     * Match against asn list
     */
    // Loop through all groups and their regular expressions
    for (Config::match_peer_group_by_asn_iter it = cfg->match_peer_group_by_asn.begin();
         it != cfg->match_peer_group_by_asn.end(); ++it) {

        // loop through all prefix ranges to see if there is a match
        for (std::list<uint32_t>::iterator lit = it->second.begin();
             lit != it->second.end(); ++lit) {

            if (*lit == peer_asn) {
                SELF_DEBUG("Peer ASN %u matched peer group %s", peer_asn, it->first.c_str());
                peer_group_name = it->first;
                return;
            }
        }
    }

}

/*********************************************************************//**
 * Lookup router group
 *
 * \param [in]  hostname          hostname/fqdn of the router
 * \param [in]  ip_addr           IP address of the peer (printed form)
 * \param [out] router_group_name Reference to string where router group will be updated
 *
 * \return bool true if matched, false if no matched peer group
 ***********************************************************************/
void KafkaTopicSelector::lookupRouterGroup(std::string hostname, std::string ip_addr,
                                         std::string &router_group_name) {

    router_group_name = "";

    SELF_DEBUG("router lookup for hostname=%s and ip_addr=%s", hostname.c_str(), ip_addr.c_str());

    /*
     * Match against hostname regexp
     */
    if (hostname.size() > 0) {

        // Loop through all groups and their regular expressions
        for (Config::match_router_group_by_name_iter it = cfg->match_router_group_by_name.begin();
             it != cfg->match_router_group_by_name.end(); ++it) {

            // loop through all regexps to see if there is a match
            for (std::list<Config::match_type_regex>::iterator lit = it->second.begin();
                 lit != it->second.end(); ++lit) {

                if (regex_search(hostname, lit->regexp)) {
                    SELF_DEBUG("Regexp matched hostname %s to router group '%s'",
                               hostname.c_str(), it->first.c_str());
                    router_group_name = it->first;
                    return;
                }
            }
        }
    }

    /*
     * Match against prefix ranges
     */
    bool isIPv4 = ip_addr.find_first_of(':') == std::string::npos ? true : false;
    uint8_t bits;

    uint32_t prefix[4]  __attribute__ ((aligned));
    bzero(prefix,sizeof(prefix));

    inet_pton(isIPv4 ? AF_INET : AF_INET6, ip_addr.c_str(), prefix);

    // Loop through all groups and their regular expressions
    for (Config::match_router_group_by_ip_iter it = cfg->match_router_group_by_ip.begin();
         it != cfg->match_router_group_by_ip.end(); ++it) {

        // loop through all prefix ranges to see if there is a match
        for (std::list<Config::match_type_ip>::iterator lit = it->second.begin();
             lit != it->second.end(); ++lit) {

            if (lit->isIPv4 == isIPv4) { // IPv4

                bits = 32 - lit->bits;

                // Big endian
                prefix[0] <<= bits;
                prefix[0] >>= bits;

                if (prefix[0] == lit->prefix[0]) {
                    SELF_DEBUG("IP %s matched router group %s", ip_addr.c_str(), it->first.c_str());
                    router_group_name = it->first;
                    return;
                }
            } else { // IPv6
                uint8_t end_idx = lit->bits / 32;
                bits = lit->bits - (32 * end_idx);

                if (bits == 0)
                    end_idx--;

                if (end_idx < 4 and bits < 32) {    // end_idx should be less than 4 and bits less than 32

                    // Big endian
                    prefix[end_idx] <<= bits;
                    prefix[end_idx] >>= bits;
                }

                if (prefix[0] == lit->prefix[0] and prefix[1] == lit->prefix[1]
                    and prefix[2] == lit->prefix[2] and prefix[3] == lit->prefix[3]) {

                    SELF_DEBUG("IP %s matched router group %s", ip_addr.c_str(), it->first.c_str());
                    router_group_name = it->first;
                    return;
                }
            }
        }
    }
}



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
RdKafka::Topic * KafkaTopicSelector::initTopic(const std::string &topic_var,
                                               const std::string *router_group, const std::string *peer_group,
                                               uint32_t peer_asn) {
    std::string errstr;
    char uint32_str[12];
    topic_flags tflags;

    // Get the actual topic name based on var
    std::string topic_name = this->cfg->topic_names_map[topic_var];

    /*
     * topics that contain the peer asn need to have the key include the peer asn
     */
    if (topic_name.find("{peer_asn}") != std::string::npos) {
        topic_flags_map[topic_var].include_peerAsn = true;
        SELF_DEBUG("peer_asn found in topic %s, setting topic flag to include peer ASN", topic_name.c_str());
    } else {
        topic_flags_map[topic_var].include_peerAsn = false;
    }

    // Update the topic key based on the peer_group/router_group
    std::string topic_key = getTopicKey(topic_var, router_group, peer_group, peer_asn);

    // Update the topic name based on app variables
    if (topic_var.compare(MSGBUS_TOPIC_VAR_COLLECTOR)) {   // if not collector topic
        if (router_group != NULL and router_group->size() > 0) {
            boost::replace_all(topic_name, "{router_group}", *router_group);
        } else
            boost::replace_all(topic_name, "{router_group}", "default");

        if (topic_var.compare(MSGBUS_TOPIC_VAR_ROUTER)) {    // if not router topic
            if (peer_group != NULL and peer_group->size() > 0) {
                boost::replace_all(topic_name, "{peer_group}", *peer_group);
            } else
                boost::replace_all(topic_name, "{peer_group}", "default");

            if (peer_asn > 0) {
                snprintf(uint32_str, sizeof(uint32_str), "%u", peer_asn);
                boost::replace_all(topic_name, "{peer_asn}", (const char *)uint32_str);
            } else
                boost::replace_all(topic_name, "{peer_asn}", "default");
        }
    }

    SELF_DEBUG("Creating topic %s (map key=%s)" , topic_name.c_str(), topic_key.c_str());

    // Delete topic if it already exists
    topic_map::iterator t_it;

    if ( (t_it=topic.find(topic_key)) != topic.end() and t_it->second != NULL) {
        delete t_it->second;
    }

    /*
     * Topic configuration
     */
    if (tconf->set("partitioner_cb", peer_partitioner_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka partitioner callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka partitioner callback";
    }

    topic[topic_key] = RdKafka::Topic::create(producer, topic_name.c_str(), tconf, errstr);

    if (topic[topic_key] == NULL) {
        LOG_ERR("Failed to create '%s' topic: %s", topic_name.c_str(), errstr.c_str());
        throw "ERROR: Failed to create topic";

    } else {
        return topic[topic_key];
    }

    return NULL;
}

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
std::string KafkaTopicSelector::getTopicKey(const std::string &topic_var,
                                            const std::string *router_group, const std::string *peer_group,
                                            uint32_t peer_asn) {

    std::string topic_key = topic_var;
    char uint32_str[12];

    // Update the topic name based on app variables
    if (topic_var.compare(MSGBUS_TOPIC_VAR_COLLECTOR)) {   // if not collector topic
        topic_key += "_";

        if (router_group != NULL and router_group->size() > 0) {
            topic_key += *router_group;
        }

        if (topic_var.compare(MSGBUS_TOPIC_VAR_ROUTER)) {    // if not router topic
            topic_key += "_";

            if (peer_group != NULL and peer_group->size() > 0) {
                topic_key += *peer_group;
            }

            if (topic_flags_map[topic_var].include_peerAsn) {
                topic_key += "_";
                if (peer_asn > 0) {
                    snprintf(uint32_str, sizeof(uint32_str), "%u", peer_asn);
                    topic_key += uint32_str;
                }
            }
        }
    }

    return topic_key;
}

/**
 * Free allocated topic map pointers
 */
void KafkaTopicSelector::freeTopicMap() {
    // Free topic pointers
    for (topic_map::iterator it = topic.begin(); it != topic.end(); it++) {
        if (it->second) {
            delete it->second;
            it->second = NULL;
        }
    }
}