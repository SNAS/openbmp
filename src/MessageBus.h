#ifndef OPENBMP_MESSAGEBUS_H
#define OPENBMP_MESSAGEBUS_H

#include <librdkafka/rdkafkacpp.h>


class MessageBus {
public:
private:
    // indicates if it has connected to Kafka server
    bool is_connected;
    RdKafka::Producer *producer; // Kafka Producer instance

};


#endif //OPENBMP_MESSAGEBUS_H
