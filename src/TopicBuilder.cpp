//
// Created by Lumin Shi on 2019-07-29.
//

#include "TopicBuilder.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <boost/algorithm/string/replace.hpp>

TopicBuilder::TopicBuilder(string &router_ip, string &router_hostname) {
    config = Config::get_config();
    debug = config->debug_all;
    logger = Logger::get_logger();

    this->router_ip = router_ip;
    this->router_hostname = router_hostname;
    // build router_group name
    find_router_group();

    // check what variables are required in collector kafka topic template
    // get_encap_msg collector topic string
    collector_topic_string = config->topic_template_collector;
    string tmp_collector_name((char*) config->collector_name, strlen((char *)(config->collector_name)));
    boost::replace_all(collector_topic_string, "{{collector_group}}", config->collector_group);
    boost::replace_all(collector_topic_string, "{{collector_name}}", tmp_collector_name);

    // check what variables are required in router kafka topic template
    router_topic_string = config->topic_template_router;
    boost::replace_all(router_topic_string, "{{collector_group}}", config->collector_group);
    boost::replace_all(router_topic_string, "{{collector_name}}", tmp_collector_name);
    boost::replace_all(router_topic_string, "{{router_hostname}}", router_hostname);
    boost::replace_all(router_topic_string, "{{router_ip}}", router_ip);
    boost::replace_all(router_topic_string, "{{router_group}}", router_group);

    // get bmp_raw topic template and fill collector and router information
    bmp_raw_topic_template = config->topic_template_bmp_raw;
    boost::replace_all(bmp_raw_topic_template, "{{collector_group}}", config->collector_group);
    boost::replace_all(bmp_raw_topic_template, "{{collector_name}}", tmp_collector_name);
    boost::replace_all(bmp_raw_topic_template, "{{router_hostname}}", router_hostname);
    boost::replace_all(bmp_raw_topic_template, "{{router_ip}}", router_ip);
    boost::replace_all(bmp_raw_topic_template, "{{router_group}}", router_group);

    // check what peer variables are required in bmp_raw kafka topic template
    if (bmp_raw_topic_template.find("{{peer_group}}") != string::npos) {
        raw_bmp_topic_requires_peer_group = true;
    }
    if (bmp_raw_topic_template.find("{{peer_asn}}") != string::npos) {
        raw_bmp_topic_requires_peer_asn = true;
    }
    if (bmp_raw_topic_template.find("{{peer_ip}}") != string::npos) {
        raw_bmp_topic_requires_peer_ip = true;
    }

}

