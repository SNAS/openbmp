#include <iostream>
#include "OpenBMP.h"

using namespace std;

OpenBMP::OpenBMP() {}

void OpenBMP::test() {
    Worker w1 = Worker(this);
    workers.emplace_back(w1);
}

void OpenBMP::start() {}
void OpenBMP::stop() {}
int OpenBMP::get_num_of_active_connections() {}
void OpenBMP::accept_bmp_connection() {}
void OpenBMP::create_worker(OpenBMP* obmp) {}



