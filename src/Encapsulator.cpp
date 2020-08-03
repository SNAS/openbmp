#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/time.h>
#include "Constant.h"
#include "Encapsulator.h"


// constructor for collector msgs
Encapsulator::Encapsulator() {
    logger = Logger::get_logger();
    config = Config::get_config();

    // initialize the openbmp msg buffer
    bin_hdr_buffer = (uint8_t *) calloc(ENCAPSULATOR_BUF_SIZE, sizeof(uint8_t));

    // calculate the binary header size
    int collect_name_len = strlen((const char *) config->collector_name);

    // calculate collector binary header size
    binary_hdr_len_collector = collect_name_len + timestamp_usec_pos
            + BINARY_HDR_TIMESTAMP_USEC_SIZE + BINARY_HDR_COLLECTOR_HASH_SIZE
            + BINARY_HDR_COLLECTOR_NAME_LEN_SIZE + BINARY_HDR_ROW_COUNT_SIZE;

    // collector bin hdr is simple, we fill out the shared part
    uint8_t* current_buff_pos = fill_common_bin_header();

    // we set the row count to 0 as it's a collector msg.
    uint32_t u32 = htonl(0); // TODO: verify if this is the right value to set.
    memcpy(current_buff_pos, &u32, sizeof(u32));

}

// constructor for bmp_raw msgs
Encapsulator::Encapsulator(uint8_t *router_ip, bool is_router_ipv4, string &router_group) {
    logger = Logger::get_logger();
    config = Config::get_config();

    // initialize the openbmp msg buffer
    bin_hdr_buffer = (uint8_t *) calloc(ENCAPSULATOR_BUF_SIZE, sizeof(uint8_t));

    // calculate the binary header size
    int collector_name_len = strlen((const char *) config->collector_name);
    int router_group_len = 0;
    if (router_group != ROUTER_GROUP_UNDEFINED_STRING)
        router_group_len = router_group.size();

    binary_hdr_len_raw_bmp = collector_name_len + router_group_len
                             + timestamp_usec_pos + BINARY_HDR_TIMESTAMP_USEC_SIZE
                             + BINARY_HDR_COLLECTOR_HASH_SIZE + BINARY_HDR_COLLECTOR_NAME_LEN_SIZE
                             + BINARY_HDR_ROUTER_HASH_SIZE + BINARY_HDR_ROUTER_IP_SIZE
                             + BINARY_HDR_ROUTER_GROUP_LEN_SIZE + BINARY_HDR_ROW_COUNT_SIZE;

    uint8_t* current_buff_pos = fill_common_bin_header();

    /*
     * Start to fill hdr buffer
     */
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;

    // set router flag to indicate that router fields are filled
    u8 = 0;
    u8 |= 0x80;
    // set ip type bit if router uses ipv6
    if (!is_router_ipv4) {
        u8 |= 0x40;
    }
    memcpy(bin_hdr_buffer + encap_flag_pos, &u8, sizeof(u8));


    // set msg type to bmp_raw
    u8 = BINARY_HDR_MSG_TYPE_BMP_RAW;
    memcpy(bin_hdr_buffer + encap_msg_type_pos, &u8, sizeof(u8));

    // Router Hash
    this->set_router_hash_id(dynamic_cast<char *>(router_ip));
    memcpy(buf, router_hash_id, sizeof(router_hash_id));
    current_buff_pos += 16;

    // Router IP
    memcpy(current_buff_pos, router_ip, 16);
    current_buff_pos += 16;

    // Router Group Name
    // Len
    u16 = htons(router_group_len);
    memcpy(current_buff_pos, &u16, sizeof(u16));
    current_buff_pos += sizeof(u16);
    // Name (could be zero-length)
    memcpy(current_buff_pos, router_group.c_str(), router_group_len);
    current_buff_pos += router_group_len;

    // Row Count
    u32 = htonl(1); // value is always 1 for raw bmp msgs.
    memcpy(current_buff_pos, &u32, sizeof(u32));
}

Encapsulator::~Encapsulator() {
    delete bin_hdr_buffer;
}

void Encapsulator::build_encap_bmp_msg(uint8_t *bmp_msg, int msg_len) {
    // we do not overwrite the openbmp hdr section in the buffer
    // the hdr shouldn't change much.
    // we may need to change the msg len field in the hdr tho.
    bmp_msg_len = msg_len;
    build_bin_header_raw_bmp();
    memcpy(bin_hdr_buffer + binary_hdr_len_raw_bmp, bmp_msg, bmp_msg_len);
}

uint8_t *Encapsulator::get_encap_bmp_msg() {
    return bin_hdr_buffer;
}

