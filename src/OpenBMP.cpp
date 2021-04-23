/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 * Copyright (c) 2019 Lumin Shi.  All rights reserved.
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <chrono>

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
    // set debug flag
    debug = config->debug_collector | config->debug_all;

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

void OpenBMP::start() {
    // connect to kafka server
    message_bus->connect();
    // open server tcp socket
    try {
        open_server_socket(config->svr_ipv4, config->svr_ipv6);
    } catch (const char * err) {
        LOG_ERR(err);
    }

    // all dependencies have been initialized, set running status to true.
    running = true;

    cpu_mon_thread = thread(&OpenBMP::cpu_usage_monitor, this);

    /*************************************
     * openbmp server routine
     *************************************/
     // instantiate a encapsulator just for sending the heartbeat msgs.
     // could be improved maybe?
     Encapsulator encapsulator = Encapsulator();
     string router_ip, router_hostname;
     TopicBuilder topicbuilder = TopicBuilder(router_ip, router_hostname);
     string collector_topic_string = topicbuilder.get_collector_topic_string();

    // worker pointer that points to a worker who needs a job.
    auto *worker = new Worker();

    auto last_heartbeat_timestamp = chrono::high_resolution_clock::now();
    while (running) {
        // remove all stopped workers
        remove_dead_workers();

        // check if we need to send a collector (heartbeat) msg
        auto current_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(current_time - last_heartbeat_timestamp);
        // if the last heartbeat msg was sent more than heartbeat_interval ago,
        //  we send another one.
        if (duration.count() >= (config->heartbeat_interval)) {
            // update heartbeat time
            encapsulator.build_encap_collector_msg();
            uint8_t *collector_msg = encapsulator.get_encap_collector_msg();
            int collector_msg_len = encapsulator.get_encap_collector_msg_size();
            message_bus->send(collector_topic_string, collector_msg, collector_msg_len);
            last_heartbeat_timestamp = current_time;
            if (debug) {
                LOG_DEBUG("sent a heartbeat msg.");
            }
        }

        // check if we can accept new connections
        if (can_accept_bmp_connection()) {
            // check for any new tcp connection.
            // if there is one, we accept the connection,
            // and hand it over to the worker
            // note that find_bmp_connection() checks connection for 1 sec.
            find_bmp_connection(worker);

            // check if the current worker has a job (established tcp connection).
            if (worker->is_running()) {
                // if it has a job, we save the worker to worker list (workers),
                workers.emplace_back(worker);
                // and instantiate a new worker to accept the future bmp connection.
                worker = new Worker();
            }
        } else {
            // sleep for a second, then start all over.
            sleep(1);
        }
        sleep(5);
    }

}

void OpenBMP::stop() {
    // set running status to false to stop openbmp server routine.
    running = false;

    /*************************************
     * openbmp server was signaled to stop
     * cleanup procedures
     *************************************/
    // stop all worker nodes
    LOG_INFO("stopping openbmp.");
    for (auto w: workers) w->stop();

    // send msg bus stop signal to cancel while loops in the msgbus.send()
    message_bus->stop();
    delete message_bus;
    LOG_INFO("msg bus stopped.");

    // join cpu util mon
    cpu_mon_thread.join();
    LOG_INFO("cpu monitor stopped.");

    LOG_INFO("openbmp server stopped.");
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
            if (debug) {
                DEBUG("found a bmp connection request, establishing the connection.");
            }
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
    int rib_waiting_workers = get_rib_dump_waiting_worker_num();
    if (debug) {
        LOG_DEBUG("%d worker(s) in router rib dump waiting state", rib_waiting_workers);
    }
    if (rib_waiting_workers >= config->max_rib_waiting_workers) {
        return false;
    }
    return true;
}

bool OpenBMP::did_not_affect_rib_dump_rate() {
    // TODO: implement conditions to accept new connections here.
    return true;
}

void OpenBMP::remove_dead_workers() {
    for (size_t i = 0; i < workers.size(); i++) {
        if (workers.at(i)->has_stopped()) {
            workers.at(i)->stop();
            delete workers.at(i);
            workers.erase(workers.begin() + i);
        }
    }
}

void OpenBMP::cpu_usage_monitor() {
    while (running) {
        cpu_util = Utility::get_avg_cpu_util();
        if (debug) {
            LOG_DEBUG("avg cpu util (%): %f", cpu_util);
        }
    }
}

int OpenBMP::get_rib_dump_waiting_worker_num() {
    int count = 0;
    for (auto w: workers) {
        if (!w->has_rib_dump_started()){
            count++;
        }
    }
    return count;
}
