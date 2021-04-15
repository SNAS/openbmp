/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_PARSER_H
#define OPENBMP_PARSER_H

extern "C" {
#include "parsebgp.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
}
#include <netinet/in.h>
#include "Logger.h"
#include "Constant.h"
#include "Config.h"

class Parser {
public:
    // (de)constructor
    Parser();
    ~Parser();

    // parsing requires bmp data buffer and the buffer len
    // it returns how many bytes were read to parse a message
    // it returns negative number when it needs to refill buffer.
    parsebgp_error_t parse(uint8_t *buffer, int buffer_len);

    // returns parse bmp msg
    parsebgp_bmp_msg_t* get_parsed_bmp_msg();
    // returns how many bytes were required to parse the message
    int get_raw_bmp_msg_len();

    void get_peer_ip(string& save_to_string);
    uint32_t get_peer_asn();


private:
    Logger *logger;
    Config *config;
    bool debug;
    // libparsebgp configurations
    parsebgp_opts_t opts;
    parsebgp_msg_type_t type = PARSEBGP_MSG_TYPE_BMP;
    parsebgp_error_t err = PARSEBGP_OK;

    // this var holds parsed bmp message
    parsebgp_msg_t *parsed_msg;

    // save how many bytes were read from the buffer; the buffer given to the parse()
    size_t read_len = 0;
};


#endif //OPENBMP_PARSER_H
