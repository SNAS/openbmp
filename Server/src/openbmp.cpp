/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

/**
 * \file   openbmpd.cpp
 *
 * \brief  The openbmp daemon
 *
 * \author Tim Evens <tievens@cisco.com>, <tim@openbmp.org>
 */

#include "BMPListener.h"
#include "MsgBusImpl_kafka.h"
#include "MsgBusInterface.hpp"
#include "client_thread.h"
#include "openbmpd_version.h"
#include "Config.h"

#include <unistd.h>
#include <fstream>
#include <csignal>
#include <cstring>
#include <sys/stat.h>
#include "md5.h"

using namespace std;

/*
 * Global parameters
 */
const char *cfg_filename    = NULL;                 // Configuration file name to load/read
const char *log_filename    = NULL;                 // Output file to log messages to
const char *debug_filename  = NULL;                 // Debug file to log messages to
const char *pid_filename    = NULL;                 // PID file to record the daemon pid
bool        debugEnabled    = false;                // Globally enable/disable dbug
bool        run             = true;                 // Indicates if server should run
bool        run_foreground  = false;                // Indicates if server should run in forground


#define MAX_THREADS 200

// Global thread list
vector<ThreadMgmt *> thr_list(0);

static Logger *logger;                              // Local source logger reference

/**
 * Usage of the program
 */
void Usage(char *prog) {
    cout << "Usage: " << prog << " <options>" << endl;
    cout << endl << "  REQUIRED OPTIONS:" << endl;
    cout << "     -c <filename>     Config filename.  " <<  endl;
    cout << "          OR " << endl;
    cout << "     -a <string>       Admin ID for collector, this must be unique for this collector.  hostname or IP is good to use" << endl;
    cout << endl;

    cout << endl << "  OPTIONAL OPTIONS:" << endl;
    cout << "     -pid <filename>   PID filename, default is no pid file" << endl;
    cout << "     -l <filename>     Log filename, default is STDOUT" << endl;
    cout << "     -d <filename>     Debug filename, default is log filename" << endl;
    cout << "     -f                Run in foreground instead of daemon (use for upstart)" << endl;

    cout << endl << "  OTHER OPTIONS:" << endl;
    cout << "     -v                   Version" << endl;

    cout << endl << "  DEBUG OPTIONS:" << endl;
    cout << "     -debug"           "Debug general items" << endl;
    cout << "     -dbgp             Debug BGP parser" <<  endl;
    cout << "     -dbmp             Debug BMP parser" << endl;
    cout << "     -dmsgbus          Debug message bus" << endl;

    cout << endl << "  DEPRECATED OPTIONS:" << endl;
    cout << endl << "       These options will be removed in a future release. You should switch to use the config file." << endl;
    cout << "     -k <host:port>    Kafka broker list format: host:port[,...]" << endl;
    cout << "                       Default is 127.0.0.1:9092" << endl;
    cout << "     -m <mode>         Mode can be 'v4, v6, or v4v6'" << endl;
    cout << "                       Default is v4.  Enables IPv4 and/or IPv6 BMP listening port" << endl;
    cout << endl;
    cout << "     -p <port>         BMP listening port (default is 5000)" << endl;
    cout << "     -b <MB>           BMP read buffer per router size in MB (default is 15), range is 2 - 128" << endl;
    cout << "     -hi <minutes>     Collector message heartbeat interval in minutes (default is 5 minutes)" << endl;
    cout << "     -tx_max_bytes <bytes>    Maximum transmit message size (default is 1000000)" << endl;
    cout << "     -rx_max_bytes <bytes>    Maximum transmit message size (default is 1000000)" << endl;
    cout << endl;

}

/**
 * Daemonize the program
 */