size_t Encapsulator::get_encap_bmp_msg_size() {
    return binary_hdr_len_raw_bmp + bmp_msg_len;
}

void * Encapsulator::get_router_hash_id() {
    return dynamic_cast<void *>(this->router_hash_id);
}

void Encapsulator::set_router_hash_id(char * router_ip) {
    MD5(router_ip, strlen(router_ip), this->router_hash_id);
    static_assert(sizeof(router_hash_id) == 16, "Raw router hash is assumed to be 16 bytes long");
}

void Encapsulator::build_bin_header_raw_bmp() {
    // requires 3 rewrites:
    // 1. timestamps (2 places)
    // 2. bmp msg len (if obj type is not raw_bmp then it is 0)

    uint32_t u32;

    // record the capture time for this message
    timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t capture_ts_secs = tv.tv_sec;
    u32 = htonl(capture_ts_secs);
    memcpy(bin_hdr_buffer + timestamp_sec_pos, &u32, sizeof(u32));
    uint32_t capture_ts_usecs = tv.tv_usec;
    u32 = htonl(capture_ts_usecs);
    memcpy(bin_hdr_buffer + timestamp_usec_pos, &u32, sizeof(u32));

    // set bmp msg len
    u32 = htonl(bmp_msg_len);
    memcpy(bin_hdr_buffer + bmp_msg_len_pos, &u32, sizeof(u32));

}

uint8_t* Encapsulator::fill_common_bin_header() {
    uint8_t* current_buff_pos = bin_hdr_buffer;
    int collect_name_len = strlen((const char *) config->collector_name);
    /*
     * Start to fill hdr buffer
     */
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;

    // Magic Number
    u32 = htonl(BINARY_HDR_MAGIC_NUMBER);
    memcpy(current_buff_pos, &u32, sizeof(u32));
    current_buff_pos += sizeof(u32);

    // Version (major and minor)
    u8 = BINARY_HDR_MAJOR_VERSION;
    memcpy(current_buff_pos, &u8, sizeof(u8));
    current_buff_pos++;
    u8 = BINARY_HDR_MINOR_VERSION;
    memcpy(current_buff_pos, &u8, sizeof(u8));
    current_buff_pos++;

    // binary header length
    u16 = htons(binary_hdr_len_raw_bmp);
    memcpy(current_buff_pos, &u16, sizeof(u16));
    current_buff_pos += sizeof(u16);

    // data length (in this case, raw bmp msg)  (skip)
    current_buff_pos += sizeof(u32);

    // flags
    // we set the flag to indicate that the binary header will not include router info
    // the constructor for bmp_msgs will change the flag.
    u8 = 0;
    memcpy(current_buff_pos, &u8, sizeof(u8));
    current_buff_pos++;

    // Message type
    // u8 = BINARY_HDR_MSG_TYPE_BMP_RAW;
    memcpy(current_buff_pos + encap_msg_type_pos, &u8, sizeof(u8));
    current_buff_pos++;

    // Timestamps (skip twice)
    current_buff_pos += sizeof(u32);
    current_buff_pos += sizeof(u32);

    // Collector Hash
    static_assert(sizeof(config->collector_hash_id) == 16, "Raw collector hash is assumed to be 16 bytes long");
    memcpy(current_buff_pos, config->collector_hash_id, sizeof(config->collector_hash_id));
    current_buff_pos += sizeof(config->collector_hash_id);

    // Collector Admin ID
    // Len
    u16 = htons(collect_name_len);
    memcpy(current_buff_pos, &u16, sizeof(u16));
    current_buff_pos += sizeof(u16);
    // Name (could be zero-length)
    memcpy(current_buff_pos, config->collector_name, collect_name_len);
    current_buff_pos += collect_name_len;

    return current_buff_pos;
}

size_t Encapsulator::get_encap_collector_msg_size() {
    return binary_hdr_len_collector;
}

void Encapsulator::build_encap_collector_msg() {
    // simply update the timestamps

    // record the capture time for this message
    uint32_t u32;
    timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t capture_ts_secs = tv.tv_sec;
    u32 = htonl(capture_ts_secs);
    memcpy(bin_hdr_buffer + timestamp_sec_pos, &u32, sizeof(u32));
    uint32_t capture_ts_usecs = tv.tv_usec;
    u32 = htonl(capture_ts_usecs);
    memcpy(bin_hdr_buffer + timestamp_usec_pos, &u32, sizeof(u32));

}

uint8_t *Encapsulator::get_encap_collector_msg() {
    return bin_hdr_buffer;
}



