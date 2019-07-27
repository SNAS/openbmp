#include <cstring>
#include <iostream>
#include "Config.h"

Config::Config() {
    // Initialize the defaults
    daemon = true;
    bmp_port = 5000;
    debug_all = false;
    debug_collector = false;
    debug_worker = false;
    debug_encapsulator = false;
    debug_message_bus = false;
    bmp_ring_buffer_size = 15 * 1024 * 1024; // 15MB
    svr_ipv6 = false;
    svr_ipv4 = true;
    bind_ipv4 = "";
    bind_ipv6 = "";
    gethostname((char *) collector_name, sizeof(collector_name));
    max_cpu_utilization = .8;

    // Initialize default values for topic templates that OpenBMP V2 uses
    // Users can/should change these values from the openbmp config file
    topic_name_templates["collector"] = "openbmp.collector";
    topic_name_templates["router"] = "openbmp.router";
    topic_name_templates["bmp_raw"] = "openbmp.bmp_raw";
}

void Config::load(const char *config_filename) {
    std::cout << "Loading configuration file" << std::endl;

    try {
        YAML::Node root = YAML::LoadFile(config_filename);

        /*
         * Iterate through the root node objects - We expect only maps at the root level, skip others
         */
        if (root.Type() == YAML::NodeType::Map) {
            // we first load debug variables if it exists
            for (auto it: root) {
                const std::string &key = it.first.Scalar();
                if (key == "debug") {
                    const YAML::Node &node = it.second;
                    parse_debug(node);
                }
            }
            // we then load other variables from the root of the config file
            for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
                const YAML::Node &node = it->second;
                const std::string &key = it->first.Scalar();

                if (node.Type() == YAML::NodeType::Map) {
                    if (key == "base")
                        parse_base(node);
                    else if (key == "librdkafka_config")
                        parse_librdkafka_config(node);
                    else if (debug_all)
                        std::cout << "   Config: Key " << key << " Type " << node.Type() << std::endl;
                }
                else {
                    print_warning("configuration file should only have maps at the root level", node);
                }
            }
        } else {
            print_warning("configuration file should only have maps at the root level", root);
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

    if (node["collector_name"]) {
        try {
            value = node["collector_name"].as<std::string>();

            if (value == "hostname") {
                gethostname((char *) collector_name, sizeof(collector_name));
            } else {
                std::strncpy((char *)collector_name, value.c_str(), sizeof(collector_name));
            }

            MD5(collector_name, strlen((char*) collector_name), collector_hash_id);
            if (debug_all) {
                std::cout << "   Config: collector name : " << collector_name << std::endl;
                std::cout << "   Config: collector hash id : " << collector_hash_id << std::endl;
            }

        } catch (YAML::TypedBadConversion<std::string> err) {
            print_warning("admin_id is not of type string", node["admin_id"]);
        }
    }

    if (node["listen_port"]) {
        try {
            bmp_port = node["listen_port"].as<uint16_t>();

            if (bmp_port < 25 || bmp_port > 65535)
                throw "invalid listen_port, not within range of 25 - 65535)";

            if (debug_all)
                std::cout << "   Config: bmp_port: " << bmp_port << std::endl;

        } catch (YAML::TypedBadConversion<uint16_t> err) {
            print_warning("bmp_port is not of type unsigned 16 bit", node["listen_port"]);
        }
    }

    if (node["listen_ipv4"]) {
        bind_ipv4 = node["listen_ipv4"].as<std::string>();

        if (debug_all)
            std::cout << "   Config: listen_ipv4: " << bind_ipv4 << "\n";
    }

    if (node["listen_ipv6"]) {
        bind_ipv6 = node["listen_ipv6"].as<std::string>();

        if (debug_all)
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

            if (debug_all)
                std::cout <<  "   Config: listen_mode is " << value << std::endl;

        } catch (YAML::TypedBadConversion<std::string> err) {
            print_warning("listen_mode is not of type string", node["listen_mode"]);
        }
    }

    if (node["bmp_ring_buffer_size"]) {
        try {
            bmp_ring_buffer_size = node["bmp_ring_buffer_size"].as<int>();

            if (bmp_ring_buffer_size < 2 || bmp_ring_buffer_size > 384)
                throw "invalid router buffer size, not within range of 2 - 384)";

            bmp_ring_buffer_size *= 1024 * 1024;  // MB to bytes

            if (debug_all)
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

            if (debug_all)
                std::cout << "   Config: max cpu utilization: " << max_cpu_utilization << std::endl;

        } catch (YAML::TypedBadConversion<float> err) {
            print_warning("max_cpu_utilization is not of type float", node["bmp_ring_buffer_size"]);
        }
    }

    if (node["daemon"]) {
        try {
            daemon = node["daemon"].as<bool>();

            if (debug_all)
                std::cout << "   Config: daemon: " << daemon << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("daemon is not of type bool", node["daemon"]);
        }
    }

}

void Config::parse_debug(const YAML::Node &node) {
    if (!debug_all and node["all"]) {
        try {
            debug_all = node["all"].as<bool>();

            if (debug_all)
                std::cout << "   Config: debug all : " << debug_all << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.all is not of type boolean", node["general"]);
        }
    }

    if (!debug_collector and node["collector"]) {
        try {
            debug_collector = node["collector"].as<bool>();

            if (debug_all)
                std::cout << "   Config: debug collector : " << debug_collector << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.collector is not of type boolean", node["bmp"]);
        }
    }

    if (!debug_worker and node["worker"]) {
        try {
            debug_worker = node["worker"].as<bool>();

            if (debug_all)
                std::cout << "   Config: debug worker : " << debug_worker << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.worker is not of type boolean", node["bmp"]);
        }
    }

    if (!debug_encapsulator and node["encapsulator"]) {
        try {
            debug_encapsulator = node["encapsulator"].as<bool>();

            if (debug_encapsulator)
                std::cout << "   Config: debug encapsulator : " << debug_encapsulator << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.encapsulator is not of type boolean", node["bmp"]);
        }
    }

    if (!debug_message_bus and node["message_bus"]) {
        try {
            debug_message_bus = node["message_bus"].as<bool>();

            if (debug_all)
                std::cout << "   Config: debug message bus : " << debug_message_bus << std::endl;

        } catch (YAML::TypedBadConversion<bool> err) {
            print_warning("debug.message_bus is not of type boolean", node["message_bus"]);
        }
    }

}

void Config::parse_librdkafka_config(const YAML::Node &node) {
    if (node and node.Type() == YAML::NodeType::Map) {
        for (auto it: node) {
            try {
                librdkafka_passthrough_configs[it.first.as<std::string>()] = it.second.as<std::string>();
            } catch (YAML::TypedBadConversion<std::string> err) {
                print_warning("kafka.topics.names error in map.  Make sure to define var: <string value>", it.second);
            }
        }
    }

    if (debug_all) {
        for (auto & it : librdkafka_passthrough_configs) {
            std::cout << "   Config: librdkafka.passthrough.config: " << it.first << " = " << it.second << std::endl;
        }
    }
}

void Config::parse_kafka_topic_template(const YAML::Node &node) {

}
