/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <string>

#include <cinttypes>

// MySQL headers
#include "mysql_connection.h"
#include "mysql_driver.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include "DbImpl_mysql.h"
#include "md5.h"


using namespace std;

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
mysqlBMP::mysqlBMP(Logger *logPtr, char *hostURL, char *username, char *password, char *db) {
    // Initialize some vars
    res = NULL;
    stmt = NULL;
    con = NULL;

    logger = logPtr;

    disableDebug();
    setMaxBlobSize(8192);

    // Make the connection to the server
    mysqlConnect(hostURL, username, password, db);
}

/**
 * Destructor
 */
mysqlBMP::~mysqlBMP() {

    /*
     * Disconnect the router entries in the DB normally
     */
    char buf[4096]; // Misc working buffer
    for (router_list_iter it = router_list.begin(); it != router_list.end(); it++) {
        try {


            // Build the query
            snprintf(buf, sizeof(buf),
                    "UPDATE %s SET isConnected=0,term_reason_code=65535,term_reason_text=\"OpenBMP server stopped\" where hash_id = '%s'",
                    TBL_NAME_ROUTERS,
                    it->first.c_str());

            // Run the query to add the record
            stmt = con->createStatement();
            stmt->execute(buf);

            // Free the query statement
            delete stmt;
        } catch (sql::SQLException &e) {
            LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                    e.what(), e.getErrorCode(), e.getSQLState().c_str() );
        }
    }

    // Disconnect the driver
    driver->threadEnd();

    // Free vars
    if (con != NULL) {
        delete con;
    }

    // Set back to NULL
    res = NULL;
    stmt = NULL;
    con = NULL;
}

/**
 * Connects to mysql server
 *
 * \param [in]  hostURL     Host URL, such as tcp://127.0.0.1:3306
 * \param [in]  username    DB username, such as openbmp
 * \param [in]  password    DB user password, such as openbmp
 * \param [in]  db          DB name, such as openBMP
 */
