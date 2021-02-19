#ifndef OPENBMP_MESSAGEBUS_H
#define OPENBMP_MESSAGEBUS_H

#include <librdkafka/rdkafkacpp.h>
#include <librdkafka/rdkafka.h>
#include "Logger.h"
#include "Config.h"


// define kafka event call back
class KafkaEventCallback : public RdKafka::EventCb {
public:
    /**
     * Constructor for callback
     *
     * \param isConnected[in,out]   Pointer to isConnected bool to indicate if connected or not
     * \param logPtr[in]            Pointer to the Logger class to use for logging
     */
    KafkaEventCallback(bool *isConnectedRef, Logger *logPtr) {
        isConnected = isConnectedRef;
        logger = logPtr;
    }

    void event_cb (RdKafka::Event &event) {
        switch (event.type())
        {
            case RdKafka::Event::EVENT_ERROR:
                LOG_ERR("Kafka error: %s", RdKafka::err2str(event.err()).c_str());

                if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN) {
                    LOG_ERR("Kafka all brokers down: %s", RdKafka::err2str(event.err()).c_str());
                    *isConnected = false;
                }
                if (event.err() == RdKafka::ERR__TRANSPORT) {
                    LOG_ERR("Kafka transport error: %s", RdKafka::err2str(event.err()).c_str());
                    *isConnected = false;
                }
                break;

            case RdKafka::Event::EVENT_STATS:
                LOG_INFO("Kafka stats: %s", event.str().c_str());
                break;

            case RdKafka::Event::EVENT_LOG: {
                switch (event.severity()) {
                    case RdKafka::Event::EVENT_SEVERITY_EMERG:
                    case RdKafka::Event::EVENT_SEVERITY_ALERT:
                    case RdKafka::Event::EVENT_SEVERITY_CRITICAL:
                    case RdKafka::Event::EVENT_SEVERITY_ERROR:
                        // rdkafka will reconnect, so no need to change at this phase
                        LOG_ERR("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                                event.str().c_str());
                        break;
                    case RdKafka::Event::EVENT_SEVERITY_WARNING:
                        LOG_WARN("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                                 event.str().c_str());
                        break;
                    case RdKafka::Event::EVENT_SEVERITY_NOTICE:
                        LOG_NOTICE("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                                   event.str().c_str());
                        break;
                    default:
                        LOG_INFO("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                                 event.str().c_str());
                        break;
                }
                break;
            }
            default:
                LOG_INFO("Kafka event type = %d (%s) %s", event.type(), RdKafka::err2str(event.err()).c_str(),
                         event.str().c_str());
                break;
        }
    }

private:
    Logger *logger;
    bool   *isConnected;           // Indicates if connected to the broker or not.
};

class MessageBus {
public:
    /*********************************************************************//*
     * Singleton class:
     * librdkafka is thread safe,
     * we only need one global instance
     ***********************************************************************/
    // initialize singleton MessageBus
    static MessageBus *init();

    // get msg bus
    static MessageBus *get_message_bus();

    // delete methods that cause problems to the singleton
    MessageBus(MessageBus const &) = delete;

    void operator=(MessageBus const &) = delete;

    // destructor
    ~MessageBus();

    // send openbmp msg to kafka
    void send(std::string &topic, uint8_t *encapsulated_msg, int msg_len,
              const void * key = nullptr, size_t key_len = 0, int64_t timestamp = 0);

    void connect();

    void disconnect();

    void stop();

private:
    Logger *logger;
    Config *config;

    // private constructor for singleton design
    explicit MessageBus();

    static MessageBus *singleton_instance;

    // indicates if it has connected to Kafka server
    bool is_connected;
    bool running;

    // RdKafka variables
    KafkaEventCallback* event_callback = nullptr;
    RdKafka::Conf *producer_config;
    RdKafka::Producer *producer; // Kafka Producer instance

};


#endif //OPENBMP_MESSAGEBUS_H
