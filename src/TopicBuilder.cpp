//
// Created by Lumin Shi on 2019-07-29.
//

#include "TopicBuilder.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <boost/algorithm/string/replace.hpp>
#include <netdb.h>

TopicBuilder::TopicBuilder() {
    config = Config::get_config();
    debug = config->debug_all;
    logger = Logger::get_logger();

    // check what variables are required in collector kafka topic template
    // build collector topic string
    collector_topic_string = config->topic_template_collector;
    if (config->topic_template_collector.find("{{collector_group}}") != string::npos) {
        boost::replace_all(collector_topic_string, "{{collector_group}}", config->collector_group);
    }
    if (config->topic_template_collector.find("{{collector_name}}") != string::npos) {
        string tmp_collector_name((char*) config->collector_name, strlen((char *)(config->collector_name)));
        boost::replace_all(collector_topic_string, "{{collector_name}}", tmp_collector_name);
    }

    // check what variables are required in router kafka topic template
    router_topic_string = config->topic_template_router;
    if (config->topic_template_router.find("{{collector_group}}") != string::npos) {
        boost::replace_all(router_topic_string, "{{collector_group}}", config->collector_group);
    }
    if (config->topic_template_router.find("{{collector_name}}") != string::npos){
        string tmp_collector_name((char*) config->collector_name, strlen((char *)(config->collector_name)));
        boost::replace_all(router_topic_string, "{{collector_name}}", tmp_collector_name);
    }
    if (config->topic_template_router.find("{{router_group}}") != string::npos)
        router_topic_requires_router_group = true;
    if (config->topic_template_router.find("{{router_hostname}}") != string::npos)
        router_topic_requires_router_hostname = true;
    if (config->topic_template_router.find("{{router_ip}}") != string::npos)
        router_topic_requires_router_ip = true;

    // check what variables are required in bmp_raw kafka topic template
    if (config->topic_template_bmp_raw.find("{{collector_group}}") != string::npos)
        raw_bmp_topic_requires_collector_group = true;
    if (config->topic_template_bmp_raw.find("{{collector_name}}") != string::npos)
        raw_bmp_topic_requires_collector_name = true;
    if (config->topic_template_bmp_raw.find("{{router_group}}") != string::npos)
        raw_bmp_topic_requires_router_group = true;
    if (config->topic_template_bmp_raw.find("{{router_hostname}}") != string::npos)
        raw_bmp_topic_requires_router_hostname = true;
    if (config->topic_template_bmp_raw.find("{{router_ip}}") != string::npos)
        raw_bmp_topic_requires_router_ip = true;
    if (config->topic_template_bmp_raw.find("{{peer_group}}") != string::npos)
        raw_bmp_topic_requires_peer_group = true;
    if (config->topic_template_bmp_raw.find("{{peer_asn}}") != string::npos)
        raw_bmp_topic_requires_peer_asn = true;
    if (config->topic_template_bmp_raw.find("{{peer_ip}}") != string::npos)
        raw_bmp_topic_requires_peer_ip = true;

}

void TopicBuilder::find_router_group(const string& ip_addr, const string& router_hostname) {
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
    bool isIPv4 = ip_addr.find_first_of(':') == string::npos;
    uint8_t bits;

    uint32_t prefix[4]  __attribute__ ((aligned));
    bzero(prefix, sizeof(prefix));

    inet_pton(isIPv4 ? AF_INET : AF_INET6, ip_addr.c_str(), prefix);

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
                    SELF_DEBUG("IP %s matched router group %s", ip_addr.c_str(), it.first.c_str());
                    router_group = it.first;
                    return;
                }
            } else { // IPv6
                uint8_t end_idx = pit.bits / 32;
                bits = pit.bits - (32 * end_idx);

                if (bits == 0)
                    end_idx--;

                if (end_idx < 4 and bits < 32) {    // end_idx should be less than 4 and bits less than 32

                    // Big endian
                    prefix[end_idx] <<= bits;
                    prefix[end_idx] >>= bits;
                }

                if (prefix[0] == pit.prefix[0] and prefix[1] == pit.prefix[1]
                    and prefix[2] == pit.prefix[2] and prefix[3] == pit.prefix[3]) {

                    SELF_DEBUG("IP %s matched router group %s", ip_addr.c_str(), it.first.c_str());
                    router_group = it.first;
                    return;
                }
            }
        }
    }

    // finally, if no match, we set a default value
    router_group = "default";
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
    if (peer_groups.find(key) != peer_groups.end())
        peer_group_name = peer_groups[key];

    peer_group_name = "";

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

                if (bits == 0)
                    end_idx--;

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
    peer_group_name = "default";
    peer_groups[key] = peer_group_name;
}

