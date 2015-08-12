/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/**
 * Structure that defines the global configuration options
 */
struct Cfg_Options {
    u_char c_hash_id[16];            ///< Collector Hash ID (raw format)
    char   admin_id[64];             ///< Admin ID

    char   *kafka_brokers;
    char   *bmp_port;

    int    bmp_buffer_size;          ///< BMP buffer size in bytes (min is 2M max is 128M)
    bool   svr_ipv4;                 ///< Indicates if server should listen for IPv4 connections
    bool   svr_ipv6;                 ///< Indicates if server should listen for IPv6 connections

    bool   debug_bgp;
    bool   debug_bmp;
    bool   debug_msgbus;

    int    heartbeat_interval;      ///< Heartbeat interval in seconds for collector updates
};



#endif /* CONFIG_H_ */