void daemonize() {
    pid_t pid, sid;

    pid = fork();

    if (pid < 0) // Error forking
        _exit(EXIT_FAILURE);

    if (pid > 0) {
        _exit(EXIT_SUCCESS);

    } else {
        sid = setsid();
        if (sid < 0)
            exit(EXIT_FAILURE);
    }

    //Change File Mask
    umask(0);

    //Change Directory
    if ((chdir("/")) < 0)
        exit(EXIT_FAILURE);

    //Close Standard File Descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Write PID to PID file if requested
    if (pid_filename != NULL) {
        //pid_t pid = getpid();
        ofstream pfile(pid_filename);

        if (pfile.is_open()) {
            pfile << pid << endl;
            pfile.close();
        } else {
            LOG_ERR("Failed to write PID to %s", pid_filename);
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Signal handler
 *
 */
void signal_handler(int signum)
{
    LOG_NOTICE("Caught signal %d", signum);

    /*
     * Respond based on the signal
     */
    switch (signum) {
        case SIGTERM :
        case SIGKILL :
        case SIGQUIT :
        case SIGPIPE :
        case SIGINT  :
        case SIGCHLD : // Handle the child cleanup

            for (size_t i=0; i < thr_list.size(); i++) {
                pthread_cancel(thr_list.at(i)->thr);
                thr_list.at(i)->running = false;
                pthread_join(thr_list.at(i)->thr, NULL);
            }

            thr_list.clear();

            run = false;
            break;

        default:
            LOG_INFO("Ignoring signal %d", signum);
            break;
    }
}

/**
 * Parse and handle the command line args
 *
 * \param [out] cfg    Reference to config options - Will be updated based on cli
 *
 * \returns true if error, false if no error
 *
 */
bool ReadCmdArgs(int argc, char **argv, Config &cfg) {

    // Make sure we have the correct number of required args
    if (argc > 1 and !strcmp(argv[1], "-v")) {   // Version
        cout << "openbmpd (www.openbmp.org) version : " << OPENBMPD_VERSION << endl;
        exit(0);
    }

    else if (argc > 1 and !strcmp(argv[1], "-h")) {
        Usage(argv[0]);
        exit(0);
    }

    // Loop through the args
    for (int i=1; i < argc; i++) {

        if (!strcmp(argv[i], "-h")) {   // Help message
            Usage(argv[0]);
            exit(0);

        } else if (!strcmp(argv[i], "-p")) {
            // We expect the next arg to be a port
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -p expects a port number" << endl;
                return true;
            }

            cfg.bmp_port = atoi(argv[++i]);

            // Validate the port
            if (cfg.bmp_port < 25 || cfg.bmp_port > 65535) {
                cout << "INVALID ARG: port '" << cfg.bmp_port
                        << "' is out of range, expected range is 100-65535" << endl;
                return true;
            }

        } else if (!strcmp(argv[i], "-hi")) {
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -hi expects minutes" << endl;
                return true;
            }

            cfg.heartbeat_interval = atoi(argv[++i]) * 60;

            // Validate range
            if (cfg.heartbeat_interval < 60 || cfg.heartbeat_interval > 86400) {
                cout << "INVALID ARG: heartbeat interval '" << cfg.heartbeat_interval <<
                                                 "' is out of range, expected range is 1 - 1440" << endl;
                return true;
            }

        } else if (!strcmp(argv[i], "-rx_max_bytes")) {
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -rx_max_bytes expects bytes" << endl;
                return true;
            }

            cfg.rx_max_bytes = atoi(argv[++i]);

            // Validate range
            if (cfg.rx_max_bytes < 1) { 
                cout << "INVALID ARG:  receive max bytes '" << cfg.rx_max_bytes <<
                                                 "' is out of range" << endl;
                return true;
            }
        } else if (!strcmp(argv[i], "-tx_max_bytes")) {
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -tx_max_bytes expects bytes" << endl;
                return true;
            }

            cfg.tx_max_bytes = atoi(argv[++i]);

            // Validate range
            if (cfg.tx_max_bytes < 1) { 
                cout << "INVALID ARG:  transmit max bytes '" << cfg.tx_max_bytes <<
                                                 "' is out of range" << endl;
                return true;
            }
        } else if (!strcmp(argv[i], "-m")) {
            // We expect the next arg to be mode
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -m expects mode value" << endl;
                return true;
            }

            ++i;
            if (!strcasecmp(argv[i], "v4")) {
                cfg.svr_ipv4 = true;
            } else if (!strcasecmp(argv[i], "v6")) {
                cfg.svr_ipv6 = true;
                cfg.svr_ipv4 = false;
            } else if (!strcasecmp(argv[i], "v4v6")) {
                cfg.svr_ipv6 = true;
                cfg.svr_ipv4 = true;
            } else {
                cout << "INVALID ARG: mode '" << argv[i] << "' is invalid. Expected v4, v6, or v4v6" << endl;
                return true;
            }

        } else if (!strcmp(argv[i], "-k")) {
            // We expect the next arg to be the kafka broker list hostname:port
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -k expects the kafka broker list host:port[,...]" << endl;
                return true;
            }

            cfg.kafka_brokers = argv[++i];

        } else if (!strcmp(argv[i], "-a")) {
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -a expects admin ID string" << endl;
                return true;
            }

            snprintf(cfg.admin_id, sizeof(cfg.admin_id), "%s", argv[++i]);

        } else if (!strcmp(argv[i], "-b")) {
            // We expect the next arg to be the size in MB
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -b expects a value between 2 and 15" << endl;
                return true;
            }

            cfg.bmp_buffer_size = atoi(argv[++i]);

            // Validate the size
            if (cfg.bmp_buffer_size < 2 || cfg.bmp_buffer_size > 384) {
                cout << "INVALID ARG: port '" << cfg.bmp_buffer_size <<
                                                 "' is out of range, expected range is 2 - 384" << endl;
                return true;
            }

            // Convert the size to bytes
            cfg.bmp_buffer_size = cfg.bmp_buffer_size * 1024 * 1024;

        } else if (!strcmp(argv[i], "-debug")) {
            cfg.debug_general = true;
            debugEnabled = true;

        } else if (!strcmp(argv[i], "-dbgp")) {
            cfg.debug_bgp = true;
            debugEnabled = true;
        } else if (!strcmp(argv[i], "-dbmp")) {
            cfg.debug_bmp = true;
            debugEnabled = true;
        } else if (!strcmp(argv[i], "-dmsgbus")) {
            cfg.debug_msgbus = true;
            debugEnabled = true;

        } else if (!strcmp(argv[i], "-f")) {
            run_foreground = true;
        }

        // Config filename
        else if (!strcmp(argv[i], "-c")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -c expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            cfg_filename = argv[++i];
        }

        // Log filename
        else if (!strcmp(argv[i], "-l")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -l expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            log_filename = argv[++i];
        }

        // Debug filename
        else if (!strcmp(argv[i], "-d")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -d expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            debug_filename = argv[++i];
        }

        // PID filename
        else if (!strcmp(argv[i], "-pid")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -p expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            pid_filename = argv[++i];
        }
    }

    return false;
}

