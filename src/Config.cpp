#include <cstring>
#include <iostream>
#include "Config.h"

Config::Config() {
    // Initialize the defaults
    daemon = true;
    bmp_port = 5000;
    debug_general = false;
    debug_bmp = false;
    debug_msgbus = false;
    bmp_ring_buffer_size = 15 * 1024 * 1024; // 15MB
    svr_ipv6 = false;
    svr_ipv4 = true;
    bind_ipv4 = "";
    bind_ipv6 = "";
    kafka_brokers = "localhost:9092";
    tx_max_bytes = 1000000;
    rx_max_bytes = 100000000;
    session_timeout = 30000;    // Default is 30 seconds
    socket_timeout = 60000;    // Default is 60 seconds
    q_buf_max_msgs = 100000;
    q_buf_max_kbytes = 1048576;
    q_buf_max_ms = 1000;         // Default is 1 sec
    msg_send_max_retry = 2;
    retry_backoff_ms = 100;
    compression = "snappy";
    memset(collector_id, 0, sizeof(collector_id));

    // Initialize three topics that OpenBMP V2 produces
    // We initialize the default values here
    // Users can change the values from the config file
    topic_names_map["collector"] = "openbmp.collector";
    topic_names_map["router"] = "openbmp.router";
    topic_names_map["bmp_raw"] = "openbmp.bmp_raw";
}

void Config::load(const char *config_filename) {
    std::cout << "Loading configuration file" << std::endl;

    try {
        YAML::Node root = YAML::LoadFile(config_filename);

        /*
         * Iterate through the root node objects - We expect only maps at the root level, skip others
         */
        if (root.Type() == YAML::NodeType::Map) {
            for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
                const YAML::Node &node = it->second;
                const std::string &key = it->first.Scalar();

                if (node.Type() == YAML::NodeType::Map) {
                    if (key == "base")
                        parse_base(node);
                    else if (key == "debug")
                        parse_debug(node);
                    else if (key == "kafka")
                        parse_kafka(node);
                    else if (debug_general)
                        std::cout << "   Config: Key " << key << " Type " << node.Type() << std::endl;
                }
                else {
                    print_warning("configuration should only have maps at the root/base level found", node);
                }
            }
        } else {
            print_warning("configuration should only have maps at the root/base level found", root);

        }

    } catch (YAML::BadFile err) {
        throw err.what();
    } catch (YAML::ParserException err) {
        throw err.what();
    } catch (YAML::InvalidNode err) {
        throw err.what();
    }

    std::cout << "Done Loading configuration file" << std::endl;
}


/**
 * print warning message for parsing node
 *
 * \param [in] msg      Warning message
 * \param [in] node     Offending node that caused the warning
 */
void Config::print_warning(std::string msg, const YAML::Node &node) {
    std::string type;

    switch (node.Type()) {
        case YAML::NodeType::Null:
            type = "Null";
            break;
        case YAML::NodeType::Scalar:
            type = "Scalar";
            break;
        case YAML::NodeType::Sequence:
            type = "Sequence";
            break;
        default:
            type = "Unknown";
            break;
    }
    std::cout << "WARN: " << msg << " : " << type << " = " << node.Scalar() << std::endl ;
}

