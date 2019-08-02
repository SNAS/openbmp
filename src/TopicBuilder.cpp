//
// Created by Lumin Shi on 2019-07-29.
//

#include "TopicBuilder.h"

TopicBuilder::TopicBuilder() {
    config = Config::get_config();

    /* check topic templates to see if router and/or peer grouping is enabled.
     * if any topic requires a grouping variable, we will have to enable the feature.
     * otherwise, the parsing cost could be reduced. */
    require_router_grouping = false;
    require_peer_grouping = false;
    if ((config->topic_template_router.find("{{router_group}}") != string::npos) |
            (config->topic_template_bmp_raw.find("{{router_group}}") != string::npos))
        require_router_grouping = true;
    if (config->topic_template_bmp_raw.find("{{peer_group}}") != string::npos)
        require_peer_grouping = true;

}
