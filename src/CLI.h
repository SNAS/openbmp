//
// Created by Lumin Shi on 2019-07-22.
//

#ifndef OPENBMP_CLI_H
#define OPENBMP_CLI_H


#include "Config.h"

class CLI {
public:
    static bool ReadCmdArgs(int argc, char **argv, Config *cfg);
private:
    static void Usage(char *prog);
};


#endif //OPENBMP_CLI_H