void TopicBuilder::find_router_group() {
    if (!router_group.empty()) return;

    if (!router_hostname.empty()) {
        for (const auto &it: config->match_router_group_by_name) {
            for (const auto &rit: it.second) {
                if (regex_search(router_hostname, rit.regexp)) {
                    router_group = it.first;
                    return;
                }
            }
        }
    }

    /*
     * Match against prefix ranges
     */
    bool isIPv4 = router_ip.find_first_of(':') == string::npos;
    uint8_t bits;

    uint32_t prefix[4]  __attribute__ ((aligned));
    bzero(prefix, sizeof(prefix));

    inet_pton(isIPv4 ? AF_INET : AF_INET6, router_ip.c_str(), prefix);

    // Loop through all groups and their regular expressions
    for (const auto &it : config->match_router_group_by_ip) {

        // loop through all prefix ranges to see if there is a match
        for (auto pit: it.second) {
            if (pit.is_ipv4 == isIPv4) { // IPv4
                bits = 32 - pit.bits;
                // Big endian
                prefix[0] <<= bits;
                prefix[0] >>= bits;
                if (prefix[0] == pit.prefix[0]) {
                    SELF_DEBUG("IP %s matched router group %s", router_ip.c_str(), it.first.c_str());
                    router_group = it.first;
                    return;
                }
            } else { // IPv6
                uint8_t end_idx = pit.bits / 32;
                bits = pit.bits - (32 * end_idx);
                if (bits == 0) {
                    end_idx--;
                }
                if (end_idx < 4 and bits < 32) {    // end_idx should be less than 4 and bits less than 32
                    // Big endian
                    prefix[end_idx] <<= bits;
                    prefix[end_idx] >>= bits;
                }
                if (prefix[0] == pit.prefix[0] and prefix[1] == pit.prefix[1]
                    and prefix[2] == pit.prefix[2] and prefix[3] == pit.prefix[3]) {

                    SELF_DEBUG("IP %s matched router group %s", router_ip.c_str(), it.first.c_str());
                    router_group = it.first;
                    return;
                }
            }
        }
    }

    // finally, if no match, we set a default router_group value
    router_group = ROUTER_GROUP_UNDEFINED_STRING;
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
void TopicBuilder::find_peer_group(string hostname, const string& ip_addr, uint32_t peer_asn, string &peer_group_name) {
    string key = hostname + "-" + ip_addr + "-" + to_string(peer_asn);
    // return peer_group_name if its cached.
    auto find_it = peer_groups.find(key);
    if (find_it != peer_groups.end()) {
        peer_group_name = find_it->second;
        return;
    }
    /*
     * Match against hostname regexp
     */
    if (!hostname.empty()) {

        // Loop through all groups and their regular expressions
        for (const auto& it: config->match_peer_group_by_name) {
            // loop through all regexps to see if there is a match
            for (const auto& rit : it.second) {
                if (regex_search(hostname, rit.regexp)) {
                    SELF_DEBUG("Regexp matched hostname %s to peer group '%s'",
                               hostname.c_str(), it.first.c_str());
                    peer_group_name = it.first;
                    return;
                }
            }
        }
    }

    /*
     * Match against prefix ranges
     */
    bool isIPv4 = ip_addr.find_first_of(':') == std::string::npos;
    uint8_t bits;

    uint32_t prefix[4]  __attribute__ ((aligned));
    bzero(prefix, sizeof(prefix));

    inet_pton(isIPv4 ? AF_INET : AF_INET6, ip_addr.c_str(), prefix);

    // Loop through all groups and their regular expressions
    for (const auto& it : config->match_peer_group_by_ip) {
        // loop through all prefix ranges to see if there is a match
        for (auto pit : it.second) {
            if (pit.is_ipv4 == isIPv4) { // IPv4
                bits = 32 - pit.bits;
                // Big endian
                prefix[0] <<= bits;
                prefix[0] >>= bits;

                if (prefix[0] == pit.prefix[0]) {
                    SELF_DEBUG("IP %s matched peer group %s", ip_addr.c_str(), it.first.c_str());
                    peer_group_name = it.first;
                    return;
                }
            } else { // IPv6
                uint8_t end_idx = pit.bits / 32;
                bits = pit.bits - (32 * end_idx);
                if (bits == 0) {
                    end_idx--;
                }
                if (end_idx < 4 and bits < 32) {    // end_idx should be less than 4 and bits less than 32
                    // Big endian
                    prefix[end_idx] <<= bits;
                    prefix[end_idx] >>= bits;
                }
                if (prefix[0] == pit.prefix[0] and prefix[1] == pit.prefix[1]
                    and prefix[2] == pit.prefix[2] and prefix[3] == pit.prefix[3]) {
                    SELF_DEBUG("IP %s matched peer group %s", ip_addr.c_str(), it.first.c_str());
                    peer_group_name = it.first;
                    return;
                }
            }
        }
    }

    /*
     * Match against asn list
     */
    // Loop through all groups and their regular expressions
    for (const auto& it : config->match_peer_group_by_asn) {
        // loop through all prefix ranges to see if there is a match
        for (auto ait : it.second) {
            if (ait == peer_asn) {
                SELF_DEBUG("Peer ASN %u matched peer group %s", peer_asn, it.first.c_str());
                peer_group_name = it.first;
                return;
            }
        }
    }

    // if not match above, assign default peer group value
    peer_group_name = PEER_GROUP_UNDEFINED_STRING;
    // save peer_group_name for faster lookup
    peer_groups[key] = peer_group_name;
}

string TopicBuilder::get_collector_topic_string() {
    return collector_topic_string;
}

string TopicBuilder::get_router_topic_string() {
    return router_topic_string;
}

string TopicBuilder::get_raw_bmp_topic_string(const string &peer_ip, uint32_t peer_asn) {
    string key = peer_ip + to_string(peer_asn);

    // return the topic string if its cached
    auto find_it = bmp_raw_topic_strings.find(key);
    if (find_it != bmp_raw_topic_strings.end())
        return find_it->second;

    // get bmp_raw topic template to get_encap_msg a new topic
    string bmp_raw_topic_string = bmp_raw_topic_template;

    if (raw_bmp_topic_requires_peer_asn) {
        boost::replace_all(bmp_raw_topic_string, "{{peer_asn}}", to_string(peer_asn));
    }
    if (raw_bmp_topic_requires_peer_ip) {
        boost::replace_all(bmp_raw_topic_string, "{{peer_ip}}", peer_ip);
    }
    if (raw_bmp_topic_requires_peer_group) {
        string peer_group;
        string peer_hostname;

        // cache peer hostname
        if (peer_hostnames.find(peer_ip) != peer_hostnames.end()) {
            peer_hostname = peer_hostnames[peer_ip];
        } else {
            peer_hostname = Utility::resolve_ip(peer_ip);
            peer_hostnames[peer_ip] = peer_hostname;
        }

        find_peer_group(peer_hostname, peer_ip, peer_asn, peer_group);
        boost::replace_all(bmp_raw_topic_string, "{{peer_group}}", peer_group);
    }

    // cache topic string
    bmp_raw_topic_strings[key] = bmp_raw_topic_string;

    return bmp_raw_topic_string;
}

string TopicBuilder::get_router_group() {
    return router_group;
}

