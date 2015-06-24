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
#include "DbImpl_mysql.h"
#include "DbInterface.hpp"
#include "client_thread.h"
#include "openbmpd_version.h"
#include "Config.h"

#include <unistd.h>
#include <fstream>
#include <csignal>
#include <cstring>

using namespace std;

/*
 * Global parameters
 */
const char *cfg_filename    = NULL;                 // Configuration file name to load/read
const char *log_filename    = NULL;                 // Output file to log messages to
const char *debug_filename  = NULL;                 // Debug file to log messages to
const char *pid_filename    = NULL;                 // PID file to record the daemon pid
bool        debugEnabled    = false;                // Globally enable/disable dbug

#define CFG_FILENAME "/etc/openbmp/openbmpd.conf"

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
    cout << "     -dburl <url>      DB url, must be in url format" << endl;
    cout << "                       Example: tcp://127.0.0.1:3306" << endl;
    cout << "     -dbu <name>       DB Username" << endl;
    cout << "     -dbp <pw>         DB Password" << endl;

    cout << endl << "  OPTIONAL OPTIONS:" << endl;
    cout << "     -p <port>         BMP listening port (default is 5000)" << endl;
    cout << "     -dbn <name>       DB name (default is openBMP)" << endl;
    cout << endl;
    cout << "     -c <filename>     Config filename, default is " << CFG_FILENAME << endl;
    cout << "     -l <filename>     Log filename, default is STDOUT" << endl;
    cout << "     -d <filename>     Debug filename, default is log filename" << endl;
    cout << "     -pid <filename>   PID filename, default is no pid file" << endl;
    cout << "     -b <MB>           BMP read buffer per router size in MB (default is 15), range is 2 - 128" << endl;

    cout << endl << "  OTHER OPTIONS:" << endl;
    cout << "     -v                   Version" << endl;

    cout << endl << "  DEBUG OPTIONS:" << endl;
    cout << "     -dbgp             Debug BGP parser" <<  endl;
    cout << "     -dbmp             Debug BMP parser" << endl;
    cout << "     -dmysql           Debug mysql" << endl;

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
bool ReadCmdArgs(int argc, char **argv, Cfg_Options &cfg) {

    // Initialize the defaults
    cfg.bmp_port = const_cast<char *>("5000");
    cfg.dbName = const_cast<char *>("openBMP");
    cfg.dbURL = NULL;
    cfg.debug_bgp = false;
    cfg.debug_bmp = false;
    cfg.debug_mysql = false;
    cfg.password = NULL;
    cfg.username = NULL;
    cfg.bmp_buffer_size = 15728640; // 15MB

    // Make sure we have the correct number of required args
    if (argc > 1 and !strcmp(argv[1], "-v")) {   // Version
        cout << "openbmpd (www.openbmp.org) version : " << OPENBMPD_VERSION << endl;
        exit(0);
    }

    else if (argc < 4) {
        cout << "ERROR: Missing required args.";
        return true;
    }



    // Loop thorugh the args
    for (int i=1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {   // Help message
            return true;
        }

        else if (!strcmp(argv[i], "-dbu")) {
            // We expect the next arg to be a username
            if (i+1 >= argc) {
                cout << "INVALID ARG: -dbu expects a username" << endl;
                return true;
            }

            cfg.username = argv[++i];

        } else if (!strcmp(argv[i], "-dbp")) {
            // We expect the next arg to be a password
            if (i+1 >= argc) {
                cout << "INVALID ARG: -dbp expects a password" << endl;
                return true;
            }

            cfg.password = argv[++i];

        } else if (!strcmp(argv[i], "-p")) {
            // We expect the next arg to be a port
            if (i+1 >= argc) {
                cout << "INVALID ARG: -p expects a port number" << endl;
                return false;
            }

            cfg.bmp_port = argv[++i];

            // Validate the port
            if (atoi(cfg.bmp_port) < 25 || atoi(cfg.bmp_port) > 65535) {
                cout << "INVALID ARG: port '" << cfg.bmp_port << "' is out of range, expected range is 100-65535" << endl;
                return true;
            }

        } else if (!strcmp(argv[i], "-dbn")) {
            // We expect the next arg to be the database name
            if (i+1 >= argc) {
                cout << "INVALID ARG: -dbn expects a database name" << endl;
                return false;
            }

            cfg.dbName = argv[++i];

        } else if (!strcmp(argv[i], "-dburl")) {
            // We expect the next arg to be the database connection url
            if (i+1 >= argc) {
                cout << "INVALID ARG: -dburl expects the database connection url" << endl;
                return false;
            }

            cfg.dbURL = argv[++i];

        } else if (!strcmp(argv[i], "-b")) {
            // We expect the next arg to be the size in MB
            if (i+1 >= argc) {
                cout << "INVALID ARG: -b expects a value between 2 and 15" << endl;
                return false;
            }

            cfg.bmp_buffer_size = atoi(argv[++i]);

            // Validate the size
            if (cfg.bmp_buffer_size < 2 || cfg.bmp_buffer_size > 384) {
                cout << "INVALID ARG: port '" << cfg.bmp_buffer_size << "' is out of range, expected range is 2 - 384" << endl;
                return true;
            }

            // Convert the size to bytes
            cfg.bmp_buffer_size = cfg.bmp_buffer_size * 1024 * 1024;

        } else if (!strcmp(argv[i], "-dbgp")) {
            cfg.debug_bgp = true;
            debugEnabled = true;
        } else if (!strcmp(argv[i], "-dbmp")) {
            cfg.debug_bmp = true;
            debugEnabled = true;
        } else if (!strcmp(argv[i], "-dmysql")) {
            cfg.debug_mysql = true;
            debugEnabled = true;
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

    // Make sure we have the required ARGS
    if (cfg.dbURL == NULL || cfg.password == NULL || cfg.username == NULL) {
        cout << "INVALID ARGS: Missing the required args" << endl;
        return true;
    }

    return false;
}


/**
 * Run Server loop
 *
 * \param [in]  cfg    Reference to the config options
 */
void runServer(Cfg_Options &cfg) {
    mysqlBMP *mysql;
    int active_connections = 0;                 // Number of active connections/threads

    LOG_INFO("Starting server");

    try {

        // Test Mysql connection
        mysql = new mysqlBMP(logger, cfg.dbURL,cfg.username, cfg.password, cfg.dbName);
        delete mysql;

        // allocate and start a new bmp server
        BMPListener *bmp_svr = new BMPListener(logger, &cfg);

        // Loop to accept new connections
        while (1) {
            /*
             * Check for any stale threads/connections
             */
             for (size_t i=0; i < thr_list.size(); i++) {
                // If thread is not running, it means it terminated, so close it out
                if (!thr_list.at(i)->running) {

                    // Join the thread to clean up
                    pthread_join(thr_list.at(i)->thr, NULL);

                    // free the vector entry
                    delete thr_list.at(i);
                    thr_list.erase(thr_list.begin() + i);
                    --active_connections;
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

                pthread_attr_t thr_attr;            // thread attribute

                // wait for a new connection and accept
                LOG_INFO("Waiting for new connection, active connections = %d", active_connections);
                bmp_svr->accept_connection(thr->client);

                /*
                 * Start a new thread for every new router connection
                 */
                LOG_INFO("Client Connected => %s:%s, sock = %d",
                        thr->client.c_ipv4, thr->client.c_port, thr->client.c_sock);


                pthread_attr_init(&thr_attr);
                //pthread_attr_setdetachstate(&thr.thr_attr, PTHREAD_CREATE_DETACHED);
                pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
                thr->running = 1;


                // Start the thread to handle the client connection
                pthread_create(&thr->thr,  &thr_attr,
                        ClientThread, thr);

                // Add thread to vector
                thr_list.insert(thr_list.end(), thr);

                // Free attribute
                pthread_attr_destroy(&thr_attr);

                // Bump the current thread count
                ++active_connections;


            } else {
                LOG_WARN("Reached max number of threads, cannot accept new BMP connections at this time.");
                sleep (1);
            }
        }


    } catch (char const *str) {
        LOG_WARN(str);
    }
}

/**
 * main function
 */
int main(int argc, char **argv) {
    Cfg_Options cfg;

    // Process the command line args
    if (ReadCmdArgs(argc, argv, cfg)) {
        Usage(argv[0]);
        return 1;
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
    else {
        /*
        * Become a daemon if debug is not enabled
        */
        daemon(1,1);

        // Write PID to PID file if requested
        if (pid_filename != NULL) {
            pid_t pid = getpid();
            ofstream pfile (pid_filename);

            if (pfile.is_open()) {
                pfile << pid << endl;
                pfile.close();
            } else {
                LOG_ERR("Failed to write PID to %s", pid_filename);
                exit (1);
            }
        }
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

