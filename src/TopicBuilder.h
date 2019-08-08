//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_TOPICBUILDER_H
#define OPENBMP_TOPICBUILDER_H

#include <string>
#include "Config.h"
#include "Logger.h"
#include "Constants.h"

using namespace std;

/*
Build kafka topic string based on topic template.
The constructor will read each topic template and build a list of variables it needs to implement the topic.
To build a topic, it searches the list of variables in order in topic_tree, and returns the value of the leaf;
 the complete topic string that complies with the topic template.
If the leaf node does not exist, we create new node(s) to complete the search tree,
 and insert the completed topic string to the leaf node.
*/


class TopicBuilder {
public:
    TopicBuilder();

    string get_collector_topic_string();  // initialized by constructor
    string get_router_topic_string(const string& router_ip);  // initialized by constructor
    // find raw_bmp topic in cache or generate one
    string get_raw_bmp_topic_string(const string& router_ip, const string& peer_ip, uint32_t peer_asn);


private:
    Config *config;
    Logger *logger;
    bool debug;
    // whether grouping is required
    // grouping incurs extra parsing cost

    string collector_topic_string;
    string router_topic_string;
    bool router_topic_string_initialized = false;
    map<string, string> bmp_raw_topic_strings;

    // cache variables
    string router_group = "";
    string router_hostname = "";
    map<string, string> peer_hostnames;
    map<string, string> peer_groups;

    /*
     * Variables available to router topic
     */
    bool router_topic_requires_router_group = false;
    bool router_topic_requires_router_hostname = false;
    bool router_topic_requires_router_ip = false;

    /*
     * Variables available to each raw bmp topic
     */
    bool raw_bmp_topic_requires_collector_group = false;
    bool raw_bmp_topic_requires_collector_name = false;
    bool raw_bmp_topic_requires_router_group = false;
    bool raw_bmp_topic_requires_router_hostname = false;
    bool raw_bmp_topic_requires_router_ip = false;
    bool raw_bmp_topic_requires_peer_group = false;
    bool raw_bmp_topic_requires_peer_asn = false;
    bool raw_bmp_topic_requires_peer_ip = false;


    // function to find router_group
    void find_router_group(const string& ip_addr, const string& router_hostname);
    void find_peer_group(string hostname, const string& ip_addr, uint32_t peer_asn, string &peer_group_name);

    // get hostname by ip
    string resolve_ip(const string& ip);
};

#endif //OPENBMP_TOPICBUILDER_H
