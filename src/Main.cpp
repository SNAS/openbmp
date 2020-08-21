#include "OpenBMP.h"
#include "CLI.h"
#include "Logger.h"
#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

// Global pointers
// needs logger to make logger macros (defined in Logger.h) work, e.g., LOG_NOTICE.
static Logger *logger;
// needs obmp object global so the signal handler can call stop() in obmp.
static OpenBMP *obmp;

// termination signal handling
static void sigterm (int sig) {
    LOG_INFO("Termination signal received %d", sig);
    obmp->stop();
}

/**
 * Daemonize the program
 */
void daemonize(const char *pid_filename) {
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
    if (pid_filename != nullptr) {
        pid = getpid();
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

int main(int argc, char **argv) {
    // Initialize Config (singleton)
    auto *config = Config::init();

    // Process CLI args
    if (CLI::ReadCmdArgs(argc, argv, config)) {
        return 1;
    }

    // Load config file
    if (config->cfg_filename != nullptr) {
        try {
            config->load(config->cfg_filename);
        } catch (char const *str) {
            cout << "ERROR: Failed to load the configuration file: " << str << endl;
            return 2;
        }
    } else {
        cout << "ERROR: Must specify the path to configuration file: " << endl;
        return 2;
    }

    cout << "loading logger" << endl;

    // Initialize Logger (singleton)
    try {
        logger = Logger::init(
                config->log_filename.empty() ? nullptr : config->log_filename.c_str(),
                config->debug_filename.empty() ? nullptr : config->debug_filename.c_str());
    } catch (char const *str) {
        cout << "Failed to open log file for read/write : " << str << endl;
        return 2;
    }

    // check if we can need to enable debug mode in the logger
    if (config->debug_all || config->debug_worker
        || config->debug_collector || config->debug_encapsulator
        || config->debug_message_bus) {
        logger->enableDebug();
    } else {
        logger->disableDebug();
    }

    // daemonize the program if needed
    if (config->daemon) {
        if(config->debug_all) {
            cout << "Sending the process to background" << endl;
        }
        daemonize(config->pid_filename.empty() ? nullptr : config->pid_filename.c_str());
    }

    // Setup the signal handlers
    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);

    // Finally, we initialize OpenBMP and start the service.
    obmp = new OpenBMP();
    obmp->start();

    delete logger;
    delete obmp;
    delete config;

    return 0;
}

