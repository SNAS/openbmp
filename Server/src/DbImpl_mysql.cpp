/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
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
#include <netdb.h>

#include <thread>

#include "DbImpl_mysql.h"
#include "md5.h"
#include "safeQueue.hpp"


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

    // Set the writer queue limit
    sql_writeQueue.setLimit(25000);

    /*
     * Create and start the SQL writer thread
     */
    sql_writer_thread_run = true;
    sql_writer_thread = new std::thread(&mysqlBMP::writerThreadLoop, this);
}

/**
 * Destructor
 */
mysqlBMP::~mysqlBMP() {

    SELF_DEBUG("Destory mysql instance");

    // Stop the writer thread
    sql_writer_thread_run = false;
    sql_writeQueue.push("");                // Make sure the queue is not empty so it stops
    sql_writer_thread->join();

    /*
     * Disconnect the router entries in the DB normally
     */
    char buf[4096]; // Misc working buffer
    for (router_list_iter it = router_list.begin(); it != router_list.end(); it++) {
        try {

            // Build the query
            snprintf(buf, sizeof(buf),
                    "UPDATE %s SET isConnected=0,conn_count=0,term_reason_code=65535,term_reason_text=\"Connection closed.\" where hash_id = '%s'",
                    TBL_NAME_ROUTERS,
                    it->first.c_str());

            // Run the query to add the record
            stmt = con->createStatement();
            stmt->execute(buf);

            // Free the query statement
            stmt->close();
            delete stmt;
            stmt = NULL;

        } catch (sql::SQLException &e) {
            LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                    e.what(), e.getErrorCode(), e.getSQLState().c_str() );
            if (stmt != NULL)
                delete stmt;
        }
    }


    delete sql_writer_thread;

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
 * SQL Writer bulk insert/update
 *
 * \param [in] bulk_queries     Reference to bulk queries map (statements)
 * \param [in/out] query_batch  Batch/multi statement query string (will be appended)
 */
void mysqlBMP::writerBulkQuery(std::map<int,std::string> &bulk_queries, std::string &query_batch) {
    string query;

    // Perform a bulk insert/update
    if (bulk_queries.size()) {
        // Loop through the keys (queries)
        for (map<int,string>::iterator it = bulk_queries.begin();
             it != bulk_queries.end();  it++) {
            int a = (*it).first;
            string b = (*it).second;


            switch (a) {
                case SQL_BULK_ADD_RIB :
                    b.erase(b.size() - 1);                              // Remove last comma
                    query = "INSERT into " +  string(TBL_NAME_RIB) +
                            " (hash_id,path_attr_hash_id,peer_hash_id,prefix,prefix_len,isIPv4,prefix_bin,prefix_bcast_bin,timestamp,origin_as)" +
                            " VALUES " + b +
                            " ON DUPLICATE KEY UPDATE timestamp=values(timestamp),path_attr_hash_id=values(path_attr_hash_id),origin_as=values(origin_as),isWithdrawn=0,db_timestamp=current_timestamp ";
                    break;

                case SQL_BULK_ADD_PATH :
                    b.erase(b.size() - 1);                              // Remove last comma
                    query = "INSERT into " + string(TBL_NAME_PATH_ATTRS) +
                            " (hash_id,peer_hash_id,origin,as_path,next_hop,med,local_pref,isAtomicAgg,aggregator,community_list,ext_community_list," +
                            "cluster_list,originator_id,origin_as,as_path_count,nexthop_isIPv4,timestamp)" +
                            " VALUES " + b +
                            " ON DUPLICATE KEY UPDATE timestamp=values(timestamp)  ";
                    break;

                case SQL_BULK_ADD_PATH_ANALYSIS:
                    b.erase(b.size() - 1);
                    query = "INSERT IGNORE into " + string(TBL_NAME_AS_PATH_ANALYSIS) +
                            " (asn,asn_left,asn_right,path_attr_hash_id, peer_hash_id)" +
                            " VALUES " + b +
                            " ON DUPLICATE KEY UPDATE timestamp=current_timestamp ";

                    break;

                case SQL_BULK_WITHDRAW_UPD :
                    b.erase(b.size() - 1);                              // Remove last comma
                    query = "INSERT IGNORE into " +  string(TBL_NAME_RIB) +
                            " (hash_id,peer_hash_id,prefix,prefix_len,timestamp,path_attr_hash_id,isIPv4,prefix_bin,prefix_bcast_bin,isWithdrawn)" +
                            " VALUES " + b +
                            " ON DUPLICATE KEY UPDATE timestamp=values(timestamp),isWithdrawn=1,db_timestamp=current_timestamp ";

                    break;
            }

            //SELF_DEBUG("BULK QUERY[%d]=%s", a, query.c_str());
            if (query.size() > 0) {
                if (query_batch.size() <= 0) {
                    query_batch.assign(query);
                } else {
                    query_batch.append("; ");
                    query_batch.append(query);
                }
            }
        }

        //if (query_batch.size() > 0)
        //    stmt->executeUpdate(query_batch);

        bulk_queries.clear();
    }
}

/**
 * SQL Writer thread function
 */
