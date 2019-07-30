#include "OpenBMP.h"
#include "CLI.h"
#include "Logger.h"
#include <iostream>
#include <csignal>
#include <unistd.h>

using namespace std;

// Global pointers
// needs logger to make logger macros (defined in Logger.h) work, e.g., LOG_NOTICE.
static Logger *logger;
// needs obmp object global so the signal handler can call stop() in obmp.
static OpenBMP *obmp;

// needs it to shutdown the program properly
void signal_handler(int signum) {
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
        case SIGCHLD : // Stop openbmp
            obmp->stop();
            LOG_INFO("Done closing all active BMP connections");
            exit(0);
        default:
            LOG_INFO("Ignoring signal %d", signum);
            break;
    }
}

int main(int argc, char **argv) {
    auto *config = Config::init();

    // Process cli args
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
    }

    // Initialize singleton Logger
    try {
        logger = Logger::init(config->log_filename, config->debug_filename);
    } catch (char const *str) {
        cout << "Failed to open log file for read/write : " << str << endl;
        return 2;
    }

    // Setup the signal handlers
    struct sigaction sigact{};
    sigact.sa_handler = signal_handler;
    sigact.sa_flags = 0;
    sigemptyset(&sigact.sa_mask);          // blocked signals while handler runs

    sigaction(SIGCHLD, &sigact, nullptr);
    sigaction(SIGHUP, &sigact, nullptr);
    sigaction(SIGTERM, &sigact, nullptr);
    sigaction(SIGPIPE, &sigact, nullptr);
    sigaction(SIGQUIT, &sigact, nullptr);
    sigaction(SIGINT, &sigact, nullptr);
    sigaction(SIGUSR1, &sigact, nullptr);
    sigaction(SIGUSR2, &sigact, nullptr);


    // Finally, we initialize OpenBMP and start the service.
    obmp = new OpenBMP();
    obmp->start();
//obmp->test();
//cout << "num of workers: " << obmp->workers.size() << endl;
    sleep(5);
    return 0;
}

