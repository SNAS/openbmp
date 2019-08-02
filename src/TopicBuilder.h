//
// Created by Lumin Shi on 2019-07-29.
//

#ifndef OPENBMP_TOPICBUILDER_H
#define OPENBMP_TOPICBUILDER_H

#include <string>
#include "Config.h"

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
    string get_topic();

private:
    Config *config;
    // whether grouping is required
    // grouping incurs extra parsing cost
    bool require_router_grouping;
    bool require_peer_grouping;

    // if there is a router group match, we save it
    // otherwise, it should be null.
    string router_group;
};

class CollectorTopicConfig {
};

class RouterTopicConfig {
};

class BMPTopicConfig {
};

#endif //OPENBMP_TOPICBUILDER_H
