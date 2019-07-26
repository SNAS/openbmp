#include <iostream>
#include <unistd.h>

#include "MessageBus.h"

using namespace std;


/*************************************/
/******* singleton message bus *******/
/*************************************/
MessageBus *MessageBus::singleton_instance = nullptr;

MessageBus *MessageBus::get_msg_bus() {
    if (!singleton_instance) {
        cout << "initialize message bus before calling get_msg_bus()." << endl;
        exit(1);
    }
    return singleton_instance;
};

MessageBus *MessageBus::init(Config *c) {
    if (!singleton_instance)   // Only allow one instance of class to be generated.
        singleton_instance = new MessageBus(c);
    return singleton_instance;
}

// private constructor
MessageBus::MessageBus(Config *c) {
    logger = Logger::get_logger();
    config = c;
    producer_config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    connect();
}

MessageBus::~MessageBus() {
    disconnect();
    delete producer_config;
}

void MessageBus::send(uint8_t *encapsulated_msg, int msg_len) {
    while (!is_connected) {
        LOG_WARN("Not connected to Kafka, attempting to reconnect");
        connect();
        sleep(1);
    }

    RdKafka::ErrorCode resp =
            producer->produce(topic, RdKafka::Topic::PARTITION_UA,
                              RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                              encapsulated_msg, msg_len, /* Value */
                              nullptr, 0, /* Key */
                              0, /* Timestamp (defaults to now) */
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
    string value;
    std::ostringstream rx_bytes, tx_bytes, sess_timeout, socket_timeout;
    std::ostringstream q_buf_max_msgs, q_buf_max_kbytes, q_buf_max_ms,
            msg_send_max_retry, retry_backoff_ms;

    disconnect();

    /*
     * Configure Kafka Producer (https://kafka.apache.org/08/configuration.html)
     */
    //TODO: Add config options to change these settings

    // Disable logging of connection close/idle timeouts caused by Kafka 0.9.x (connections.max.idle.ms)
    //    See https://github.com/edenhill/librdkafka/issues/437 for more details.
    // TODO: change this when librdkafka has better handling of the idle disconnects
    value = "false";
    if (producer_config->set("log.connection.close", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure log.connection.close=false: %s.", errstr.c_str());
    }

    value = "true";
    if (producer_config->set("api.version.request", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure api.version.request=true: %s.", errstr.c_str());
    }

    // TODO: Add config for address family - default is any
    /*value = "v4";
    if (producer_config->set("broker.address.family", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker.address.family: %s.", errstr.c_str());
    }*/

    // pass librdkafka configs from the config file to producer producer_config
    for (auto & it : config->librdkafka_passthrough_configs) {
        if (producer_config->set(it.first, it.second, errstr) != RdKafka::Conf::CONF_OK) {
            LOG_ERR("Failed to configure kafka producer.");
            throw "ERROR: Failed to configure kafka producer";
        }
    }

    // Register event callback
    //event_callback = new KafkaEventCallback(&isConnected, logger);
    //if (producer_config->set("event_cb", event_callback, errstr) != RdKafka::Conf::CONF_OK) {
    //    LOG_ERR("Failed to configure kafka event callback: %s", errstr.c_str());
    //    throw "ERROR: Failed to configure kafka event callback";
    //}

    // Register delivery report callback
    /*
    delivery_callback = new KafkaDeliveryReportCallback();
    if (producer_config->set("dr_cb", delivery_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka delivery report callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka delivery report callback";
    }
    */

    // Create producer and connect
    producer = RdKafka::Producer::create(producer_config, errstr);
    if (producer == nullptr) {
        LOG_ERR("Failed to create producer: %s", errstr.c_str());
        throw "ERROR: Failed to create producer";
    }
    is_connected = true;

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

    if (producer != nullptr) delete producer;
    producer = nullptr;

    // suggested by librdkafka to free memory
    RdKafka::wait_destroyed(2000);

    //if (event_callback != NULL) delete event_callback;
    //event_callback = NULL;

    //if (delivery_callback != NULL) delete delivery_callback;
    //delivery_callback = NULL;

    is_connected = false;
}

