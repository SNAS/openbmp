#ifndef OPENBMP_CONFIG_H
#define OPENBMP_CONFIG_H

#include <string>
#include <yaml-cpp/yaml.h>


using namespace std;
class Config {
public:
    /* Constructor */
    Config();


    /* Method to load variables from a config file */
    void load(const char *cfg_filename);


    /* Config Variables */
    bool run_foreground;             ///run the program foreground
    const char *cfg_filename    = nullptr;                 // Configuration file name to load/read
    const char *log_filename    = nullptr;                 // Output file to log messages to
    const char *debug_filename  = nullptr;                 // Debug file to log messages to
    const char *pid_filename    = nullptr;                 // PID file to record the daemon pid

    u_char c_hash_id[16];            ///< Collector Hash ID (raw format)
    char admin_id[64];             ///< Admin ID

    string kafka_brokers;            ///< metadata.broker.list
    uint16_t bmp_port;                 ///< BMP listening port
    string bind_ipv4;                ///< IP to listen on for IPv4
    string bind_ipv6;                ///< IP to listen on for IPv6

    int bmp_ring_buffer_size;          ///< BMP buffer size in bytes (min is 2M max is 128M)
    bool svr_ipv4;                 ///< Indicates if server should listen for IPv4 connections
    bool svr_ipv6;                 ///< Indicates if server should listen for IPv6 connections

    bool debug_general;
    bool debug_bmp;
    bool debug_msgbus;

    int tx_max_bytes;            ///< Maximum transmit message size
    int rx_max_bytes;            ///< Maximum receive  message size
    int session_timeout;         ///< Client session timeout
    int socket_timeout;          ///< Network requests timeout
    int q_buf_max_msgs;      ///< Max msgs allowed in producer queue
    int q_buf_max_kbytes;    ///< Max kbytes allowed in producer queue
    int q_buf_max_ms;         ///< Max time for buffering msgs in queue
    int msg_send_max_retry;      ///< No. of times to resend failed msgs
    int retry_backoff_ms;        ///< Backoff time before resending msgs
    string compression;         ///< Compression to use :none, gzip, snappy
    int max_concurrent_routers;  ///<Maximum allowed routers that can connect
    int initial_router_time;     ///<Initial time in allowing another concurrent router

private:
    void print_warning(string msg, const YAML::Node &node);
    void parse_base(const YAML::Node &node);
    void parse_debug(const YAML::Node &node);
    void parse_kafka(const YAML::Node &node);

};

#endif //OPENBMP_CONFIG_H
