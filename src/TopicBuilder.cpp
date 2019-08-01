//
// Created by Lumin Shi on 2019-07-29.
//

#include "TopicBuilder.h"

TopicBuilder::TopicBuilder() {

    require_router_grouping = false;
    require_peer_grouping = false;
    /*
     * check topic templates to see if router and/or peer grouping is enabled.
     * if any topic requires a grouping variable, we will have to enable the feature.
     * otherwise, the parsing cost could be reduced.
    */
}
