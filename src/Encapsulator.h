#ifndef OPENBMP_ENCAPSULATOR_H
#define OPENBMP_ENCAPSULATOR_H


#include <cstdint>

#define MIN_BIN_HDR_SIZE 40;
#define MAX_BGP_MSG_SIZE 4096;
/** The "magic number" prefixed to all binary headers ("OBMP") */
#define MSGBUS_BIN_HDR_MAGIC 0x4F424D50


class Encapsulator {
public:
    Encapsulator();

    void build(uint8_t* bmp_msg, int bmp_msg_len);
    void build_header();

    uint8_t* get_encapsulated_msg();
    int get_encapsulated_msg_size();

private:
    bool is_header_built = false;
    uint8_t* openbmp_msg_buffer;

    // the len of header;
    int openbmp_hdr_len;
    // the len of bmp msg;
    int bmp_msg_len;

};


#endif //OPENBMP_ENCAPSULATOR_H