string TopicBuilder::get_collector_topic_string() {
    return collector_topic_string;
}

string TopicBuilder::get_router_topic_string(const string& router_ip) {
    if (router_topic_string_initialized)
        return router_topic_string;

    // cache router hostname
    if (router_hostname.empty())
        router_hostname = resolve_ip(router_ip);

    if (router_topic_requires_router_hostname)
        boost::replace_all(router_topic_string, "{{router_hostname}}", router_hostname);
    if (router_topic_requires_router_ip)
        boost::replace_all(router_topic_string, "{{router_ip}}", router_ip);
    if (router_topic_requires_router_group) {
        find_router_group(router_ip, router_hostname);
        boost::replace_all(router_topic_string, "{{router_group}}", router_group);
    }

    // indicate that router topic string has been initialized
    router_topic_string_initialized = true;
    return router_topic_string;
}

string TopicBuilder::resolve_ip(const string &ip) {
    string hostname;
    addrinfo *ai;
    char host[255];

    if (!getaddrinfo(ip.c_str(), nullptr, nullptr, &ai)) {
        if (!getnameinfo(ai->ai_addr,ai->ai_addrlen, host, sizeof(host), nullptr, 0, NI_NAMEREQD)) {
            hostname.assign(host);
            LOG_INFO("resolve: %s to %s", ip.c_str(), hostname.c_str());
        }
        freeaddrinfo(ai);
    }
    return hostname;
}

string TopicBuilder::get_raw_bmp_topic_string(const string &router_ip, const string &peer_ip, uint32_t peer_asn) {
    string key = router_ip + "-" + peer_ip + "-" + to_string(peer_asn);
    // return the topic string if its cached
    if (bmp_raw_topic_strings.find(key) != bmp_raw_topic_strings.end())
        return bmp_raw_topic_strings[key];

    // get bmp_raw topic template to build a new topic
    string bmp_raw_topic_string = config->topic_template_bmp_raw;

    if (raw_bmp_topic_requires_collector_group)
        boost::replace_all(bmp_raw_topic_string, "{{collector_group}}", config->collector_group);
    if (raw_bmp_topic_requires_collector_name) {
        string tmp_collector_name((char*) config->collector_name, strlen((char *)(config->collector_name)));
        boost::replace_all(bmp_raw_topic_string, "{{collector_name}}", tmp_collector_name);
    }

    if (raw_bmp_topic_requires_router_hostname) {
        // cache router hostname
        if (router_hostname.empty())
            router_hostname = resolve_ip(router_ip);
        boost::replace_all(bmp_raw_topic_string, "{{router_hostname}}", router_hostname);
    }
    if (raw_bmp_topic_requires_router_ip)
        boost::replace_all(bmp_raw_topic_string, "{{router_ip}}", router_ip);
    if (raw_bmp_topic_requires_router_group) {
        find_router_group(router_ip, router_hostname);
        boost::replace_all(bmp_raw_topic_string, "{{router_group}}", router_group);
    }

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
            peer_hostname = resolve_ip(peer_ip);
            peer_hostnames[peer_ip] = peer_hostname;
        }

        find_peer_group(peer_hostname, peer_ip, peer_asn, peer_group);
        boost::replace_all(bmp_raw_topic_string, "{{peer_group}}", peer_group);
    }

    // cache topic string
    bmp_raw_topic_strings[key] = bmp_raw_topic_string;

    return std::__cxx11::string();
}