void mysqlBMP::writerThreadLoop() {
    string query;
    string query_batch;
    int i;
    int pop_num;
    int retries = 0;
    int last_bulk_key = 0;
    bool logQueueHigh = false;
    map<int,string> bulk_queries;

    SELF_DEBUG("SQL writer thread started");

    while (sql_writer_thread_run) {

        sql_writeQueue.wait();

        pop_num = sql_writeQueue.size() < MYSQL_MAX_BULK_INSERT ? sql_writeQueue.size() : MYSQL_MAX_BULK_INSERT;

        if (not logQueueHigh and sql_writeQueue.size() > 20000) {
            LOG_INFO("rtr=%s: Queue size is above 20,000, this is normal on RIB dump: %d", router_ip.c_str(), sql_writeQueue.size());
            logQueueHigh = true;

        } else if (logQueueHigh and sql_writeQueue.size() < 10000) {
            LOG_INFO("rtr=%s: Queue size is below 10,000, this is normal: %d", router_ip.c_str(), sql_writeQueue.size());
            logQueueHigh = false;
        }

        // Loop to process as many messages we can in one commit transaction
        try {

            // Run the query to add the record
            stmt = con->createStatement();

            for (i = 0; i < pop_num; i++) {

                if (sql_writeQueue.front(query) and query.size() > 0) {

                    // If not tagged as bulk, process one by one
                    if (query[0] != 'B') {
                        SELF_DEBUG("QUERY=%s", query.c_str());

                        // Process the current pending bulk queries since they need to be done
                        // in order of non-bulk statements, namely the withdraws/unreach
                        writerBulkQuery(bulk_queries, query_batch);
                    }
                    else {
                        // Process the current pending bulk queries since the current one requires order to be maintained
                        if (last_bulk_key >= SQL_BULK_WITHDRAW_UPD and query[1] < 8) {
                            writerBulkQuery(bulk_queries, query_batch);
                        }

                        // Add statement to the bulk queries map
                        bulk_queries[query[1]] += query.substr(2) + ",";

                        last_bulk_key = query[1];
                    }

                    sql_writeQueue.pop();
                }
            }

            // Run bulk query
            writerBulkQuery(bulk_queries, query_batch);

            if (query_batch.size() > 0) {
                stmt->executeUpdate(query_batch);
                query_batch.clear();
            }

            // Free the query statement
            stmt->close();
            delete stmt;
            stmt = NULL;

            retries = 0;

            if (debug and sql_writeQueue.size() > 1000)
                SELF_DEBUG("rtr=%s:  popped  %d (queue=%d)", router_ip.c_str(), i, sql_writeQueue.size());

        } catch (sql::SQLException &e) {
            if (e.getErrorCode() == 1213 or e.getErrorCode() == 1205) {     // deadlock or lock exceeded timeout
                // Message is still in queue, next pass will be the retry
                SELF_DEBUG("rtr=%s: Deadlock/lock exceeded: %d", router_ip.c_str(), e.getErrorCode());
                if (retries < 10) {
                    retries++;

                } else {
                    LOG_ERR("rtr=%s: retries failed, skipping: %s, error Code = %d, state = %s",
                            router_ip.c_str(), e.what(), e.getErrorCode(), e.getSQLState().c_str());
                }
            }
            else {
                LOG_ERR("rtr=%s: mysql error: %s, error Code = %d, state = %s",
                        router_ip.c_str(), e.what(), e.getErrorCode(), e.getSQLState().c_str());

                sql_writeQueue.pop();
            }

            SELF_DEBUG("rtr=%s: query=%s", router_ip.c_str(), query.c_str());
            if (stmt != NULL)
                delete stmt;
        }
    }

    SELF_DEBUG("SQL writer thread stopped");
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
        connection_properties["CLIENT_MULTI_STATEMENTS"] = true;
        connection_properties["CLIENT_COMPRESS"] = true;

        //con = driver->connect(hostURL, username,password);
        con = driver->connect(connection_properties);

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLStateCStr());

        if (stmt != NULL)
            delete stmt;
        throw "ERROR: Cannot connect to mysql.";
    }

}


/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::startTransaction() {
    try {
        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute("START TRANSACTION READ WRITE");

        // Free the query statement
        stmt->close();
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );

        if (stmt != NULL)
            delete stmt;
    }
}


/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::commitTransaction() {
    try {
        // Run the query to add the record
        stmt = con->createStatement();
        stmt->execute("COMMIT");

        // Free the query statement
        stmt->close();
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );

        if (stmt != NULL)
            delete stmt;
    }
}


