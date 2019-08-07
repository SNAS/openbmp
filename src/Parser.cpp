//
// Created by Lumin Shi on 2019-08-02.
//

#include <sys/socket.h>
#include "Parser.h"


Parser::Parser() {
    logger = Logger::get_logger();
    config = Config::get_config();
    debug = (config->debug_all | config->debug_worker);
    parsebgp_opts_init(&opts);
    opts.ignore_not_implemented = 1;
    opts.silence_not_implemented = debug ? 0 : 1;
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

int Parser::get_parsed_len() {
    return (int) read_len;
}

parsebgp_msg_t *Parser::get_parsed_bmp_msg() {
    return parsed_msg;
}
