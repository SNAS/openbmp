#include <cstring>
#include <iostream>
#include "Config.h"

Config::Config() {
    // Initialize the defaults
    run_foreground = false;
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
    max_concurrent_routers = 2;
    initial_router_time = 60;
    memset(admin_id, 0, sizeof(admin_id));
}

void Config::load(const char *cfg_filename) {
    std::cout << "Loading configuration file" << std::endl;

    try {
        YAML::Node root = YAML::LoadFile(cfg_filename);

        /*
         * Iterate through the root node objects - We expect only maps at the root level, skip others
         */
        if (root.Type() == YAML::NodeType::Map) {
            for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
                const YAML::Node &node = it->second;
                const std::string &key = it->first.Scalar();

                if (node.Type() == YAML::NodeType::Map) {
                    if (key.compare("base") == 0)
                        parse_base(node);
                    else if (key.compare("debug") == 0)
                        parse_debug(node);
                    else if (key.compare("kafka") == 0)
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

}

void Config::parse_debug(const YAML::Node &node) {

}

void Config::parse_kafka(const YAML::Node &node) {

}
