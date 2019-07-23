#ifndef OPENBMP_MESSAGEBUS_H
#define OPENBMP_MESSAGEBUS_H

#include <librdkafka/rdkafkacpp.h>
#include <librdkafka/rdkafka.h>


class MessageBus {
public:
    // constructor
    MessageBus();
    // destructor
    ~MessageBus();
    // send openbmp msg to kafka
    void send(uint8_t* encapsulated_msg, int msg_len);
    void connect();
    void disconnect();
private:
    // indicates if it has connected to Kafka server
    bool is_connected;

    // RdKafka variables
    RdKafka::Conf   *conf;
    RdKafka::Producer *producer; // Kafka Producer instance


};


#endif //OPENBMP_MESSAGEBUS_H