/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_Peer(tbl_bgp_peer &p_entry) {

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

    /* TODO: Uncomment once this is fixed in XR
     * Disable hashing the bgp peer ID since XR has an issue where it sends 0.0.0.0 on subsequent PEER_UP's
     *    This will be fixed in XR, but for now we can disable hashing on it.
     *
    hash.update((unsigned char *) p_entry.peer_bgp_id,
            strlen(p_entry.peer_bgp_id));
    */

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

    // Get the hostname using DNS
    string hostname;
    resolveIp(p_entry.peer_addr, hostname);

    // Insert/Update map entry
    peer_list[p_hash_str] = time(NULL);

    // Build the query
    snprintf(buf, sizeof(buf),
            "REPLACE into %s (%s) VALUES ('%s','%s','%s',%d, '%s', '%s', %u, %d, %d, current_timestamp,1, '%s')",
            TBL_NAME_BGP_PEERS,
            "hash_id,router_hash_id, peer_rd,isIPv4,peer_addr,peer_bgp_id,peer_as,isL3VPNpeer,isPrePolicy,timestamp,state,name",
            p_hash_str.c_str(), r_hash_str.c_str(), p_entry.peer_rd,
            p_entry.isIPv4, p_entry.peer_addr, p_entry.peer_bgp_id,
            p_entry.peer_as, p_entry.isL3VPN, p_entry.isPrePolicy, hostname.c_str());

    SELF_DEBUG("QUERY=%s", buf);

    sql_writeQueue.push(buf);
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
void mysqlBMP::add_Router(tbl_router &r_entry, bool incConnectCount) {

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

    router_ip.assign((char *)r_entry.src_addr);                     // Update router IP for logging

    // Insert/Update map entry
    router_list[r_hash_str] = time(NULL);

    // Convert the init data to string for storage
    string initData(r_entry.initiate_data);
    std::replace(initData.begin(), initData.end(), '\'', '"');

    // Get the hostname
    if (strlen((char *)r_entry.name) <= 0) {
        string hostname;
        resolveIp((char *) r_entry.src_addr, hostname);
        snprintf((char *)r_entry.name, sizeof(r_entry.name), "%s", hostname.c_str());
    }

    // Build the query
    snprintf(buf, sizeof(buf),
            "INSERT into %s (%s) VALUES ('%s', '%s', '%s','%s','%s', 1, 1)",
            TBL_NAME_ROUTERS, "hash_id,name,description,ip_address,init_data,isConnected,conn_count", r_hash_str.c_str(),
            r_entry.name, r_entry.descr, r_entry.src_addr, initData.c_str());

    // Add the on duplicate statement
    if (incConnectCount)
        strcat(buf, " ON DUPLICATE KEY UPDATE timestamp=current_timestamp,isConnected=1,name=values(name),description=values(description),init_data=values(init_data),term_reason_code=0,conn_count=conn_count+1,term_reason_text=''");
    else
        strcat(buf, " ON DUPLICATE KEY UPDATE timestamp=current_timestamp,isConnected=1,name=values(name),description=values(description),init_data=values(init_data),term_reason_code=0,term_reason_text=''");

    sql_writeQueue.push(buf);

    // Update all peers to indicate peers are not up - till proven other wise
    snprintf(buf, sizeof(buf), "UPDATE %s SET state=0 where router_hash_id='%s'",
            TBL_NAME_BGP_PEERS, r_hash_str.c_str());
    sql_writeQueue.push(buf);

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

        add_Router(r_entry, false);

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

        // Convert the term data to string for storage
        string termData (r_entry.term_data);
        std::replace(termData.begin(), termData.end(), '\'', '"');

        char buf[4096]; // Misc working buffer

        // Build the query
        snprintf(buf, sizeof(buf),
                "UPDATE %s SET isConnected=if(conn_count > 1, 1, 0),conn_count=if(conn_count > 1, conn_count - 1, 0),term_reason_code=%" PRIu16 ",term_reason_text=\"%s\",term_data='%s' where hash_id = '%s'",
                TBL_NAME_ROUTERS,
                r_entry.term_reason_code, r_entry.term_reason_text, termData.c_str(),
                r_hash_str.c_str());

        sql_writeQueue.push(buf);
    }

    return false;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_Rib(vector<tbl_rib> &rib_entry) {
    char    *buf = new char[1800000];            // Misc working buffer
    char    buf2[8192];                          // Second working buffer
    size_t  buf_len = 0;                         // query buffer length

    // Build the initial part of the query
    buf_len = sprintf(buf, "B%c", SQL_BULK_ADD_RIB);

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

        //hash.update(rib_entry[i].peer_hash_id, HASH_SIZE);
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

        if (rib_entry[i].isIPv4) {
            // IPv4
            buf_len += snprintf(buf2, sizeof(buf2),
                    " ('%s','%s','%s','%s', %d, 1, X'%02hX%02hX%02hX%02hX', X'%02hX%02hX%02hX%02hX', from_unixtime(%u), %" PRIu32 "),",
                    rib_hash_str.c_str(),
                    path_hash_str.c_str(), p_hash_str.c_str(),
                    rib_entry[i].prefix, rib_entry[i].prefix_len,
                    rib_entry[i].prefix_bin[0], rib_entry[i].prefix_bin[1], rib_entry[i].prefix_bin[2], rib_entry[i].prefix_bin[3],
                    rib_entry[i].prefix_bcast_bin[0], rib_entry[i].prefix_bcast_bin[1],
                    rib_entry[i].prefix_bcast_bin[2], rib_entry[i].prefix_bcast_bin[3],
                    rib_entry[i].timestamp_secs, rib_entry[i].origin_as);
        } else {
            // IPv6
            buf_len += snprintf(buf2, sizeof(buf2),
                    " ('%s','%s','%s','%s', %d, 0, X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX', X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX', from_unixtime(%u),%" PRIu32 "),",
                    rib_hash_str.c_str(),
                    path_hash_str.c_str(), p_hash_str.c_str(),
                    rib_entry[i].prefix, rib_entry[i].prefix_len,
                    rib_entry[i].prefix_bin[0], rib_entry[i].prefix_bin[1], rib_entry[i].prefix_bin[2], rib_entry[i].prefix_bin[3],
                    rib_entry[i].prefix_bin[4], rib_entry[i].prefix_bin[5], rib_entry[i].prefix_bin[6], rib_entry[i].prefix_bin[7],
                    rib_entry[i].prefix_bin[8], rib_entry[i].prefix_bin[9], rib_entry[i].prefix_bin[10], rib_entry[i].prefix_bin[11],
                    rib_entry[i].prefix_bin[12], rib_entry[i].prefix_bin[13], rib_entry[i].prefix_bin[14], rib_entry[i].prefix_bin[15],
                    rib_entry[i].prefix_bcast_bin[0], rib_entry[i].prefix_bcast_bin[1], rib_entry[i].prefix_bcast_bin[2], rib_entry[i].prefix_bcast_bin[3],
                    rib_entry[i].prefix_bcast_bin[4], rib_entry[i].prefix_bcast_bin[5], rib_entry[i].prefix_bcast_bin[6], rib_entry[i].prefix_bcast_bin[7],
                    rib_entry[i].prefix_bcast_bin[8], rib_entry[i].prefix_bcast_bin[9], rib_entry[i].prefix_bcast_bin[10], rib_entry[i].prefix_bcast_bin[11],
                    rib_entry[i].prefix_bcast_bin[12], rib_entry[i].prefix_bcast_bin[13], rib_entry[i].prefix_bcast_bin[14], rib_entry[i].prefix_bcast_bin[15],
                    rib_entry[i].timestamp_secs, rib_entry[i].origin_as);
        }

        // Cat the entry to the query buff
        if (buf_len < 1800000 /* size of buf */)
            strcat(buf, buf2);
    }

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::delete_Rib(vector<tbl_rib> &rib_entry) {
    char    buf2[4096];                         // Second working buffer
    size_t  buf_len = 0;                        // query buffer length

    char    *buf = new char[800000];            // Misc working buffer

    string p_hash_str;
    hash_toStr(rib_entry[0].peer_hash_id, p_hash_str);

    string rib_hash_str;

    /*
     * Update the current RIB entry state
     */
    // Build the initial part of the query
    buf_len = sprintf(buf, "B%c", SQL_BULK_WITHDRAW_UPD);

    // Loop through the vector array of rib entries
    for (size_t i = 0; i < rib_entry.size(); i++) {
        MD5 hash;

        // Generate the hash
        hash.update((unsigned char *) rib_entry[i].prefix,  strlen(rib_entry[i].prefix));
        hash.update(&rib_entry[i].prefix_len, sizeof(rib_entry[i].prefix_len));
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        memcpy(rib_entry[i].hash_id, hash_raw, 16);
        delete[] hash_raw;

        // Build the query
        hash_toStr(rib_entry[i].hash_id, rib_hash_str);

        buf_len +=
                snprintf(buf2, sizeof(buf2),
                        " ('%s','%s','%s',%d,from_unixtime(%u),'',%d,0,0,1),",
                         rib_hash_str.c_str(), p_hash_str.c_str(), rib_entry[i].prefix,
                        rib_entry[i].prefix_len,rib_entry[i].timestamp_secs, rib_entry[i].isIPv4);

        // Cat the entry to the query buff
        if (buf_len < 800000 /* size of buf */)
            strcat(buf, buf2);
    }

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);


    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_AsPathAnalysis(tbl_as_path_analysis &record) {
    char    buf[8192];                  // Misc working buffer
    char    buf2[2048] ;                // Second working buffer
    size_t  buf_len;                    // size of the query buff

    // Setup the initial MySQL query
    buf_len = sprintf(buf, "B%c", SQL_BULK_ADD_PATH_ANALYSIS);

    // Build the query
    string path_hash_str;
    string p_hash_str;
    hash_toStr(record.path_hash_id, path_hash_str);
    hash_toStr(record.peer_hash_id, p_hash_str);

    buf_len +=
            snprintf(buf2, sizeof(buf2),
                     "('%" PRIu32 "','%" PRIu32 "','%" PRIu32 "','%s','%s'),",
                     record.asn, record.asn_left, record.asn_right,
                     path_hash_str.c_str(), p_hash_str.c_str());

    // Cat the string to our query buffer
    if (buf_len < sizeof(buf))
        strcat(buf, buf2);

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PathAttrs(tbl_path_attr &path_entry) {
    char    buf[64000];                 // Misc working buffer
    char    buf2[24000];                // Second working buffer
    size_t  buf_len;                    // size of the query buff

    // Setup the initial MySQL query
    buf_len = sprintf(buf, "B%c", SQL_BULK_ADD_PATH);

    /*
     * Generate router table hash from the following fields
     *     peer_hash_id, as_path, next_hop, aggregator,
     *     origin, med, local_pref
     *
     */
    MD5 hash;

    // Generate the hash
    //hash.update(path_entry.peer_hash_id, HASH_SIZE);
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

    hash.update((unsigned char *) path_entry.community_list, strlen(path_entry.community_list));
    hash.update((unsigned char *) path_entry.ext_community_list, strlen(path_entry.ext_community_list));
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

    sql_writeQueue.push(buf);
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PeerDownEvent(tbl_peer_down_event &down_event) {
    char buf[4096]; // Misc working buffer

    // Build the query
    string p_hash_str;
    hash_toStr(down_event.peer_hash_id, p_hash_str);

    // Insert the bgp peer down event
    snprintf(buf, sizeof(buf),
            "INSERT into %s (%s) VALUES ('%s', %d, %d, %d, '%s')",
            TBL_NAME_PEER_DOWN,
            "peer_hash_id,bmp_reason,bgp_err_code,bgp_err_subcode,error_text",
            p_hash_str.c_str(), down_event.bmp_reason,
            down_event.bgp_err_code, down_event.bgp_err_subcode,
            down_event.error_text);

    sql_writeQueue.push(buf);

    // Update the bgp peer state to be not active
    snprintf(buf, sizeof(buf), "UPDATE %s SET state=0 WHERE hash_id = '%s'",
    TBL_NAME_BGP_PEERS, p_hash_str.c_str());
    sql_writeQueue.push(buf);
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_StatReport(tbl_stats_report &stats) {
    char buf[4096];                 // Misc working buffer

    // Build the query
    string p_hash_str;
    hash_toStr(stats.peer_hash_id, p_hash_str);

    snprintf(buf, sizeof(buf),
             "INSERT into %s (%s%s%s) VALUES ('%s', %u, %u, %u, %u, %u, %u, %u, %" PRIu64 ", %" PRIu64 ")",
             TBL_NAME_STATS_REPORT,
             "peer_hash_id,prefixes_rejected,known_dup_prefixes,known_dup_withdraws,",
             "updates_invalid_by_cluster_list,updates_invalid_by_as_path_loop,updates_invalid_by_originagtor_id,",
             "updates_invalid_by_as_confed_loop,num_routes_adj_rib_in,num_routes_local_rib",

             p_hash_str.c_str(), stats.prefixes_rej,
             stats.known_dup_prefixes, stats.known_dup_withdraws,
             stats.invalid_cluster_list, stats.invalid_as_path_loop,
             stats.invalid_originator_id, stats.invalid_as_confed_loop,
             stats.routes_adj_rib_in, stats.routes_loc_rib);

    sql_writeQueue.push(buf);
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_PeerUpEvent(DbInterface::tbl_peer_up_event &up_event) {
    char buf[16384]; // Misc working buffer

    // Build the query
    string p_hash_str;
    hash_toStr(up_event.peer_hash_id, p_hash_str);

    // Insert the bgp peer up event
    snprintf(buf, sizeof(buf),
            "REPLACE into %s (%s) VALUES ('%s','%s','%s',%" PRIu16 ",%" PRIu16 ",%" PRIu32 ",%" PRIu16 ",%" PRIu16 ",'%s','%s')",
            TBL_NAME_PEER_UP,
            "peer_hash_id,local_ip,local_bgp_id,local_port,local_hold_time,local_asn,remote_port,remote_hold_time,sent_capabilities,recv_capabilities",
            p_hash_str.c_str(), up_event.local_ip, up_event.local_bgp_id, up_event.local_port,up_event.local_hold_time,
            up_event.local_asn,
            up_event.remote_port,up_event.remote_hold_time,up_event.sent_cap, up_event.recv_cap);

    sql_writeQueue.push(buf);

    // Update the bgp peer state to be active
    snprintf(buf, sizeof(buf), "UPDATE %s SET state=1 WHERE hash_id = '%s'",
    TBL_NAME_BGP_PEERS, p_hash_str.c_str());
    sql_writeQueue.push(buf);
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_LsNodes(std::list<DbInterface::tbl_ls_node> &nodes) {
    char    *buf = new char[1800000];            // Misc working buffer
    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length

    // Build the initial part of the query
    buf_len = sprintf(buf, "REPLACE into %s (%s) VALUES ", TBL_NAME_LS_NODE,
                      "hash_id,path_attr_hash_id,peer_hash_id,id,asn,bgp_ls_id,igp_router_id,ospf_area_id,"
                      "protocol,router_id,isIPv4,isis_area_id,flags,name,timestamp");

    string hash_str;
    string path_hash_str;
    string peer_hash_str;

    // Loop through the vector array of entries
    for (std::list<DbInterface::tbl_ls_node>::iterator it = nodes.begin();
            it != nodes.end(); it++) {

        DbInterface::tbl_ls_node &node = (*it);

        // Build the query
        hash_toStr(node.hash_id, hash_str);
        hash_toStr(node.path_atrr_hash_id, path_hash_str);
        hash_toStr(node.peer_hash_id, peer_hash_str);


        buf_len += snprintf(buf2, sizeof(buf2),
                " ('%s','%s','%s',%" PRIu64 ",%u,%u,X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',X'%02hX%02hX%02hX%02hX',"
                        "'%s',X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                        "%d,X'%02hX%02hX%02hX%02hX','%s','%s',from_unixtime(%u)),",
                hash_str.c_str(),path_hash_str.c_str(),peer_hash_str.c_str(), node.id, node.asn, node.bgp_ls_id,
                node.igp_router_id[0], node.igp_router_id[1],node.igp_router_id[2],node.igp_router_id[3],
                node.igp_router_id[4], node.igp_router_id[5],node.igp_router_id[6],node.igp_router_id[7],
                node.ospf_area_Id[0], node.ospf_area_Id[1],node.ospf_area_Id[2], node.ospf_area_Id[3],
                node.protocol,
                node.router_id[0],node.router_id[1],node.router_id[2],node.router_id[3],node.router_id[4],node.router_id[5],
                node.router_id[6],node.router_id[7],node.router_id[8],node.router_id[9],node.router_id[10],node.router_id[11],
                node.router_id[12],node.router_id[13],node.router_id[14],node.router_id[15],
                node.isIPv4, node.isis_area_id[3], node.isis_area_id[2],node.isis_area_id[1],node.isis_area_id[0],
                node.flags, node.name, node.timestamp_secs
        );


        // Cat the entry to the query buff
        if (buf_len < 1800000 /* size of buf */)
            strcat(buf, buf2);
    }

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::del_LsNodes(std::list<DbInterface::tbl_ls_node> &nodes) {
    char    *buf = new char[1800000];            // Misc working buffer

    string  IN_node_hash_list;                   // List of node hashes

    string hash_str;
    string peer_hash_str;

    // build a IN where clause list of node hash ids
    for (std::list<DbInterface::tbl_ls_node>::iterator it = nodes.begin();
         it != nodes.end(); it++) {

        DbInterface::tbl_ls_node &node = (*it);

        // Get the node hash
        hash_toStr(node.hash_id, hash_str);
        hash_toStr(node.peer_hash_id, peer_hash_str);

        if (IN_node_hash_list.size() < 1800000) {
            IN_node_hash_list.append("'").append(hash_str).append("',");
        }
    }

    // Erase/drop the last comma
    IN_node_hash_list.erase(IN_node_hash_list.end()-1);

    // Delete nodes
    snprintf(buf, 1800000,  "DELETE FROM %s WHERE hash_id IN (%s) AND peer_hash_id = '%s'",
             TBL_NAME_LS_NODE, IN_node_hash_list.c_str(), peer_hash_str.c_str());

    //LOG_INFO("QUERY=%s", buf);
    sql_writeQueue.push(buf);

    // Delete associated links
    snprintf(buf, 1800000,  "DELETE FROM %s WHERE local_node_hash_id IN (%s) AND peer_hash_id = '%s'",
             TBL_NAME_LS_LINK, IN_node_hash_list.c_str(), peer_hash_str.c_str());

    //LOG_INFO("QUERY=%s", buf);
    sql_writeQueue.push(buf);

    // Delete associated prefixes
    snprintf(buf, 1800000,  "DELETE FROM %s WHERE local_node_hash_id IN (%s) AND peer_hash_id = '%s'",
             TBL_NAME_LS_PREFIX, IN_node_hash_list.c_str(), peer_hash_str.c_str());

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_LsLinks(std::list<DbInterface::tbl_ls_link> &links) {
    char    *buf = new char[1800000];            // Misc working buffer
    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length

    // Build the initial part of the query
    buf_len = sprintf(buf, "REPLACE into %s (%s) VALUES ", TBL_NAME_LS_LINK,
            "hash_id,path_attr_hash_id,peer_hash_id,id,mt_id,interface_addr,neighbor_addr,isIPv4,"
            "protocol,local_link_id,remote_link_id,local_node_hash_id,remote_node_hash_id,"
            "admin_group,max_link_bw,max_resv_bw,unreserved_bw,te_def_metric,protection_type,"
            "mpls_proto_mask,igp_metric,srlg,name,timestamp"
    );

    string hash_str;
    string path_hash_str;
    string peer_hash_str;
    string local_node_hash_id;
    string remote_node_hash_id;

    // Loop through the vector array of entries
    for (std::list<DbInterface::tbl_ls_link>::iterator it = links.begin();
         it != links.end(); it++) {

        DbInterface::tbl_ls_link &link = (*it);

        MD5 hash;

        hash.update(link.intf_addr, sizeof(link.intf_addr));
        hash.update(link.nei_addr, sizeof(link.nei_addr));
        hash.update((unsigned char *)&link.id, sizeof(link.id));
        hash.update(link.local_node_hash_id, sizeof(link.local_node_hash_id));
        hash.update(link.remote_node_hash_id, sizeof(link.remote_node_hash_id));
        hash.update((unsigned char *)&link.local_link_id, sizeof(link.local_link_id));
        hash.update((unsigned char *)&link.remote_link_id, sizeof(link.remote_link_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(link.hash_id, hash_bin, 16);
        delete[] hash_bin;

        // Build the query
        hash_toStr(link.hash_id, hash_str);
        hash_toStr(link.path_atrr_hash_id, path_hash_str);
        hash_toStr(link.peer_hash_id, peer_hash_str);
        hash_toStr(link.local_node_hash_id, local_node_hash_id);
        hash_toStr(link.remote_node_hash_id, remote_node_hash_id);

        buf_len += snprintf(buf2, sizeof(buf2),
                " ('%s','%s','%s',%" PRIu64 ",%u,X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                "X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',%d,'%s',%u,%u,"
                "'%s','%s',X'%02hX%02hX%02hX%02hX',%lf,%lf,"
                "X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                "%u,'%s','%s',%u,'%s','%s',from_unixtime(%u)),",

                hash_str.c_str(),path_hash_str.c_str(),peer_hash_str.c_str(),link.id, link.mt_id,
                link.intf_addr[0],link.intf_addr[1],link.intf_addr[2],link.intf_addr[3],link.intf_addr[4],link.intf_addr[5],
                link.intf_addr[6],link.intf_addr[7],link.intf_addr[8],link.intf_addr[9],link.intf_addr[10],link.intf_addr[11],
                link.intf_addr[12],link.intf_addr[13],link.intf_addr[14],link.intf_addr[15],
                link.nei_addr[0],link.nei_addr[1],link.nei_addr[2],link.nei_addr[3],link.nei_addr[4],link.nei_addr[5],
                link.nei_addr[6],link.nei_addr[7],link.nei_addr[8],link.nei_addr[9],link.nei_addr[10],link.nei_addr[11],
                link.nei_addr[12],link.nei_addr[13],link.nei_addr[14],link.nei_addr[15],
                link.isIPv4, link.protocol,link.local_link_id,link.remote_link_id,local_node_hash_id.c_str(), remote_node_hash_id.c_str(),
                link.admin_group[0],link.admin_group[1],link.admin_group[2],link.admin_group[3],
                link.max_link_bw,link.max_resv_bw,
                link.unreserved_bw[0],link.unreserved_bw[1],link.unreserved_bw[2],link.unreserved_bw[3],link.unreserved_bw[4],link.unreserved_bw[5],
                link.unreserved_bw[6],link.unreserved_bw[7],link.unreserved_bw[8],link.unreserved_bw[9],link.unreserved_bw[10],link.unreserved_bw[11],
                link.unreserved_bw[12],link.unreserved_bw[13],link.unreserved_bw[14],link.unreserved_bw[15],link.unreserved_bw[16],link.unreserved_bw[17],
                link.unreserved_bw[18],link.unreserved_bw[19],link.unreserved_bw[20],link.unreserved_bw[21],link.unreserved_bw[22],link.unreserved_bw[23],
                link.unreserved_bw[24],link.unreserved_bw[25],link.unreserved_bw[26],link.unreserved_bw[27],link.unreserved_bw[28],link.unreserved_bw[29],
                link.unreserved_bw[30],link.unreserved_bw[31],link.te_def_metric,link.protection_type,
                link.mpls_proto_mask,link.igp_metric,link.srlg,link.name,link.timestamp_secs
        );


        // Cat the entry to the query buff
        if (buf_len < 1800000 /* size of buf */)
            strcat(buf, buf2);
    }

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::del_LsLinks(std::list<DbInterface::tbl_ls_link> &links) {
    char    *buf = new char[1800000];            // Misc working buffer

    string  IN_link_hash_list;                   // List of link hashes

    string hash_str;
    string peer_hash_str;


    // build a IN where clause list of node hash ids
    for (std::list<DbInterface::tbl_ls_link>::iterator it = links.begin();
         it != links.end(); it++) {

        DbInterface::tbl_ls_link &link = (*it);

        MD5 hash;

        hash.update(link.intf_addr, sizeof(link.intf_addr));
        hash.update(link.nei_addr, sizeof(link.nei_addr));
        hash.update((unsigned char *)&link.id, sizeof(link.id));
        hash.update(link.local_node_hash_id, sizeof(link.local_node_hash_id));
        hash.update(link.remote_node_hash_id, sizeof(link.remote_node_hash_id));
        hash.update((unsigned char *)&link.local_link_id, sizeof(link.local_link_id));
        hash.update((unsigned char *)&link.remote_link_id, sizeof(link.remote_link_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(link.hash_id, hash_bin, 16);
        delete[] hash_bin;

        hash_toStr(link.hash_id, hash_str);
        hash_toStr(link.peer_hash_id, peer_hash_str);

        if (IN_link_hash_list.size() < 1800000) {
            IN_link_hash_list.append("'").append(hash_str).append("',");
        }
    }

    // Erase/drop the last comma
    IN_link_hash_list.erase(IN_link_hash_list.end()-1);

    // Delete links
    snprintf(buf, 1800000,  "DELETE FROM %s WHERE hash_id IN (%s) AND peer_hash_id = '%s'",
             TBL_NAME_LS_LINK, IN_link_hash_list.c_str(), peer_hash_str.c_str());

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}


/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::add_LsPrefixes(std::list<DbInterface::tbl_ls_prefix> &prefixes) {
    char    *buf = new char[1800000];            // Misc working buffer
    char    buf2[8192];                          // Second working buffer
    int     buf_len = 0;                         // query buffer length

    // Build the initial part of the query
    buf_len = sprintf(buf, "REPLACE into %s (%s) VALUES ", TBL_NAME_LS_PREFIX,
            "hash_id,path_attr_hash_id,peer_hash_id,id,local_node_hash_id,mt_id,protocol,prefix_len,"
            "prefix_bin,prefix_bcast_bin,ospf_route_type,igp_flags,isIPv4,route_tag,"
            "ext_route_tag,metric,ospf_fwd_addr,timestamp"
    );

    string hash_str;
    string path_hash_str;
    string peer_hash_str;
    string local_node_hash_id;

    // Loop through the vector array of entries
    for (std::list<DbInterface::tbl_ls_prefix>::iterator it = prefixes.begin();
         it != prefixes.end(); it++) {

        DbInterface::tbl_ls_prefix &prefix = (*it);

        MD5 hash;

        hash.update(prefix.prefix_bin, sizeof(prefix.prefix_bin));
        hash.update(&prefix.prefix_len, 1);
        hash.update((unsigned char *)&prefix.id, sizeof(prefix.id));
        hash.update(prefix.local_node_hash_id, sizeof(prefix.local_node_hash_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(prefix.hash_id, hash_bin, 16);
        delete[] hash_bin;

        // Build the query
        hash_toStr(prefix.hash_id, hash_str);
        hash_toStr(prefix.path_atrr_hash_id, path_hash_str);
        hash_toStr(prefix.peer_hash_id, peer_hash_str);
        hash_toStr(prefix.local_node_hash_id, local_node_hash_id);

        buf_len += snprintf(buf2, sizeof(buf2),
                " ('%s','%s','%s',%" PRIu64 ",'%s',%u,'%s',%d,"
                        "X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                        "X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                        "'%s','%s',%d,%u,%" PRIu64 ",%u,"
                        "X'%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX',"
                        "from_unixtime(%u)),",

                hash_str.c_str(),path_hash_str.c_str(),peer_hash_str.c_str(),prefix.id,
                local_node_hash_id.c_str(), prefix.mt_id,prefix.protocol, prefix.prefix_len,
                prefix.prefix_bin[0],prefix.prefix_bin[1],prefix.prefix_bin[2],prefix.prefix_bin[3],prefix.prefix_bin[4],prefix.prefix_bin[5],
                prefix.prefix_bin[6],prefix.prefix_bin[7],prefix.prefix_bin[8],prefix.prefix_bin[9],prefix.prefix_bin[10],prefix.prefix_bin[11],
                prefix.prefix_bin[12],prefix.prefix_bin[13],prefix.prefix_bin[14],prefix.prefix_bin[15],
                prefix.prefix_bcast_bin[0],prefix.prefix_bcast_bin[1],prefix.prefix_bcast_bin[2],prefix.prefix_bcast_bin[3],prefix.prefix_bcast_bin[4],prefix.prefix_bcast_bin[5],
                prefix.prefix_bcast_bin[6],prefix.prefix_bcast_bin[7],prefix.prefix_bcast_bin[8],prefix.prefix_bcast_bin[9],prefix.prefix_bcast_bin[10],prefix.prefix_bcast_bin[11],
                prefix.prefix_bcast_bin[12],prefix.prefix_bcast_bin[13],prefix.prefix_bcast_bin[14],prefix.prefix_bcast_bin[15],
                prefix.ospf_route_type, prefix.igp_flags,prefix.isIPv4,prefix.route_tag, prefix.ext_route_tag,
                prefix.metric,
                prefix.ospf_fwd_addr[0],prefix.ospf_fwd_addr[1],prefix.ospf_fwd_addr[2],prefix.ospf_fwd_addr[3],prefix.ospf_fwd_addr[4],prefix.ospf_fwd_addr[5],
                prefix.ospf_fwd_addr[6],prefix.ospf_fwd_addr[7],prefix.ospf_fwd_addr[8],prefix.ospf_fwd_addr[9],prefix.ospf_fwd_addr[10],prefix.ospf_fwd_addr[11],
                prefix.ospf_fwd_addr[12],prefix.ospf_fwd_addr[13],prefix.ospf_fwd_addr[14],prefix.ospf_fwd_addr[15],
                prefix.timestamp_secs
        );

        // Cat the entry to the query buff
        if (buf_len < 1800000 /* size of buf */)
            strcat(buf, buf2);
    }

    // Remove the last comma since we don't need it
    buf[buf_len - 1] = 0;

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
void mysqlBMP::del_LsPrefixes(std::list<DbInterface::tbl_ls_prefix> &prefixes) {
    char    *buf = new char[1800000];            // Misc working buffer

    string  IN_prefix_hash_list;                 // List of prefix hashes

    string hash_str;
    string peer_hash_str;


    // build a IN where clause list of node hash ids
    for (std::list<DbInterface::tbl_ls_prefix>::iterator it = prefixes.begin();
         it != prefixes.end(); it++) {

        DbInterface::tbl_ls_prefix &prefix = (*it);

        MD5 hash;

        hash.update(prefix.prefix_bin, sizeof(prefix.prefix_bin));
        hash.update(&prefix.prefix_len, 1);
        hash.update((unsigned char *)&prefix.id, sizeof(prefix.id));
        hash.update(prefix.local_node_hash_id, sizeof(prefix.local_node_hash_id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(prefix.hash_id, hash_bin, 16);
        delete[] hash_bin;

        // Build the query
        hash_toStr(prefix.hash_id, hash_str);
        hash_toStr(prefix.peer_hash_id, peer_hash_str);

        if (IN_prefix_hash_list.size() < 1800000) {
            IN_prefix_hash_list.append("'").append(hash_str).append("',");
        }
    }

    // Erase/drop the last comma
    IN_prefix_hash_list.erase(IN_prefix_hash_list.end()-1);

    // Delete links
    snprintf(buf, 1800000,  "DELETE FROM %s WHERE hash_id IN (%s) AND peer_hash_id = '%s'",
             TBL_NAME_LS_PREFIX, IN_prefix_hash_list.c_str(), peer_hash_str.c_str());

    sql_writeQueue.push(buf);

    // Free the large buffer
    delete[] buf;
}

/**
* \brief Method to resolve the IP address to a hostname
*
*  \param [in]   name      String name (ip address)
*  \param [out]  hostname  String reference for hostname
*
*  \returns true if error, false if no error
*/
bool mysqlBMP::resolveIp(string name, string &hostname) {
    addrinfo *ai;
    char host[255];

    if (!getaddrinfo(name.c_str(), NULL, NULL, &ai) and
            !getnameinfo(ai->ai_addr,ai->ai_addrlen, host, sizeof(host), NULL, 0, NI_NAMEREQD)) {

        hostname.assign(host);
        LOG_INFO("resovle: %s to %s", name.c_str(), hostname.c_str());

        freeaddrinfo(ai);
        return false;
    }

    return true;
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
