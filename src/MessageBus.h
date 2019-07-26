#ifndef OPENBMP_MESSAGEBUS_H
#define OPENBMP_MESSAGEBUS_H

#include <librdkafka/rdkafkacpp.h>
#include <librdkafka/rdkafka.h>
#include "Logger.h"
#include "Config.h"

class MessageBus {
public:
    /*********************************************************************//*
     * Singleton class:
     * librdkafka is thread safe,
     * we only need one global instance
     ***********************************************************************/
    // initialize singleton MessageBus
    static MessageBus *init(Config *);

    // get msg bus
    static MessageBus *get_msg_bus();

    // delete methods that cause problems to the singleton
    MessageBus(MessageBus const &) = delete;

    void operator=(MessageBus const &) = delete;

    // destructor
    ~MessageBus();

    // send openbmp msg to kafka
    void send(uint8_t *encapsulated_msg, int msg_len);

    void connect();

    void disconnect();

private:
    Logger *logger;
    Config *config;

    // private constructor for singleton design
    explicit MessageBus(Config *c);

    static MessageBus *singleton_instance;

    // indicates if it has connected to Kafka server
    bool is_connected;
    string topic = "bmp_raw";

    // RdKafka variables
    RdKafka::Conf *producer_config;
    RdKafka::Producer *producer; // Kafka Producer instance


};


#endif //OPENBMP_MESSAGEBUS_H