/**
 * Collector Update Message
 *
 * \param [in] kafka                 Pointer to kafka instance
 * \param [in] cfg                   Reference to configuration
 * \param [in] code                  reason code for the update
 */
void collector_update_msg(msgBus_kafka *kafka, Config &cfg,
                          MsgBusInterface::collector_action_code code) {

    MsgBusInterface::obj_collector oc;

    snprintf(oc.admin_id, sizeof(oc.admin_id), "%s", cfg.admin_id);

    oc.router_count = thr_list.size();

    string router_ips;
    for (int i=0; i < thr_list.size(); i++) {
        //MsgBusInterface::hash_toStr(thr_list.at(i)->client.hash_id, hash_str);
        if (router_ips.size() > 0)
            router_ips.append(", ");

        router_ips.append(thr_list.at(i)->client.c_ip);
    }

    snprintf(oc.routers, sizeof(oc.routers), "%s", router_ips.c_str());

    timeval tv;
    gettimeofday(&tv, NULL);
    oc.timestamp_secs = tv.tv_sec;
    oc.timestamp_us = tv.tv_usec;

    kafka->update_Collector(oc, code);
}

/**
 * Run Server loop
 *
 * \param [in]  cfg    Reference to the config options
 */
void runServer(Config &cfg) {
    msgBus_kafka *kafka;
    int active_connections = 0;                 // Number of active connections/threads
    time_t last_heartbeat_time = 0;

    LOG_INFO("Initializing server");

    try {
        // Define the collector hash
        MD5 hash;
        hash.update((unsigned char *)cfg.admin_id, strlen(cfg.admin_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(cfg.c_hash_id, hash_raw, 16);
        delete[] hash_raw;

        // Kafka connection
        kafka = new msgBus_kafka(logger, &cfg, cfg.c_hash_id);

        // allocate and start a new bmp server
        BMPListener *bmp_svr = new BMPListener(logger, &cfg);

        BMPListener::ClientInfo client;
        collector_update_msg(kafka, cfg, MsgBusInterface::COLLECTOR_ACTION_STARTED);
        last_heartbeat_time = time(NULL);

        LOG_INFO("Ready. Waiting for connections");

        // Loop to accept new connections
        while (run) {
            /*
             * Check for any stale threads/connections
             */
             for (size_t i=0; i < thr_list.size(); i++) {

                // If thread is not running, it means it terminated, so close it out
                if (!thr_list.at(i)->running) {

                    // Join the thread to clean up
                    pthread_join(thr_list.at(i)->thr, NULL);
                    --active_connections;

                    // free the vector entry
                    delete thr_list.at(i);
                    thr_list.erase(thr_list.begin() + i);

                    collector_update_msg(kafka, cfg,
                                         MsgBusInterface::COLLECTOR_ACTION_CHANGE);

                }

                //TODO: Add code to check for a socket that is open, but not really connected/half open
            }

            /*
             * Create a new client thread if we aren't at the max number of active sessions
             */
            if (active_connections <= MAX_THREADS) {
                ThreadMgmt *thr = new ThreadMgmt;
                thr->cfg = &cfg;
                thr->log = logger;

                // wait for a new connection and accept
                if (bmp_svr->wait_and_accept_connection(thr->client, 500)) {
                    // Bump the current thread count
                    ++active_connections;

                    LOG_INFO("Accepted new connection; active connections = %d", active_connections);

                    /*
                     * Start a new thread for every new router connection
                     */
                    LOG_INFO("Client Connected => %s:%s, sock = %d",
                             thr->client.c_ip, thr->client.c_port, thr->client.c_sock);

                    pthread_attr_t thr_attr;            // thread attribute
                    pthread_attr_init(&thr_attr);
                    //pthread_attr_setdetachstate(&thr.thr_attr, PTHREAD_CREATE_DETACHED);
                    pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
                    thr->running = 1;

                    // Start the thread to handle the client connection
                    pthread_create(&thr->thr, &thr_attr,
                                   ClientThread, thr);

                    // Add thread to vector
                    thr_list.insert(thr_list.end(), thr);

                    // Free attribute
                    pthread_attr_destroy(&thr_attr);

                    collector_update_msg(kafka, cfg,
                                         MsgBusInterface::COLLECTOR_ACTION_CHANGE);

                    last_heartbeat_time = time(NULL);
                }
                else {
                    delete thr;

                    // Send heartbeat if needed
                    if ( (time(NULL) - last_heartbeat_time) >= cfg.heartbeat_interval) {
                        BMPListener::ClientInfo client;
                        collector_update_msg(kafka, cfg, MsgBusInterface::COLLECTOR_ACTION_HEARTBEAT);
                        last_heartbeat_time = time(NULL);
                    }

                    usleep(10000);
                }

            } else {
                LOG_WARN("Reached max number of threads, cannot accept new BMP connections at this time.");
                sleep (1);
            }
        }

        collector_update_msg(kafka, cfg, MsgBusInterface::COLLECTOR_ACTION_STOPPED);
        delete kafka;

    } catch (char const *str) {
        LOG_WARN(str);
    }
}

/**
 * main function
 */
int main(int argc, char **argv) {
    Config cfg;

    // Process the command line args
    if (ReadCmdArgs(argc, argv, cfg)) {
        return 1;
    }

    if (cfg_filename != NULL) {
        try {
            cfg.load(cfg_filename);

        } catch (char const *str) {
            cout << "ERROR: Failed to load the configuration file: " << str << endl;
            return 2;
        }
    }

    // Make sure we have the required ARGS
    if (strlen(cfg.admin_id) <= 0) {
        cout << "ERROR: Missing required 'admin ID', use -c <config> or -a <string> to set the collector admin ID" << endl;
        return true;
    }

    try {
        // Initialize logging
        logger = new Logger(log_filename, debug_filename);
    } catch (char const *str) {
        cout << "Failed to open log file for read/write : " << str << endl;
        return 2;
    }

    // Set up defaults for logging
    logger->setWidthFilename(15);
    logger->setWidthFunction(18);

    if (debugEnabled)
        logger->enableDebug();
    else if (not run_foreground){
        /*
        * Become a daemon if debug is not enabled
        */
        daemonize();
    }

    /*
     * Setup the signal handlers
     */
    struct sigaction sigact;
    sigact.sa_handler = signal_handler;
    sigact.sa_flags = 0;
    sigemptyset( &sigact.sa_mask);          // blocked signals while handler runs

    sigaction(SIGCHLD, &sigact, NULL);
    sigaction(SIGHUP, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);

    // Run the server (loop)
    runServer(cfg);

	LOG_NOTICE("Program ended normally");

	return 0;
}

