#include "OpenBMP.h"
#include "CLI.h"
#include <iostream>
#include <csignal>
#include <cstring>

using namespace std;


int main(int argc, char **argv) {
    OpenBMP obmp = OpenBMP();

    // Process the command line args
    if (CLI::ReadCmdArgs(argc, argv, obmp.config)) {
        return 1;
    }

    if (obmp.config.cfg_filename != nullptr) {
        try {
            obmp.config.load(obmp.config.cfg_filename);

        } catch (char const *str) {
            cout << "ERROR: Failed to load the configuration file: " << str << endl;
            return 2;
        }
    }

    obmp.test();
    cout << "num of workers: " << obmp.workers.size() << endl;
    return 0;
}