void mysqlBMP::mysqlConnect(char *hostURL, char *username, char *password,
        char *db) {

    try {
        // get direct instance
        driver = get_driver_instance();
        //driver = sql::mysql::get_driver_instance();

        // Set the connection properties
        sql::ConnectOptionsMap connection_properties;

        connection_properties["OPT_RECONNECT"] = true;
        connection_properties["hostName"] = sql::SQLString(hostURL);
        connection_properties["userName"] = sql::SQLString(username);
        connection_properties["password"] = sql::SQLString(password);
        connection_properties["schema"] = sql::SQLString(db);

        //con = driver->connect(hostURL, username,password);
        con = driver->connect(connection_properties);

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
        /*
        cout << "mysqlBMP: # ERR: SQLException in " << __FILE__;
        cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
        cout << "mysqlBMP: # ERR: " << e.what();
        cout << " (MySQL error code: " << e.getErrorCode();
        cout << ", SQLState: " << e.getSQLState() << " )" << endl;
        */
        throw "ERROR: Cannot connect to mysql.  Check mysql server host and credentials.";
    }

}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_Peer(tbl_bgp_peer &p_entry) {
    try {
        char buf[4096]; // Misc working buffer

        /*
         * Generate peer table hash from the following fields
         *  p_entry.router_hash_id, p_entry.peer_rd, p_entry.peer_addr,
         *         p_entry.peer_bgp_id
         *
         */
        MD5 hash;

        // Generate the hash
        hash.update(p_entry.router_hash_id, HASH_SIZE);
        hash.update((unsigned char *) p_entry.peer_rd, strlen(p_entry.peer_rd));
        hash.update((unsigned char *) p_entry.peer_addr,
                strlen(p_entry.peer_addr));
        hash.update((unsigned char *) p_entry.peer_bgp_id,
                strlen(p_entry.peer_bgp_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(p_entry.hash_id, hash_raw, 16);
        delete[] hash_raw;

        // Convert binary hash to string
        string p_hash_str;
        hash_toStr(p_entry.hash_id, p_hash_str);
        string r_hash_str;
        hash_toStr(p_entry.router_hash_id, r_hash_str);

        // Check if we have already processed this entry, if so update it an return
        if (peer_list.find(p_hash_str) != peer_list.end()) {
            peer_list[p_hash_str] = time(NULL);
            return;
        }

        // Insert/Update map entry
        peer_list[p_hash_str] = time(NULL);

        // Build the query
        snprintf(buf, sizeof(buf),
                "REPLACE into %s (%s) values ('%s','%s','%s',%d, '%s', '%s', %u, %d, %d, current_timestamp,1)",
                TBL_NAME_BGP_PEERS,
                "hash_id,router_hash_id, peer_rd,isIPv4,peer_addr,peer_bgp_id,peer_as,isL3VPNpeer,isPrePolicy,timestamp,state",
                p_hash_str.c_str(), r_hash_str.c_str(), p_entry.peer_rd,
                p_entry.isIPv4, p_entry.peer_addr, p_entry.peer_bgp_id,
                p_entry.peer_as, p_entry.isL3VPN, p_entry.isPrePolicy);

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
bool mysqlBMP::update_Peer(tbl_bgp_peer &p_entry) {
    string p_hash_str;
    hash_toStr(p_entry.hash_id, p_hash_str);

    // Check if the peer exists, if so purge it from the list so it can be added/updated again
    if (peer_list.find(p_hash_str) != peer_list.end()) {
        peer_list.erase(p_hash_str);

        add_Peer(p_entry);

        return true;
    }

    return false;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_Router(tbl_router &r_entry) {
    try {
        char buf[4096]; // Misc working buffer

        /*
         * Generate router table hash from the following fields
         *     r_entry.name, r_entry.src_addr
         *
         */
        MD5 hash;

        // Generate the hash
        //hash.update (r_entry.name, strlen((char *)r_entry.name));
        hash.update(r_entry.src_addr, strlen((char *) r_entry.src_addr));
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(r_entry.hash_id, hash_raw, 16);
        delete [] hash_raw;

        // Convert binary hash to string
        string r_hash_str;
        hash_toStr(r_entry.hash_id, r_hash_str);

        // Check if we have already processed this entry, if so update it an return
        if (router_list.find(r_hash_str) != router_list.end()) {
            router_list[r_hash_str] = time(NULL);
            return;
        }

        // Insert/Update map entry
        router_list[r_hash_str] = time(NULL);

        // Convert the init data to string for storage
        string initData(r_entry.initiate_data);
        std::replace(initData.begin(), initData.end(), '\'', '"');

        // Build the query
        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s) values ('%s', '%s', '%s','%s','%s')",
                TBL_NAME_ROUTERS, "hash_id,name,description,ip_address,init_data", r_hash_str.c_str(),
                r_entry.name, r_entry.descr, r_entry.src_addr, initData.c_str());

        // Add the on duplicate statement
        strcat(buf, " ON DUPLICATE KEY UPDATE timestamp=current_timestamp,isConnected=1,name=values(name),description=values(description),init_data=values(init_data)");

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Update all peers to indicate peers are not up - till proven other wise
        snprintf(buf, sizeof(buf), "UPDATE %s SET state=0 where router_hash_id='%s'",
                TBL_NAME_BGP_PEERS, r_hash_str.c_str());
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );

    }
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
bool mysqlBMP::update_Router(tbl_router &r_entry) {
    string r_hash_str;
    hash_toStr(r_entry.hash_id, r_hash_str);

    // Check if the router exists, if so purge it from the list so it can be added/updated again
    if (router_list.find(r_hash_str) != router_list.end()) {
        router_list.erase(r_hash_str);

        add_Router(r_entry);

        return true;
    }

    return false;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
bool mysqlBMP::disconnect_Router(tbl_router &r_entry) {
    string r_hash_str;
    hash_toStr(r_entry.hash_id, r_hash_str);

    // Check if the router exists, if so purge it from the list so it can be added/updated again
    if (router_list.find(r_hash_str) != router_list.end()) {
        router_list.erase(r_hash_str);

        try {
            // Convert the term data to string for storage
            string termData (r_entry.term_data);
            std::replace(termData.begin(), termData.end(), '\'', '"');

            char buf[4096]; // Misc working buffer

            // Build the query
            snprintf(buf, sizeof(buf),
                    "UPDATE %s SET isConnected=0,term_reason_code=%" PRIu16 ",term_reason_text=\"%s\",term_data='%s' where hash_id = '%s'",
                    TBL_NAME_ROUTERS,
                    r_entry.term_reason_code, r_entry.term_reason_text, termData.c_str(),
                    r_hash_str.c_str());

            // Run the query to add the record
            stmt = con->createStatement();
            stmt->execute(buf);

            // Free the query statement
            delete stmt;

            return true;
        } catch (sql::SQLException &e) {
            LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                    e.what(), e.getErrorCode(), e.getSQLState().c_str() );
        }
    }

    return false;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_Rib(vector<tbl_rib> &rib_entry) {
    char    *buf = new char[800000];            // Misc working buffer
    char    buf2[4096];                         // Second working buffer
    size_t  buf_len = 0;                        // query buffer length

    try {

        // Build the initial part of the query
        //buf_len = sprintf(buf, "REPLACE into %s (%s) values ", TBL_NAME_RIB,
        //        "hash_id,path_attr_hash_id,peer_hash_id,prefix, prefix_len");
        buf_len = sprintf(buf, "INSERT into %s (%s) values ", TBL_NAME_RIB,
                "hash_id,path_attr_hash_id,peer_hash_id,prefix, prefix_len,timestamp");

        string rib_hash_str;
        string path_hash_str;
        string p_hash_str;

        // Loop through the vector array of rib entries
        for (size_t i = 0; i < rib_entry.size(); i++) {
            /*
             * Generate router table hash from the following fields
             *     rib_entry.peer_hash_id, rib_entry.prefix, rib_entry.prefix_len
             *
             */
            MD5 hash;

            // Generate the hash

            hash.update(rib_entry[i].peer_hash_id, HASH_SIZE);
            hash.update((unsigned char *) rib_entry[i].prefix,
                    strlen(rib_entry[i].prefix));
            hash.update(&rib_entry[i].prefix_len,
                    sizeof(rib_entry[i].prefix_len));
            hash.finalize();

            // Save the hash
            unsigned char *hash_raw = hash.raw_digest();
            memcpy(rib_entry[i].hash_id, hash_raw, 16);
            delete[] hash_raw;

            // Build the query
            hash_toStr(rib_entry[i].hash_id, rib_hash_str);
            hash_toStr(rib_entry[i].path_attr_hash_id, path_hash_str);
            hash_toStr(rib_entry[i].peer_hash_id, p_hash_str);

            buf_len += snprintf(buf2, sizeof(buf2),
                    " ('%s','%s','%s','%s', %d, from_unixtime(%u)),", rib_hash_str.c_str(),
                    path_hash_str.c_str(), p_hash_str.c_str(),
                    rib_entry[i].prefix, rib_entry[i].prefix_len,
                    rib_entry[i].timestamp_secs);

            // Cat the entry to the query buff
            if (buf_len < 800000 /* size of buf */)
                strcat(buf, buf2);
        }

        // Remove the last comma since we don't need it
        buf[buf_len - 1] = 0;

        // Add the on duplicate statement
        snprintf(buf2, sizeof(buf2),
                " ON DUPLICATE KEY UPDATE timestamp=values(timestamp),path_attr_hash_id=values(path_attr_hash_id),db_timestamp=current_timestamp");
        strcat(buf, buf2);


        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::delete_Rib(vector<tbl_rib> &rib_entry) {
    char    *buf = new char[800000];            // Misc working buffer
    char    buf2[4096];                         // Second working buffer
    size_t  buf_len = 0;                        // query buffer length

    try {
        /*
         * Update the current RIB entry state
         */
        // Build the initial part of the query
        buf_len = sprintf(buf, "DELETE from %s WHERE ", TBL_NAME_RIB);

        string p_hash_str;

        // Loop through the vector array of rib entries
        for (size_t i = 0; i < rib_entry.size(); i++) {

            hash_toStr(rib_entry[i].peer_hash_id, p_hash_str);

            buf_len +=
                    snprintf(buf2, sizeof(buf2),
                            " (peer_hash_id = '%s' and prefix = '%s' and prefix_len = %d) OR ",
                            p_hash_str.c_str(), rib_entry[i].prefix,
                            rib_entry[i].prefix_len);

            // Cat the entry to the query buff
            if (buf_len < 800000 /* size of buf */)
                strcat(buf, buf2);

        }

        // Remove the last OR since we don't need it
        buf[buf_len - 3] = 0;

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to update the record
        stmt = con->createStatement();
        stmt->execute(buf);

        /*
         * Insert a new withdrawn log entry
         */
        // Build the initial part of the query
        buf_len = sprintf(buf, "INSERT into %s (%s) values ",
                TBL_NAME_WITHDRAWN_LOG, "peer_hash_id,prefix, prefix_len");

        // Loop through the vector array of rib entries
        for (size_t i = 0; i < rib_entry.size(); i++) {

            hash_toStr(rib_entry[i].peer_hash_id, p_hash_str);

            buf_len += snprintf(buf2, sizeof(buf2), " ('%s','%s',%d),",
                    p_hash_str.c_str(), rib_entry[i].prefix,
                    rib_entry[i].prefix_len);

            // Cat the entry to the query buff
            if (buf_len < 800000 /* size of buf */)
                strcat(buf, buf2);

        }

        // Remove the last OR since we don't need it
        buf[buf_len - 1] = 0;

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to insert the record
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PathAttrs(tbl_path_attr &path_entry) {
    try {
        char    buf[64000];                 // Misc working buffer
        char    buf2[24000];                // Second working buffer
        size_t  buf_len;                    // size of the query buff

        // Setup the initial MySQL query
        buf_len =
                sprintf(buf, "INSERT into %s (%s) values ", TBL_NAME_PATH_ATTRS,
                        "hash_id,peer_hash_id,origin,as_path,next_hop,med,local_pref,isAtomicAgg,aggregator,community_list,ext_community_list,cluster_list,originator_id,origin_as,as_path_count,nexthop_isIPv4,timestamp");

        /*
         * Generate router table hash from the following fields
         *     peer_hash_id, as_path, next_hop, aggregator,
         *     origin, med, local_pref
         *
         */
        MD5 hash;

        // Generate the hash
        hash.update(path_entry.peer_hash_id, HASH_SIZE);
        hash.update((unsigned char *) path_entry.as_path,
                strlen(path_entry.as_path));
        hash.update((unsigned char *) path_entry.next_hop,
                strlen(path_entry.next_hop));
        hash.update((unsigned char *) path_entry.aggregator,
                strlen(path_entry.aggregator));
        hash.update((unsigned char *) path_entry.origin,
                strlen(path_entry.origin));
        hash.update((unsigned char *) &path_entry.med, sizeof(path_entry.med));
        hash.update((unsigned char *) &path_entry.local_pref,
                sizeof(path_entry.local_pref));
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(path_entry.hash_id, hash_raw, 16);
        delete[] hash_raw;

        // Build the query
        string path_hash_str;
        string p_hash_str;
        hash_toStr(path_entry.hash_id, path_hash_str);
        hash_toStr(path_entry.peer_hash_id, p_hash_str);

        buf_len +=
                snprintf(buf2, sizeof(buf2),
                        "('%s','%s','%s','%s','%s', %u,%u,%d,'%s','%s','%s','%s','%s','%" PRIu32 "','%hu','%d',from_unixtime(%u)),",
                        path_hash_str.c_str(), p_hash_str.c_str(),
                        path_entry.origin, path_entry.as_path,
                        path_entry.next_hop, path_entry.med,
                        path_entry.local_pref, path_entry.atomic_agg,
                        path_entry.aggregator, path_entry.community_list,
                        path_entry.ext_community_list, path_entry.cluster_list,
                        path_entry.originator_id, path_entry.origin_as,
                        path_entry.as_path_count, path_entry.nexthop_isIPv4,
                        path_entry.timestamp_secs);

        // Cat the string to our query buffer
        if (buf_len < sizeof(buf))
            strcat(buf, buf2);

        // Remove the last comma since we don't need it
        buf[buf_len - 1] = 0;

        // Add the on duplicate statement
        snprintf(buf2, sizeof(buf2),
                " ON DUPLICATE KEY UPDATE timestamp=values(timestamp)  ");
        strcat(buf, buf2);

        SELF_DEBUG("QUERY=%s\n", buf);

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PeerDownEvent(tbl_peer_down_event &down_event) {
    try {
        char buf[4096]; // Misc working buffer

        // Build the query
        string p_hash_str;
        hash_toStr(down_event.peer_hash_id, p_hash_str);

        // Insert the bgp peer down event
        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s) values ('%s', %d, %d, %d, '%s')",
                TBL_NAME_PEER_DOWN,
                "peer_hash_id,bmp_reason,bgp_err_code,bgp_err_subcode,error_text",
                p_hash_str.c_str(), down_event.bmp_reason,
                down_event.bgp_err_code, down_event.bgp_err_subcode,
                down_event.error_text);

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Update the bgp peer state to be not active
        snprintf(buf, sizeof(buf), "UPDATE %s SET state=0 WHERE hash_id = '%s'",
        TBL_NAME_BGP_PEERS, p_hash_str.c_str());
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_StatReport(tbl_stats_report &stats) {
    try {
        char buf[4096];                 // Misc working buffer

        // Build the query
        string p_hash_str;
        hash_toStr(stats.peer_hash_id, p_hash_str);

        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s%s%s) values ('%s', %u, %u, %u, %u, %u, %u, %u, %" PRIu64 ", %" PRIu64 ")",
                TBL_NAME_STATS_REPORT,
                "peer_hash_id,prefixes_rejected,known_dup_prefixes,known_dup_withdraws,",
                "updates_invalid_by_cluster_list,updates_invalid_by_as_path_loop,updates_invalid_by_originagtor_id,",
                "updates_invalid_by_as_confed_loop,num_routes_adj_rib_in,num_routes_local_rib",

                p_hash_str.c_str(), stats.prefixes_rej,
                stats.known_dup_prefixes, stats.known_dup_withdraws,
                stats.invalid_cluster_list, stats.invalid_as_path_loop,
                stats.invalid_originator_id, stats.invalid_as_confed_loop,
                stats.routes_adj_rib_in, stats.routes_loc_rib);


        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PeerUpEvent(DbInterface::tbl_peer_up_event &up_event) {
    try {
        char buf[16384]; // Misc working buffer

        // Build the query
        string p_hash_str;
        hash_toStr(up_event.peer_hash_id, p_hash_str);

        // Insert the bgp peer up event
        snprintf(buf, sizeof(buf),
                "REPLACE into %s (%s) values ('%s','%s','%s',%" PRIu16 ",%" PRIu16 ",%" PRIu32 ",%" PRIu16 ",%" PRIu16 ",'%s','%s')",
                TBL_NAME_PEER_UP,
                "peer_hash_id,local_ip,local_bgp_id,local_port,local_hold_time,local_asn,remote_port,remote_hold_time,sent_capabilities,recv_capabilities",
                p_hash_str.c_str(), up_event.local_ip, up_event.local_bgp_id, up_event.local_port,up_event.local_hold_time,
                up_event.local_asn,
                up_event.remote_port,up_event.remote_hold_time,up_event.sent_cap, up_event.recv_cap);

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute(buf);

        // Update the bgp peer state to be active
        snprintf(buf, sizeof(buf), "UPDATE %s SET state=1 WHERE hash_id = '%s'",
        TBL_NAME_BGP_PEERS, p_hash_str.c_str());
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

/*
 * Enable/disable debugs
 */
void mysqlBMP::enableDebug() {
    debug = true;
}
void mysqlBMP::disableDebug() {
    debug = false;
}
