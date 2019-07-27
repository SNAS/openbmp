#ifndef OPENBMP_CONFIG_H
#define OPENBMP_CONFIG_H

#include <string>
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <openssl/md5.h>


using namespace std;

class Config {
public:
    /* Constructor */
    Config();

    /* Method to load variables from a config file */
    void load(const char *config_filename);

    /* Config Variables */
    bool daemon;             ///run the program foreground
    const char *cfg_filename = nullptr;                 // Configuration file name to load/read
    const char *log_filename = nullptr;                 // Output file to log messages to
    const char *debug_filename = nullptr;               // Debug file to log messages to
    const char *pid_filename = nullptr;                 // PID file to record the daemon pid
    // Collector Hash ID (raw format); MD5 digest length is 16 bytes
    unsigned char collector_hash_id[MD5_DIGEST_LENGTH];
    // Collector name
    unsigned char collector_name[64];

    uint16_t bmp_port;               // BMP listening port
    string bind_ipv4;                // IP to listen on for IPv4
    string bind_ipv6;                // IP to listen on for IPv6

    int bmp_ring_buffer_size;      // BMP buffer size in bytes (min is 2M max is 128M)
    float max_cpu_utilization;     // CPU utilization cap of the program

    bool svr_ipv4;                 // Indicates if server should listen for IPv4 connections
    bool svr_ipv6;                 // Indicates if server should listen for IPv6 connections

    bool debug_all;
    bool debug_collector;
    bool debug_worker;
    bool debug_encapsulator;
    bool debug_message_bus;

    /*
     * kafka topic names
     */
    std::map<std::string, std::string> topic_name_templates;

    /*
     * librdkafka passthrough configurations
     */
    std::map<std::string, std::string> librdkafka_passthrough_configs;

private:
    static void print_warning(string msg, const YAML::Node &node);

    /*
     * a set of parsing methods to parse different sections of openbmp config file.
     */
    void parse_base(const YAML::Node &node);

    void parse_debug(const YAML::Node &node);

    void parse_librdkafka_config(const YAML::Node &node);

    void parse_kafka_topic_template(const YAML::Node &node);

};

#endif //OPENBMP_CONFIG_H
