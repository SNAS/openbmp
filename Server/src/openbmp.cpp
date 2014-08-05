/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
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

#include "BMPServer.h"
#include "DbImpl_mysql.h"
#include "DbInterface.hpp"
#include "openbmpd_version.h"

#include <unistd.h>
#include <fstream>

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

/*
 * Configuration Options (global arg)
 */
BMPServer::Cfg_Options cfg;

// GLOBAL var - thread management structure
struct Thread_Mgmt {
    pthread_t thr;
    BMPServer::ClientInfo client;
    char running;                       // true if running, zero if not running
};

// Global thread list
vector<Thread_Mgmt *> thr_list(0);

// Create global bmp_server pointer
BMPServer *bmp_svr;


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
    cout << "     -c <filename>        Config filename, default is " << CFG_FILENAME << endl;
    cout << "     -l <filename>        Log filename, default is STDOUT" << endl;
    cout << "     -d <filename>        Debug filename, default is log filename" << endl;
    cout << "     -pid <filename>      PID filename, default is no pid file" << endl;

    cout << endl << "  OTHER OPTIONS:" << endl;
    cout << "     -v                   Version" << endl;

    cout << endl << "  DEBUG OPTIONS:" << endl;
    cout << "     -dbgp             Debug BGP parser" <<  endl;
    cout << "     -dbmp             Debug BMP parser" << endl;
    cout << "     -dmysql           Debug mysql" << endl;

}

/**
 * Parse and handle the command line args
 *
 * \returns true if error, false if no error
 *
 */
bool ReadCmdArgs(int argc, char **argv) {

    // Initialize the defaults
    cfg.bmp_port = const_cast<char *>("5000");
    cfg.dbName = const_cast<char *>("openBMP");
    cfg.dbURL = NULL;
    cfg.debug_bgp = false;
    cfg.debug_bmp = false;
    cfg.debug_mysql = false;
    cfg.password = NULL;
    cfg.username = NULL;

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
        else if (!strcmp(argv[i], "-p")) {
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
 * Client thread function
 *
 * Thread function that is called when starting a new thread.
 * The DB/mysql is initialized for each thread.
 *
 * @param [in]  arg     Pointer to the BMPServer ClientInfo
 */
/* ------------------------------------------------------------------
 * Client Thread function
 *     The client connection will be handled by this thread.
 * ------------------------------------------------------------------ */
void *ClientThread(void *arg) {
    pthread_t myid = pthread_self();

    // Setup the args
    BMPServer::ClientInfo *client = static_cast<BMPServer::ClientInfo *>(arg);

    Logger *log = bmp_svr->log;

    // connect to mysql
    mysqlBMP *mysql = new mysqlBMP(log, cfg.dbURL,cfg.username, cfg.password, cfg.dbName);

    if (cfg.debug_mysql)
        mysql->enableDebug();

    try {
        while (1) {
            bmp_svr->ReadIncomingMsg(client, (DbInterface *)mysql);
        }

    } catch (char const *str) {
        LOG_NOTICE("%s: Thread for sock [%d] ended", str, client->c_sock);
    }

    // Delete mysql
    delete mysql;

    // close the socket
    shutdown(client->c_sock, SHUT_RDWR);
    close (client->c_sock);

    // Indicate that we are no longer running
    for (size_t i=0; i < thr_list.size(); i++) {
        if (pthread_equal(thr_list.at(i)->thr, myid)) {
            thr_list.at(i)->running = 0;
            break;
        }
    }

    // Exit the thread
    pthread_exit(NULL);

    return NULL;
}

/**
 * main function
 */
int main(int argc, char **argv) {
    mysqlBMP *mysql;
    int active_connections = 0;                 // Number of active connections/threads

    // Process the command line args
    if (ReadCmdArgs(argc, argv)) {
        Usage(argv[0]);
        return 1;
    }

    // Initialize logging
    Logger *log = new Logger(log_filename, debug_filename);

    // Set up defaults for logging
    log->setWidthFilename(15);
    log->setWidthFunction(18);

    if (debugEnabled)
        log->enableDebug();
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


    LOG_INFO("Starting server");

	try {

	    // Test Mysql connection
        mysql = new mysqlBMP(log, cfg.dbURL,cfg.username, cfg.password, cfg.dbName);
        delete mysql;

        // allocate and start a new bmp server
        bmp_svr = new BMPServer(log, &cfg);

	    // Loop to accept new connections
	    while (1) {
            /*
             * Check for any stale connections
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
                Thread_Mgmt *thr = new Thread_Mgmt;
                pthread_attr_t thr_attr;            // thread attribute

	            // wait for a new connection and accept
                LOG_INFO("Waiting for new connection, active connections = %d", active_connections);
	            bmp_svr->accept_connection(thr->client);

	            /*
	             * Start a new thread for every new router connection
	             */
                LOG_INFO("Client Connected => %s:%d, sock = %d",
                        thr->client.c_ipv4, thr->client.c_port, thr->client.c_sock);


                // set thread detach state attribute to DETACHED
                pthread_attr_init(&thr_attr);
                //pthread_attr_setdetachstate(&thr.thr_attr, PTHREAD_CREATE_DETACHED);
                pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
                thr->running = 1;


                // Start the thread to handle the client connection
	            pthread_create(&thr->thr,  &thr_attr,
	                    ClientThread, &thr->client);

                // Add thread to vector
                thr_list.insert(thr_list.end(), thr);

	            // Free attribute
	            pthread_attr_destroy(&thr_attr);


	            // Bump the current thread count
	            // TODO: Need to remove threads when they are gone
                ++active_connections;

	        } else {
	            LOG_WARN("Reached max number of threads, cannot accept new BMP connections at this time.");
	            sleep (1);
	        }
	    }


	} catch (char const *str) {
	    cout << str << endl;
	}

	LOG_NOTICE("Program ended normally");

	return 0;
}

