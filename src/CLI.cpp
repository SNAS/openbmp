#include "CLI.h"
#include <iostream>

using namespace std;

/**
 * Usage of the program
 */
void CLI::Usage(char *prog) {
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
    cout << "     -h                   Help" << endl;


    cout << endl << "  DEBUG OPTIONS:" << endl;
    cout << "     -debug"           "Debug general items" << endl;
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

    cout << endl;
}

/**
 * Parse and handle the command line args
 *
 * \param [out] cfg    Reference to config options - Will be updated based on cli
 *
 * \returns true if error, false if no error
 *
 */
bool CLI::ReadCmdArgs(int argc, char **argv, Config &cfg) {

    if (argc > 1 and !strcmp(argv[1], "-h")) {
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

            cfg.bmp_ring_buffer_size = atoi(argv[++i]);

            // Validate the size
            if (cfg.bmp_ring_buffer_size < 2 || cfg.bmp_ring_buffer_size > 384) {
                cout << "INVALID ARG: port '" << cfg.bmp_ring_buffer_size <<
                     "' is out of range, expected range is 2 - 384" << endl;
                return true;
            }

            // Convert the size to bytes
            cfg.bmp_ring_buffer_size = cfg.bmp_ring_buffer_size * 1024 * 1024;

        } else if (!strcmp(argv[i], "-debug")) {
            cfg.debug_general = true;
        } else if (!strcmp(argv[i], "-dbmp")) {
            cfg.debug_bmp = true;
        } else if (!strcmp(argv[i], "-dmsgbus")) {
            cfg.debug_msgbus = true;

        } else if (!strcmp(argv[i], "-f")) {
            cfg.run_foreground = true;
        }

            // Config filename
        else if (!strcmp(argv[i], "-c")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -c expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            cfg.cfg_filename = argv[++i];
        }

            // Log filename
        else if (!strcmp(argv[i], "-l")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -l expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            cfg.log_filename = argv[++i];
        }

            // Debug filename
        else if (!strcmp(argv[i], "-d")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -d expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            cfg.debug_filename = argv[++i];
        }

            // PID filename
        else if (!strcmp(argv[i], "-pid")) {
            // We expect the next arg to be the filename
            if (i + 1 >= argc) {
                cout << "INVALID ARG: -p expects the filename to be specified" << endl;
                return true;
            }

            // Set the new filename
            cfg.pid_filename = argv[++i];
        }
    }

    return false;
}

/**
 * Signal handler
 *
 */
//void signal_handler(int signum)
//{
//    LOG_NOTICE("Caught signal %d", signum);

//    /*
//     * Respond based on the signal
//     */
//    switch (signum) {
//        case SIGTERM :
//        case SIGKILL :
//        case SIGQUIT :
//        case SIGPIPE :
//        case SIGINT  :
//        case SIGCHLD : // Handle the child cleanup

//            for (size_t i=0; i < thr_list.size(); i++) {
//                if (thr_list.at(i)->running) {
//                    pthread_cancel(thr_list.at(i)->thr);
//                    thr_list.at(i)->running = false;
//                    pthread_join(thr_list.at(i)->thr, NULL);
//                }
//            }

//            thr_list.clear();

//            LOG_INFO("Done closing all active BMP connections");

//            run = false;
//            exit(0);
//            break;

//        default:
//            LOG_INFO("Ignoring signal %d", signum);
//            break;
//    }
//}
