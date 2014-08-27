/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef MYSQLBMP_H_
#define MYSQLBMP_H_

#define HASH_SIZE 16

#include "DbInterface.hpp"
#include "Logger.h"
#include <string>
#include <map>
#include <vector>
#include <ctime>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>


/**
 * \class   mysqlMBP
 *
 * \brief   Mysql database implementation
 * \details Enables a DB backend using mysql 5.5 or greater.
  */
class mysqlBMP: public DbInterface {
public:

    /******************************************************************//**
     * \brief This function will initialize and connect to MySQL.  
     *
     * \details It is expected that this class will start off with a new connection.
     *
     *  \param [in] logPtr      Pointer to Logger instance
     *  \param [in] hostURL     mysql HOST URL such as "tcp://10.1.1.1:3306"
     *  \param [in] username    the mysql username
     *  \param [in] password    the mysql password
     *  \param [in] db          the mysql database name
     ********************************************************************/
    mysqlBMP(Logger *logPtr, char *hostURL, char *username, char *password, char *db);
    ~mysqlBMP();

    /*
     * abstract methods implemented
     * See DbInterface.hpp for method details
     */
    void add_Peer(tbl_bgp_peer &peer);
    bool update_Peer(tbl_bgp_peer &peer);
    void add_Router(struct tbl_router &r_entry);
    bool update_Router(struct tbl_router &r_entry);
    bool disconnect_Router(struct tbl_router &r_entry);
    void add_Rib(std::vector<tbl_rib> &rib);
    void delete_Rib(std::vector<tbl_rib> &rib);
    void add_PathAttrs(tbl_path_attr &path);
    void add_StatReport(tbl_stats_report &stats);
    void add_PeerDownEvent(tbl_peer_down_event &down_event);
    void add_PeerUpEvent(tbl_peer_up_event &up_event);

    // Debug methods
    void enableDebug();
    void disableDebug();

private:
    bool            debug;                      ///< debug flag to indicate debugging
    Logger          *log;                       ///< Logging class pointer

    sql::Driver     *driver;
    sql::Connection *con;
    sql::Statement  *stmt;
    sql::ResultSet  *res;

    // array of hashes
    typedef std::map<std::string, time_t>::iterator router_list_iter;
    std::map<std::string, time_t> router_list;
    std::map<std::string, time_t> peer_list;
    typedef std::map<std::string, time_t>::iterator peer_list_iter;
    /**
     * Connects to mysql server
     *
     * \param [in]  hostURL     Host URL, such as tcp://127.0.0.1:3306
     * \param [in]  username    DB username, such as openbmp
     * \param [in]  password    DB user password, such as openbmp
     * \param [in]  db          DB name, such as openBMP
     */
    void mysqlConnect(char *hostURL, char *username, char *password, char *db);

};

#endif /* MYSQLBMP_H_ */