void Config::parse_base(const YAML::Node &node) {
    std::string value;

    if (node["collector_id"]) {
        try {
            value = node["collector_id"].as<std::string>();

            if (value == "hostname") {
                gethostname(collector_id, sizeof(collector_id));
            } else {
                std::strncpy(collector_id, value.c_str(), sizeof(collector_id));
            }

            if (debug_general)
                std::cout << "   Config: collector id : " << collector_id << std::endl;

        } catch (YAML::TypedBadConversion<std::string> err) {
            print_warning("admin_id is not of type string", node["admin_id"]);
        }
    }

    if (node["listen_port"]) {
        try {
            bmp_port = node["listen_port"].as<uint16_t>();

            if (bmp_port < 25 || bmp_port > 65535)
                throw "invalid listen_port, not within range of 25 - 65535)";

            if (debug_general)
                std::cout << "   Config: bmp_port: " << bmp_port << std::endl;

        } catch (YAML::TypedBadConversion<uint16_t> err) {
            print_warning("bmp_port is not of type unsigned 16 bit", node["listen_port"]);
        }
    }

    if (node["listen_ipv4"]) {
        bind_ipv4 = node["listen_ipv4"].as<std::string>();

        if (debug_general)
            std::cout << "   Config: listen_ipv4: " << bind_ipv4 << "\n";
    }

    if (node["listen_ipv6"]) {
        bind_ipv6 = node["listen_ipv6"].as<std::string>();

        if (debug_general)
            std::cout << "   Config: listen_ipv6: " << bind_ipv6 << "\n";
    }

    if (node["listen_mode"]) {
        try {
            value = node["listen_mode"].as<std::string>();

            if (value == "v4") {
                svr_ipv4 = true;
            } else if (value.compare("v6") == 0) {
                svr_ipv6 = true;
                svr_ipv4 = false;
            } else { /* don't care if it's v4v6 or not */
                svr_ipv6 = true;
                svr_ipv4 = true;
            }

            if (debug_general)
                std::cout <<  "   Config: listen_mode is " << value << std::endl;

        } catch (YAML::TypedBadConversion<std::string> err) {
            print_warning("listen_mode is not of type string", node["listen_mode"]);
        }
    }

    if (node["bmp_ring_buffer_size"]) {
        try {
            bmp_ring_buffer_size = node["bmp_ring_buffer_size"].as<int>();
            cout << "buffer: " << bmp_ring_buffer_size << endl;

            if (bmp_ring_buffer_size < 2 || bmp_ring_buffer_size > 384)
                throw "invalid router buffer size, not within range of 2 - 384)";

            bmp_ring_buffer_size *= 1024 * 1024;  // MB to bytes

            if (debug_general)
                std::cout << "   Config: bmp buffer: " << bmp_ring_buffer_size << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("buffers.router is not of type int", node["bmp_ring_buffer_size"]);
        }
    }

    if (node["max_cpu_utilization"]) {
        try {
            max_cpu_utilization = node["max_cpu_utilization"].as<float>();

            if (max_cpu_utilization < 0.0 || max_cpu_utilization > 1.0)
                throw "invalid max cpu utilization, not within range of (0, 1))";

            if (debug_general)
                std::cout << "   Config: max cpu utilization: " << max_cpu_utilization << std::endl;

        } catch (YAML::TypedBadConversion<float> err) {
            print_warning("max_cpu_utilization is not of type float", node["bmp_ring_buffer_size"]);
        }
    }

    if (node["daemon"]) {
        try {
            daemon = node["daemon"].as<bool>();

            if (debug_general)
                std::cout << "   Config: daemon: " << daemon << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("daemon is not of type bool", node["daemon"]);
        }
    }

}

void Config::parse_debug(const YAML::Node &node) {
    if (!debug_general and node["general"]) {
        try {
            debug_general = node["general"].as<bool>();

            if (debug_general)
                std::cout << "   Config: debug general : " << debug_general << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.general is not of type boolean", node["general"]);
        }
    }

    if (!debug_bmp and node["bmp"]) {
        try {
            debug_bmp = node["bmp"].as<bool>();

            if (debug_general)
                std::cout << "   Config: debug bmp : " << debug_bmp << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.bmp is not of type boolean", node["bmp"]);
        }
    }

    if (!debug_msgbus and node["msgbus"]) {
        try {
            debug_msgbus = node["msgbus"].as<bool>();

            if (debug_general)
                std::cout << "   Config: debug msgbus : " << debug_msgbus << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.msgbus is not of type boolean", node["msgbus"]);
        }
    }

}

