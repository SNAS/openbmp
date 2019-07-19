#ifndef OPENBMP_OPENBMP_H
#define OPENBMP_OPENBMP_H

#include "Config/Config.h"


class OpenBMP {
public:
    OpenBMP();
    Config config;
    void start();
    void stop();
    int get_num_of_active_connections();
private:
    void accept_bmp_connection();
    void create_worker(OpenBMP obmp);
};


#endif //OPENBMP_OPENBMP_H
