#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <openssl/md5.h>

#include "OpenBMP.h"


using namespace std;

OpenBMP::OpenBMP() {
    // set up config
    config = Config::get_config();
    // set up logger
    logger = Logger::get_logger();
    // set up message bus after the logger is initialized.
    message_bus = MessageBus::init();

    // set server running status
    running = false;

    /*
     * set up server socket related variables
     */
    sock = 0;
    sockv6 = 0;
    debug = false;

    if (config->debug_collector)
        debug = true;

    svr_addr.sin_family      = PF_INET;
    svr_addr.sin_port        = htons(config->bmp_port);

    if(config->bind_ipv4.length()) {
        inet_pton(AF_INET, config->bind_ipv4.c_str(), &(svr_addr.sin_addr.s_addr));
    } else {
        svr_addr.sin_addr.s_addr = INADDR_ANY;
    }

    svr_addrv6.sin6_family   = AF_INET6;
    svr_addrv6.sin6_port     = htons(config->bmp_port);
    svr_addrv6.sin6_scope_id = 0;

    if(config->bind_ipv6.length()) {
        inet_pton(AF_INET6, config->bind_ipv6.c_str(), &(svr_addrv6.sin6_addr));
    } else {
        svr_addrv6.sin6_addr = in6addr_any;
    }
}

void OpenBMP::test() {
    Worker w1 = Worker();
    workers.emplace_back(w1);
}

void OpenBMP::start() {
    running = true;
    open_socket(config->svr_ipv4, config->svr_ipv6);
    while (running) {
        remove_dead_workers();

    }
}

void OpenBMP::stop() {
    running = false;
}

/**
 * Opens server (v4 or 6) listening socket(s)
 *
 * \param [in] ipv4     True to open v4 socket
 * \param [in] ipv6     True to open v6 socket
 */
void OpenBMP::open_socket(bool ipv4, bool ipv6) {
    int on = 1;

    if (ipv4) {
        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            throw "ERROR: Cannot open IPv4 socket.";
        }

        // Set socket options
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            close(sock);
            throw "ERROR: Failed to set IPv4 socket option SO_REUSEADDR";
        }

        // Bind to the address/port
        if (::bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr)) < 0) {
            close(sock);
            throw "ERROR: Cannot bind to IPv4 address and port";
        }

        // listen for incoming connections
        listen(sock, 10);
    }

    if (ipv6) {
        if ((sockv6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
            throw "ERROR: Cannot open IPv6 socket.";
        }

        // Set socket options
        if (setsockopt(sockv6, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            close(sockv6);
            throw "ERROR: Failed to set IPv6 socket option SO_REUSEADDR";
        }

        if (setsockopt(sockv6, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
            close(sockv6);
            throw "ERROR: Failed to set IPv6 socket option IPV6_V6ONLY";
        }

        // Bind to the address/port
        if (::bind(sockv6, (struct sockaddr *) &svr_addrv6, sizeof(svr_addrv6)) < 0) {
            close(sockv6);
            throw "ERROR: Cannot bind to IPv6 address and port";
        }

        // listen for incoming connections
        listen(sockv6, 10);
    }
}

int OpenBMP::get_num_of_active_connections() {}

void OpenBMP::accept_bmp_connection() {}

void OpenBMP::create_worker() {}

bool OpenBMP::can_accept_bmp_connection() {
    return below_max_cpu_utilization_threshold() & did_not_affect_rib_dump_rate();
}

bool OpenBMP::below_max_cpu_utilization_threshold() {
    // TODO: implement conditions to accept new connections here.
    return true;
}

bool OpenBMP::did_not_affect_rib_dump_rate() {
    // TODO: implement conditions to accept new connections here.
    return true;
}

void OpenBMP::remove_dead_workers() {

}