void Config::parse_kafka(const YAML::Node &node) {
    std::string value;

    if (node["brokers"] && node["brokers"].Type() == YAML::NodeType::Sequence) {
        kafka_brokers.clear();

        for (std::size_t i = 0; i < node["brokers"].size(); i++) {
            value = node["brokers"][i].Scalar();

            if (!value.empty()) {
                if (!kafka_brokers.empty())
                    kafka_brokers.append(",");

                kafka_brokers.append(value);
            }

            if (debug_general)
                std::cout << "   Config: kafka.brokers = " << kafka_brokers << "\n";
        }
    }

    if (node["message.max.bytes"] && node["message.max.bytes"].Type() == YAML::NodeType::Scalar) {
        try {
            tx_max_bytes = node["message.max.bytes"].as<int>();

            if (tx_max_bytes < 1000 || tx_max_bytes > 1000000000)
                throw "invalid transmit max bytes , should be "
                      "in range 1000 - 1000000000";

            // Below corrects older configs to use 1M instead of 200MB.
            if (tx_max_bytes == 200000000)
                tx_max_bytes = 1000000;

            if (debug_general)
                std::cout << "   Config: transmit max bytes : " << tx_max_bytes
                          << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("message.max.bytes is not of type int",
                         node["message.max.bytes"]);
        }
    }

    if (node["receive.message.max.bytes"]  && node["receive.message.max.bytes"].Type() == YAML::NodeType::Scalar) {
        try {
            rx_max_bytes = node["receive.message.max.bytes"].as<int>();

            if (rx_max_bytes < 1000 || rx_max_bytes > 1000000000)
                throw "invalid receive max bytes , should be "
                      "in range 1000 - 1000000000";
            if (debug_general)
                std::cout << "   Config: receive max bytes : " << rx_max_bytes
                          << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("receive.message.max.bytes is not of type int",
                         node["receive.message.max.bytes"]);
        }
    }

    if (node["session.timeout.ms"]  &&
        node["session.timeout.ms"].Type() == YAML::NodeType::Scalar) {
        try {
            session_timeout = node["session.timeout.ms"].as<int>();

            if (session_timeout < 1 || session_timeout > 3600000)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 3600000";
            if (debug_general)
                std::cout << "   Config: session timeout in ms: " << session_timeout
                          << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("session_timeout is not of type int",
                         node["session.timeout.ms"]);
        }
    }

    if (node["socket.timeout.ms"]  &&
        node["socket.timeout.ms"].Type() == YAML::NodeType::Scalar) {
        try {
            socket_timeout = node["socket.timeout.ms"].as<int>();

            if (socket_timeout < 10 || socket_timeout > 300000)
                throw "invalid receive max bytes , should be "
                      "in range 10 - 300000";
            if (debug_general)
                std::cout << "   Config: socket timeout in ms: " << socket_timeout
                          << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("socket_timeout is not of type int",
                         node["socket.timeout.ms"]);
        }
    }

    if (node["queue.buffering.max.messages"]  &&
        node["queue.buffering.max.messages"].Type() == YAML::NodeType::Scalar) {
        try {
            q_buf_max_msgs = node["queue.buffering.max.messages"].as<int>();

            if (q_buf_max_msgs < 1 || q_buf_max_msgs > 10000000)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 10000000";
            if (debug_general)
                std::cout << "   Config: queue buffering max messages: " <<
                          q_buf_max_msgs << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("q_buf_max_msgs is not of type int",
                         node["queue.buffering.max.messages"]);
        }
    }

    if (node["queue.buffering.max.kbytes"]  &&
        node["queue.buffering.max.kbytes"].Type() == YAML::NodeType::Scalar) {
        try {
            q_buf_max_kbytes = node["queue.buffering.max.kbytes"].as<int>();

            if (q_buf_max_kbytes < 1 || q_buf_max_kbytes > 2097151)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 2097151";
            if (debug_general)
                std::cout << "   Config: queue buffering max kbytes: " <<
                          q_buf_max_kbytes << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("q_buf_max_kbytes is not of type int",
                         node["queue.buffering.max.kbytes"]);
        }
    }


    if (node["queue.buffering.max.ms"]  &&
        node["queue.buffering.max.ms"].Type() == YAML::NodeType::Scalar) {
        try {
            q_buf_max_ms = node["queue.buffering.max.ms"].as<int>();

            if (q_buf_max_ms < 1 || q_buf_max_ms > 900000)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 900000";
            if (debug_general)
                std::cout << "   Config: queue buffering max time in ms: " <<
                          q_buf_max_ms << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("q_buf_max_ms is not of type int",
                         node["queue.buffering.max.ms"]);
        }
    }

    if (node["message.send.max.retries"]  &&
        node["message.send.max.retries"].Type() == YAML::NodeType::Scalar) {
        try {
            msg_send_max_retry = node["message.send.max.retries"].as<int>();

            if (msg_send_max_retry < 0 || msg_send_max_retry > 10000000)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 10000000";
            if (debug_general)
                std::cout << "   Config: max message send retry: " <<
                          msg_send_max_retry << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("msg_send_max_retry is not of type int",
                         node["message.send.max.retries"]);
        }
    }

    if (node["retry.backoff.ms"]  &&
        node["retry.backoff.ms"].Type() == YAML::NodeType::Scalar) {
        try {
            retry_backoff_ms = node["retry.backoff.ms"].as<int>();

            if (retry_backoff_ms < 1 || retry_backoff_ms > 300000)
                throw "invalid receive max bytes , should be "
                      "in range 1 - 300000";
            if (debug_general)
                std::cout << "   Config: backoff time before resending failed message in ms: " <<
                          retry_backoff_ms << std::endl;

        } catch (YAML::TypedBadConversion<int> err) {
            print_warning("retry_backoff_ms is not of type int",
                         node["retry.backoff.ms"]);
        }
    }

    if (node["compression.codec"]  &&
        node["compression.codec"].Type() == YAML::NodeType::Scalar) {
        try {
            compression = node["compression.codec"].as<std::string>();

            if (compression != "none" && compression != "snappy" &&
                compression != "gzip" && compression != "lz4")
                throw "invalid value for compression, should be one of none,"
                      " gzip, snappy, or lz4";
            if (debug_general)
                std::cout << "   Config: Compression : " <<
                          compression << std::endl;

        } catch (YAML::TypedBadConversion<std::string> err) {
            print_warning("Compression is not of type string",
                         node["compression.codec"]);
        }
    }

    if (node["topics"] && node["topics"].Type() == YAML::NodeType::Map) {
        parse_kafka_topics(node["topics"]);
    }

}

