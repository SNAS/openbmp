//
// Created by Lumin Shi on 2019-08-08.
//

#ifndef OPENBMP_UTILITY_H
#define OPENBMP_UTILITY_H

#include <string>
#include <netdb.h>

using namespace std;

class Utility {
public:
    static string resolve_ip(const string& ip) {
        string hostname;
        addrinfo *ai;
        char host[255];

        if (!getaddrinfo(ip.c_str(), nullptr, nullptr, &ai)) {
            if (!getnameinfo(ai->ai_addr,ai->ai_addrlen, host, sizeof(host), nullptr, 0, NI_NAMEREQD)) {
                hostname.assign(host);
            }
            freeaddrinfo(ai);
        }
        return hostname;
    }
};
#endif //OPENBMP_UTILITY_H
