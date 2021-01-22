#include <iostream>
#include <unistd.h>

#include "MessageBus.h"

using namespace std;



/*************************************/
/******* singleton message bus *******/
/*************************************/
MessageBus *MessageBus::singleton_instance = nullptr;

MessageBus *MessageBus::get_message_bus() {
    if (!singleton_instance) {
        cout << "initialize message bus before calling get_message_bus()." << endl;
        exit(1);
    }
    return singleton_instance;
};

MessageBus *MessageBus::init() {
    if (!singleton_instance)   // Only allow one instance of class to be generated.
        singleton_instance = new MessageBus();
    return singleton_instance;
}

// private constructor
MessageBus::MessageBus() {
    logger = Logger::get_logger();
    config = Config::get_config();
    producer_config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    is_connected = false;
    producer = nullptr;
    running = true;
}

MessageBus::~MessageBus() {
    disconnect();
    delete singleton_instance;
    delete producer_config;
    delete event_callback;
}

void MessageBus::send(std::string &topic, uint8_t *encapsulated_msg, int msg_len,
                      const void * key, size_t key_len, int64_t timestamp) {
    while (!is_connected && running) {
        LOG_WARN("Not connected to Kafka, attempting to reconnect");
        connect();
        sleep(1);
    }
    if (!running) {
        return;
    }
    RdKafka::ErrorCode resp =
            producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                              RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                              encapsulated_msg, msg_len, /* Value */
                              key, key_len, /* Key */
                              timestamp, /* Timestamp */
                              nullptr, /* Message headers, if any */
                              nullptr);
    if (resp != RdKafka::ERR_NO_ERROR) {
        std::cerr << "% Produce failed: " <<
                  RdKafka::err2str(resp) << std::endl;
    }
    producer->poll(0);
}

void MessageBus::connect() {
    string errstr;
    std::ostringstream rx_bytes, tx_bytes, sess_timeout, socket_timeout;
    std::ostringstream q_buf_max_msgs, q_buf_max_kbytes, q_buf_max_ms,
            msg_send_max_retry, retry_backoff_ms;

    disconnect();

    // pass librdkafka configs from the config file to producer producer_config
    for (auto & it : config->librdkafka_passthrough_configs) {
        if (producer_config->set(it.first, it.second, errstr) != RdKafka::Conf::CONF_OK) {
            LOG_ERR("Failed to configure kafka producer.");
            throw "ERROR: Failed to configure kafka producer";
        }
    }

    // Register event callback
    if (event_callback == nullptr) {
        event_callback = new KafkaEventCallback(&is_connected, logger);
    }
    if (producer_config->set("event_cb", event_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka event callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka event callback";
    }

    // Create producer
    producer = RdKafka::Producer::create(producer_config, errstr);
    if (producer == nullptr) {
        LOG_ERR("Failed to create producer: %s", errstr.c_str());
        throw "ERROR: Failed to create producer";
    }

    is_connected = true;

    // check if the connection is up
    producer->poll(1000);

    if (not is_connected) {
        LOG_ERR("Failed to connect to Kafka, will try again in a few");
        return;
    }
}

void MessageBus::disconnect() {
    if (is_connected) {
        int i = 0;
        while (producer->outq_len() > 0 and i < 8) {
            LOG_INFO("Waiting for producer to finish before disconnecting: outq=%d", producer->outq_len());
            producer->poll(500);
            i++;
        }
    }
    if (producer != nullptr) {
        delete producer;
    }
    producer = nullptr;

    // suggested by librdkafka to free memory
    RdKafka::wait_destroyed(500);

    is_connected = false;
}

void MessageBus::stop() {
    running = false;
}

