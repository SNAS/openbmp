#ifndef OPENBMP_ENCAPSULATOR_H
#define OPENBMP_ENCAPSULATOR_H

#include "Constant.h"
#include "Logger.h"
#include "Config.h"
#include <cstdint>
#include <string>

using namespace std;

class Encapsulator {
public:
    // constructor for collector msgs
    Encapsulator();
    // constructor for bmp_raw msgs
    Encapsulator(uint8_t *router_ip, bool is_router_ipv4, string &router_hostname, string &router_group);

    // functions for raw bmp msgs
    void build_encap_bmp_msg(uint8_t* bmp_msg, int bmp_msg_len);
    uint8_t* get_encap_bmp_msg();
    size_t get_encap_bmp_msg_size();

    // TODO: functions for collector msgs

    // TODO: functions for router msgs

private:
    Logger *logger;
    Config *config;

    uint8_t* bin_hdr_buffer;

    // len of last processed bmp msg;
    int bmp_msg_len = 0;

    // len of binary hdr for raw bmp msgs
    size_t binary_hdr_len_raw_bmp = 0;
    // len of binary hdr for collector msgs
    size_t binary_hdr_len_collector = 0;

    // bmp_msg_len pos for rewrite
    int bmp_msg_len_pos = BINARY_HDR_MAGIC_NUMBER_SIZE + BINARY_HDR_MAJOR_VERSION_SIZE
                          + BINARY_HDR_MINOR_VERSION_SIZE + BINARY_HDR_HDR_LEN_SIZE;;
    // encap flag pos for rewrite
    int encap_flag_pos = bmp_msg_len_pos + BINARY_HDR_BMP_MSG_LEN_SIZE;
    // encap msg type pos for rewrite
    int encap_msg_type_pos = encap_flag_pos + BINARY_HDR_FLAG_SIZE;
    // timestamp positions for rewriting  collector timestamp for each bmp msg.
    int timestamp_sec_pos = encap_msg_type_pos + BINARY_HDR_TYPE_SIZE;
    int timestamp_usec_pos = timestamp_sec_pos + BINARY_HDR_TIMESTAMP_SEC_SIZE;

    // fill information that shared by both collector and raw_bmp msgs.
    uint8_t* fill_common_bin_header();
    // for each raw bmp msg, we call this function to modify the raw_bmp bin header
    void build_bin_header_raw_bmp();
};

/*
 * V2 Binary Header Format
 * Magic Number (0x4F424D50)
 * Major Ver
 * Minor Ver
 * Binary Header Length
 * Raw BMP Message Length
 * Flags (1. has router fields, 2. router ip v4 or v6)
 * Msg Type (collector or raw_bmp)
 * Collector Timestamp (seconds)
 * Collector Timestamp (microseconds)
 * Collector Hash (16 bytes)
 * Collector Name Length
 * Collector Name (variable)
 * Router Hash (16 bytes)
 * Router IP (16 bytes)
 * Router Group Length
 * Router Group (variable)
 * Row Count (4 bytes)
 */

#endif //OPENBMP_ENCAPSULATOR_H
