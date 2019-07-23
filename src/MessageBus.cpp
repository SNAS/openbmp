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
    conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    connect();
}

MessageBus::~MessageBus() {
    disconnect();
    delete conf;
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
                                /* Per-message opaque value passed to
                                 * delivery report */
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
    if (conf->set("log.connection.close", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure log.connection.close=false: %s.", errstr.c_str());
    }

    value = "true";
    if (conf->set("api.version.request", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure api.version.request=true: %s.", errstr.c_str());
    }

    // TODO: Add config for address family - default is any
    /*value = "v4";
    if (conf->set("broker.address.family", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker.address.family: %s.", errstr.c_str());
    }*/


    // Batch message number
    value = "100";
    if (conf->set("batch.num.messages", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure batch.num.messages for kafka: %s.", errstr.c_str());
        throw "ERROR: Failed to configure kafka batch.num.messages";
    }

    // Batch message max wait time (in ms)
    q_buf_max_ms << config->q_buf_max_ms;
    if (conf->set("queue.buffering.max.ms", q_buf_max_ms.str(), errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure queue.buffering.max.ms for kafka: %s.", errstr.c_str());
        throw "ERROR: Failed to configure kafka queue.buffer.max.ms";
    }


    // compression
    value = config->compression;
    if (conf->set("compression.codec", value, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure %s compression for kafka: %s.", value.c_str(), errstr.c_str());
        throw "ERROR: Failed to configure kafka compression";
    }

    // broker list
    if (conf->set("metadata.broker.list", config->kafka_brokers, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure broker list for kafka: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka broker list";
    }

    // Maximum transmit byte size
    tx_bytes << config->tx_max_bytes;
    if (conf->set("message.max.bytes", tx_bytes.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure transmit max message size for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure transmit max message size";
    }

    // Maximum receive byte size
    rx_bytes << config->rx_max_bytes;
    if (conf->set("receive.message.max.bytes", rx_bytes.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure receive max message size for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure receive max message size";
    }

    // Client group session and failure detection timeout
    sess_timeout << config->session_timeout;
    if (conf->set("session.timeout.ms", sess_timeout.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure session timeout for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure session timeout ";
    }

    // Timeout for network requests
    socket_timeout << config->socket_timeout;
    if (conf->set("socket.timeout.ms", socket_timeout.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure socket timeout for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure socket timeout ";
    }

    // Maximum number of messages allowed on the producer queue
    q_buf_max_msgs << config->q_buf_max_msgs;
    if (conf->set("queue.buffering.max.messages", q_buf_max_msgs.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure max messages in buffer for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure max messages in buffer ";
    }

    // Maximum number of messages allowed on the producer queue
    q_buf_max_kbytes << config->q_buf_max_kbytes;
    if (conf->set("queue.buffering.max.kbytes", q_buf_max_kbytes.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure max kbytes in buffer for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure max kbytes in buffer ";
    }


    // How many times to retry sending a failing MessageSet
    msg_send_max_retry << config->msg_send_max_retry;
    if (conf->set("message.send.max.retries", msg_send_max_retry.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure max retries for sending "
                "failed message for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure max retries for sending failed message";
    }

    // Backoff time in ms before retrying a message send
    retry_backoff_ms << config->retry_backoff_ms;
    if (conf->set("retry.backoff.ms", retry_backoff_ms.str(),
                  errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure backoff time before retrying to send"
                "failed message for kafka: %s",
                errstr.c_str());
        throw "ERROR: Failed to configure backoff time before resending"
              " failed messages ";
    }

    // Register event callback
    //event_callback = new KafkaEventCallback(&isConnected, logger);
    //if (conf->set("event_cb", event_callback, errstr) != RdKafka::Conf::CONF_OK) {
    //    LOG_ERR("Failed to configure kafka event callback: %s", errstr.c_str());
    //    throw "ERROR: Failed to configure kafka event callback";
    //}

    // Register delivery report callback
    /*
    delivery_callback = new KafkaDeliveryReportCallback();
    if (conf->set("dr_cb", delivery_callback, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERR("Failed to configure kafka delivery report callback: %s", errstr.c_str());
        throw "ERROR: Failed to configure kafka delivery report callback";
    }
    */


    // Create producer and connect
    producer = RdKafka::Producer::create(conf, errstr);
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