void Config::parse_kafka_topics(const YAML::Node &node) {
    if (node and node.Type() == YAML::NodeType::Map) {
        for (auto it: node) {
            try {
                // Only add topic names that are initialized, otherwise ignore them
                if (topic_names_map.find(it.first.as<std::string>()) != topic_names_map.end()) {
                    if (it.second.Type() == YAML::NodeType::Null) {
                        // why do we set value as an empty string?
                        topic_names_map[it.first.as<std::string>()] = "";
                    } else {
                        topic_names_map[it.first.as<std::string>()] = it.second.as<std::string>();
                    }
                } else if (debug_general)
                    std::cout << "   Ignore: '" << it.first.as<std::string>()
                              << "' is not a valid topic name entry" << std::endl;

            } catch (YAML::TypedBadConversion<std::string> err) {
                print_warning("kafka.topics.names error in map.  Make sure to define var: <string value>", it.second);
            }
        }

        if (debug_general) {
            for (auto & it : topic_names_map) {
                std::cout << "   Config: kafka.topics: " << it.first << " = " << it.second << std::endl;
            }
        }
    }

    if (debug_general) {
        for (auto & it : topic_names_map) {
            std::cout << "   Config: postsub: kafka.topics.names: " << it.first << " = " << it.second << std::endl;
        }
    }
}
