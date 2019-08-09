//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_TOPICBUILDER_H
#define OPENBMP_TOPICBUILDER_H

#include <string>
#include "Config.h"
#include "Logger.h"
#include "Constant.h"
#include "Utility.h"

using namespace std;

/*
Build kafka topic string based on topic template.
The constructor will read each topic template and build a list of variables it needs to implement the topic.
To get_encap_msg a topic, it searches the list of variables in order in topic_tree, and returns the value of the leaf;
 the complete topic string that complies with the topic template.
If the leaf node does not exist, we create new node(s) to complete the search tree,
 and insert the completed topic string to the leaf node.
*/


class TopicBuilder {
public:
    TopicBuilder(string &router_ip, string &router_hostname);

    string get_collector_topic_string();  // initialized by constructor
    string get_router_topic_string();  // initialized by constructor
    // find raw_bmp topic in cache or generate one
    string get_raw_bmp_topic_string(const string& peer_ip, uint32_t peer_asn);

    // can return router group name once its initialized
    string get_router_group();

private:
    Config *config;
    Logger *logger;
    bool debug;

    // collector topic and router topic will be initialized at construction time
    string collector_topic_string;
    string router_topic_string;
    // bmp_raw_topic_template will be partially filled at construction time
    string bmp_raw_topic_template;
    map<string, string> bmp_raw_topic_strings;

    // cache variables
    string router_group = "";
    string router_ip = "";
    string router_hostname = "";
    map<string, string> peer_hostnames;
    map<string, string> peer_groups;

    /*
     * Variables available to each raw bmp topic
     */
    bool raw_bmp_topic_requires_peer_group = false;
    bool raw_bmp_topic_requires_peer_asn = false;
    bool raw_bmp_topic_requires_peer_ip = false;


    // function to find router_group
    void find_router_group();
    void find_peer_group(string peer_hostname, const string& peer_ip, uint32_t peer_asn, string &peer_group_name);

};

#endif //OPENBMP_TOPICBUILDER_H
