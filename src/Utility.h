/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_UTILITY_H
#define OPENBMP_UTILITY_H

#include <string>
#include <netdb.h>
#include "stdlib.h"
#include "stdio.h"

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

    static double get_avg_cpu_util() {
        unsigned long long totalUser[2], totalUserLow[2], totalSys[2], totalIdle[2];
        unsigned long long total;
        double percent = 0;
        FILE* file = fopen("/proc/stat", "r");
        fscanf(file, "cpu %llu %llu %llu %llu",
                &totalUser[0], &totalUserLow[0], &totalSys[0], &totalIdle[0]);
        fclose(file);

        sleep(5);

        // sleep for a while and check the cpu util again
        file = fopen("/proc/stat", "r");
        fscanf(file, "cpu %llu %llu %llu %llu",
               &totalUser[1], &totalUserLow[1], &totalSys[1], &totalIdle[1]);
        fclose(file);

        if (totalUser[1] < totalUser[0] || totalUserLow[1] < totalUserLow[0] ||
            totalSys[1] < totalSys[0] || totalIdle[1] < totalIdle[0]){
            //Overflow detection. Just skip this value.
            percent = -1.0;
        } else {
            total = (totalUser[1] - totalUser[0])
                    + (totalUserLow[1] - totalUserLow[0])
                    + (totalSys[1] - totalSys[0]);
            percent = total;
            total += (totalIdle[1] - totalIdle[0]);
            percent /= total;
            percent *= 100;
        }
        return percent;
    }
};

#endif //OPENBMP_UTILITY_H
