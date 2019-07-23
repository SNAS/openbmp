#include "MessageBus.h"
#include <librdkafka/rdkafkacpp.h>
#include <thread>
#include <librdkafka/rdkafka.h>

MessageBus::MessageBus() {
    // conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    if (producer == NULL) {
        // TODO: call logger here
    }

}

MessageBus::~MessageBus() {

}

void MessageBus::send(uint8_t *encapsulated_msg, int msg_len) {

}

void MessageBus::connect() {
    // TODO: read variables from server configs
    std::string errstr;

    // try to connect to kafka server
    // producer = RdKafka::Producer::create(conf, errstr);
}

void MessageBus::disconnect() {

}
