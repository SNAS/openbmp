#include <stdlib.h>
#include <cstring>
#include "Encapsulator.h"

Encapsulator::Encapsulator() {
    // initialize the openbmp msg buffer
    int default_buffer_size = 10000;
    openbmp_msg_buffer = (uint8_t*) calloc(default_buffer_size, sizeof(uint8_t));
}

void Encapsulator::build(uint8_t* bmp_msg, int msg_len) {
    // we do not overwrite the openbmp hdr section in the buffer
    // the hdr shouldn't change much.
    // we may need to change the msg len field in the hdr tho.
    bmp_msg_len = msg_len;
    memcpy(openbmp_msg_buffer + openbmp_hdr_len, bmp_msg, msg_len);
}

uint8_t* Encapsulator::get_encapsulated_msg() {
    return openbmp_msg_buffer;
}

int Encapsulator::get_encapsulated_msg_size() {
    return openbmp_hdr_len + bmp_msg_len;
}

void Encapsulator::build_header() {
    // TODO: build the binary header here

    is_header_built = true;
}
