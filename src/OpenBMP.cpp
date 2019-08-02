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

    debug = config->debug_collector;

    /*********************************************
        set up server socket related variables
     *********************************************/
    sock = 0;
    sock_v6 = 0;

    // collector ipv4 configuration
    collector_addr.sin_family = PF_INET;
    collector_addr.sin_port = htons(config->bmp_port);
    if (config->bind_ipv4.length())
        inet_pton(AF_INET, config->bind_ipv4.c_str(), &(collector_addr.sin_addr.s_addr));
    else
        collector_addr.sin_addr.s_addr = INADDR_ANY;

    // collector ipv6 configuration
    collector_addr_v6.sin6_family = AF_INET6;
    collector_addr_v6.sin6_port = htons(config->bmp_port);
    collector_addr_v6.sin6_scope_id = 0;
    if (config->bind_ipv6.length())
        inet_pton(AF_INET6, config->bind_ipv6.c_str(), &(collector_addr_v6.sin6_addr));
    else
        collector_addr_v6.sin6_addr = in6addr_any;
}

void OpenBMP::test() {
    Worker *w1 = new Worker();
    workers.emplace_back(w1);
}

void OpenBMP::start() {
    // connect to kafka server
    // TODO: make sure the connect() will block the code until the message bus is connected.
    message_bus->connect();
    // open server tcp socket
    open_server_socket(config->svr_ipv4, config->svr_ipv6);

    // all dependencies have been initialized, set running status to true.
    running = true;

    /*************************************
     * openbmp server routine
     *************************************/
    // worker pointer that points to a worker who needs a job.
    auto *worker = new Worker();
    while (running) {

        // remove all stopped workers
        remove_dead_workers();

        // check if we can accept new connections
        if (can_accept_bmp_connection()) {
            // check for any new tcp connection.
            // if there is one, we accept the connection,
            // and hand it over to the worker
            find_bmp_connection(worker);

            // check if the current worker has a job (established tcp connection).
            if (worker->is_running()) {
                // if it has a job, we save the worker to workers,
                workers.emplace_back(worker);
                // and instantiate a new worker to accept new bmp connection.
                worker = new Worker();
            }
        } else {
            // sleep for a second, then start all over.
            sleep(1);
        }
    }

    /*************************************
     * openbmp server has stopped
     * cleanup procedures
     *************************************/
    // stop all worker nodes
    for (auto worker: workers) worker->stop();
    // disconnect message bus
    message_bus->disconnect();
    // close sockets?
}

void OpenBMP::stop() {
    // set running status to false to stop openbmp server routine.
    running = false;
}

/**
 * Opens server (v4 or 6) listening socket(s)
 *
 * \param [in] ipv4     True to open v4 socket
 * \param [in] ipv6     True to open v6 socket
 */
void OpenBMP::open_server_socket(bool ipv4, bool ipv6) {
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
        if (::bind(sock, (struct sockaddr *) &collector_addr, sizeof(collector_addr)) < 0) {
            close(sock);
            throw "ERROR: Cannot bind to IPv4 address and port";
        }

        // listen for incoming connections
        listen(sock, 10);
    }

    if (ipv6) {
        if ((sock_v6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
            throw "ERROR: Cannot open IPv6 socket.";
        }

        // Set socket options
        if (setsockopt(sock_v6, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            close(sock_v6);
            throw "ERROR: Failed to set IPv6 socket option SO_REUSEADDR";
        }

        if (setsockopt(sock_v6, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
            close(sock_v6);
            throw "ERROR: Failed to set IPv6 socket option IPV6_V6ONLY";
        }

        // Bind to the address/port
        if (::bind(sock_v6, (struct sockaddr *) &collector_addr_v6, sizeof(collector_addr_v6)) < 0) {
            close(sock_v6);
            throw "ERROR: Cannot bind to IPv6 address and port";
        }

        // listen for incoming connections
        listen(sock_v6, 10);
    }
}

int OpenBMP::get_num_of_active_connections() {
    int count = 0;
    for (auto worker: workers) {
        if (worker->is_running())
            count++;
    }
    return count;
}

// checks for any bmp connection, if it finds one,
// it hands over the connection to a worker.
void OpenBMP::find_bmp_connection(Worker *worker) {
    pollfd pfd[4];
    memset(pfd, 0 , sizeof(pfd));
    int fds_cnt = 0;
    int cur_sock = 0;
    bool close_sock = false;

    if (sock > 0) {
        pfd[fds_cnt].fd = sock;
        pfd[fds_cnt].events = POLLIN | POLLHUP | POLLERR;
        pfd[fds_cnt].revents = 0;
        fds_cnt++;
    }

    if (sock_v6 > 0) {
        pfd[fds_cnt].fd = sock_v6;
        pfd[fds_cnt].events = POLLIN | POLLHUP | POLLERR;
        pfd[fds_cnt].revents = 0;
        fds_cnt++;
    }

    // Check if the listening socket has a new connection
    if (poll(pfd, fds_cnt + 1, 1000)) {
        for (int i = 0; i < fds_cnt; i++) {
            if (pfd[i].revents & POLLHUP or pfd[i].revents & POLLERR) {
                LOG_WARN("sock=%d: received POLLHUP/POLLHERR while accepting", pfd[i].fd);
                cur_sock = pfd[i].fd;
                close_sock = true;
                break;

            } else if (pfd[i].revents & POLLIN) {
                cur_sock = pfd[i].fd;
                break;
            }
        }
    }

    // get the active socket
    int active_socket = 0;
    if (cur_sock > 0) {
        if (close_sock) {
            if (cur_sock == sock)  close(sock);
            else if (cur_sock == sock_v6)  close(sock_v6);
        } else {
            cout << "we found a bmp connection request, now try to establish the connection." << endl;
            // v4 socket is active
            if (cur_sock == sock) {
                active_socket = sock;
                // hand over the active socket to the worker who needs a job.
                worker->start(active_socket, true);
            }
            // v6 socket is active
            else {
                active_socket = sock_v6;
                // hand over the active socket to the worker who needs a job.
                worker->start(active_socket, false);
            }
        }
    }

}

// check if the collector has enough head room to accept new bmp connections.
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
    for (int i = 0; i < workers.size(); i++) {
        if (workers.at(i)->has_stopped()) {
            delete workers.at(i);
            workers.erase(workers.begin() + i);
        }
    }
}
