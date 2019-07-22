#ifndef OPENBMP_ENCAPSULATOR_H
#define OPENBMP_ENCAPSULATOR_H


#include <cstdint>

struct {


} encap_msg;

class Encapsulator {
public:
    Encapsulator();
    void build();
    uint8_t* get_encapsulated_msg();
    int get_encapsulated_msg_size();
private:
    uint8_t msg_buffer[100];
    int bmp_msg_len;
};


#endif //OPENBMP_ENCAPSULATOR_H
