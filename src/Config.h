#ifndef OPENBMP_CONFIG_H
#define OPENBMP_CONFIG_H

#include <string>
#include <unistd.h>
#include <yaml-cpp/yaml.h>


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

    u_char collector_hash_id[16];      // Collector Hash ID (raw format)
    char collector_id[64];             // Collector ID

    string kafka_brokers;            // metadata.broker.list
    uint16_t bmp_port;               // BMP listening port
    string bind_ipv4;                // IP to listen on for IPv4
    string bind_ipv6;                // IP to listen on for IPv6

    int bmp_ring_buffer_size;      // BMP buffer size in bytes (min is 2M max is 128M)
    float max_cpu_utilization;     // CPU utilization cap of the program

    bool svr_ipv4;                 // Indicates if server should listen for IPv4 connections
    bool svr_ipv6;                 // Indicates if server should listen for IPv6 connections

    bool debug_general;
    bool debug_bmp;
    bool debug_msgbus;

    int tx_max_bytes;        // Maximum transmit message size
    int rx_max_bytes;        // Maximum receive  message size
    int session_timeout;     // Client session timeout
    int socket_timeout;      // Network requests timeout
    int q_buf_max_msgs;      // Max msgs allowed in producer queue
    int q_buf_max_kbytes;    // Max kbytes allowed in producer queue
    int q_buf_max_ms;        // Max time for buffering msgs in queue
    int msg_send_max_retry;  // No. of times to resend failed msgs
    int retry_backoff_ms;    // Backoff time before resending msgs
    string compression;      // Compression to use :none, gzip, snappy

    /*
     * kafka topic names
     */
    std::map<std::string, std::string> topic_names_map;

private:
    static void print_warning(string msg, const YAML::Node &node);

    /*
     * a set of parsing methods to parse different sections of openbmp config file.
     */
    void parse_base(const YAML::Node &node);

    void parse_debug(const YAML::Node &node);

    void parse_kafka(const YAML::Node &node);

    void parse_kafka_topics(const YAML::Node &node);

};

#endif //OPENBMP_CONFIG_H
