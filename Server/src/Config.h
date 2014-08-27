/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
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
    char *password;
    char *username;
    char *dbURL;
    char *dbName;
    char *bmp_port;

    bool debug_bgp;
    bool debug_bmp;
    bool debug_mysql;
};



#endif /* CONFIG_H_ */
