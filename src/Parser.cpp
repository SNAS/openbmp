/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include "Parser.h"


Parser::Parser() {
    logger = Logger::get_logger();
    config = Config::get_config();
    debug = (config->debug_all | config->debug_worker);
    parsebgp_opts_init(&opts);
    opts.ignore_not_implemented = 1;
    opts.silence_not_implemented = debug ? 0 : 1;
    // enable shallow parsing
    opts.bmp.parse_headers_only = 1;
    // instantiate libparsebgp msg.
    parsed_msg = parsebgp_create_msg();
}

Parser::~Parser() {
    parsebgp_destroy_msg(parsed_msg);
}

parsebgp_error_t Parser::parse(uint8_t *buffer, int buffer_len) {
    // libparsebgp decode function reads the buffer with bmp_data_unread_len,
    // and modify the following variable (read_len) with the actual number of bytes read.
    read_len = (size_t) buffer_len;
    // clear previously parsed bmp msg.
    parsebgp_clear_msg(parsed_msg);
    // parse bmp data
    err = parsebgp_decode(opts, type, parsed_msg, buffer, &read_len);
    return err;
}

int Parser::get_raw_bmp_msg_len() {
    return (int) read_len;
}

parsebgp_bmp_msg_t *Parser::get_parsed_bmp_msg() {
    return parsed_msg->types.bmp;
}

void Parser::get_peer_ip(string &save_to_string) {
    int mapping[] = {-1, AF_INET, AF_INET6};
    char ip_buf[INET6_ADDRSTRLEN] = "[no_peer_IP]";
    parsebgp_bmp_peer_hdr_t peer_hdr = parsed_msg->types.bmp->peer_hdr;
    if (peer_hdr.afi)
        inet_ntop(mapping[peer_hdr.afi],
                  peer_hdr.addr, ip_buf, INET6_ADDRSTRLEN);
    save_to_string.assign(ip_buf);
}

uint32_t Parser::get_peer_asn() {
    parsebgp_bmp_peer_hdr_t peer_hdr = parsed_msg->types.bmp->peer_hdr;
    return peer_hdr.asn;
}
