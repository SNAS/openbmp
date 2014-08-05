/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include <stdlib.h>
#include <cstring>
#include <iostream>

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

    log = logPtr;

    disableDebug();
    setMaxBlobSize(8192);

    // Make the connection to the server
    mysqlConnect(hostURL, username, password, db);
}

/**
 * Destructor
 */
mysqlBMP::~mysqlBMP() {
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

        /****************
         * The below code adds the efficiency to not update mysql if we already have updated it,
         *    but because we maintain state of the bgp peer, we still need to update it.
         *    Using INSERT .. ON DUPLICATE UPDATE should do the trick to still be efficent.
         *
         // Check if we have already processed this entry
         if (hashCompare(peer_list, p_entry.hash_id)) {
         delete [] hash_raw;             // Delete since we don't need to save this
         return;
         }

         // Else we need to add this to the hash
         peer_list.insert(peer_list.end(), hash_raw);
         *
         ***************************/

        // Build the query
        string p_hash_str;
        string r_hash_str;
        hash_toStr(p_entry.hash_id, p_hash_str);
        hash_toStr(p_entry.router_hash_id, r_hash_str);

        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s) values ('%s','%s','%s',%d, '%s', '%s', %u, %d,from_unixtime(%u)) ON DUPLICATE KEY UPDATE timestamp=from_unixtime(%u),state=1",
                TBL_NAME_BGP_PEERS,
                "hash_id,router_hash_id, peer_rd,isIPv4,peer_addr,peer_bgp_id,peer_as,isL3VPNpeer,timestamp",
                p_hash_str.c_str(), r_hash_str.c_str(), p_entry.peer_rd,
                p_entry.isIPv4, p_entry.peer_addr, p_entry.peer_bgp_id,
                p_entry.peer_as, p_entry.isL3VPN, p_entry.timestamp_secs,
                p_entry.timestamp_secs);

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
        //delete [] hash_raw;

        // Check if we have already processed this entry
        if (hashCompare(router_list, r_entry.hash_id)) {
            delete[] hash_raw; // Delete this since we don't need to save it
            return;
        }

        // Else we need to add this to the hash
        router_list.insert(router_list.end(), hash_raw);

        // Build the query
        string r_hash_str;
        hash_toStr(r_entry.hash_id, r_hash_str);

        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s) values ('%s', '%s','%s')",
                TBL_NAME_ROUTERS, "hash_id,name,ip_address", r_hash_str.c_str(),
                r_entry.name, r_entry.src_addr);

        // Add the on duplicate statement
        strcat(buf, " ON DUPLICATE KEY UPDATE timestamp=current_timestamp ");

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
void mysqlBMP::add_Rib(vector<tbl_rib> &rib_entry) {
    char    *buf = new char[800000];            // Misc working buffer
    char    buf2[4096];                         // Second working buffer
    size_t  buf_len = 0;                        // query buffer length

    try {

        // Build the initial part of the query
        //buf_len = sprintf(buf, "REPLACE into %s (%s) values ", TBL_NAME_RIB,
        //        "hash_id,path_attr_hash_id,peer_hash_id,prefix, prefix_len");
        buf_len = sprintf(buf, "INSERT into %s (%s) values ", TBL_NAME_RIB,
                "hash_id,path_attr_hash_id,peer_hash_id,prefix, prefix_len");

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
                    " ('%s','%s','%s','%s', %d),", rib_hash_str.c_str(),
                    path_hash_str.c_str(), p_hash_str.c_str(),
                    rib_entry[i].prefix, rib_entry[i].prefix_len);

            // Cat the entry to the query buff
            if (buf_len < 800000 /* size of buf */)
                strcat(buf, buf2);
        }

        // Remove the last comma since we don't need it
        buf[buf_len - 1] = 0;

        // Add the on duplicate statement
        snprintf(buf2, sizeof(buf2),
                " ON DUPLICATE KEY UPDATE timestamp=current_timestamp,path_attr_hash_id=values(path_attr_hash_id)");
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
                        "hash_id,peer_hash_id,origin,as_path,next_hop,med,local_pref,isAtomicAgg,aggregator,community_list,ext_community_list,cluster_list,originator_id,origin_as,as_path_count");

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
                        "('%s','%s','%s','%s','%s', %u,%u,%d,'%s','%s','%s','%s','%s','%u','%u'),",
                        path_hash_str.c_str(), p_hash_str.c_str(),
                        path_entry.origin, path_entry.as_path,
                        path_entry.next_hop, path_entry.med,
                        path_entry.local_pref, path_entry.atomic_agg,
                        path_entry.aggregator, path_entry.community_list,
                        path_entry.ext_community_list, path_entry.cluster_list,
                        path_entry.originator_id, path_entry.origin_as,
                        path_entry.as_path_count);

        // Cat the string to our query buffer
        if (buf_len < sizeof(buf))
            strcat(buf, buf2);

        // Remove the last comma since we don't need it
        buf[buf_len - 1] = 0;

        // Add the on duplicate statement
        snprintf(buf2, sizeof(buf2),
                " ON DUPLICATE KEY UPDATE timestamp=current_timestamp  ");
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
        snprintf(buf, sizeof(buf), "UPDATE %s set state=0 WHERE hash_id = '%s'",
        TBL_NAME_BGP_PEERS, p_hash_str.c_str());

        SELF_DEBUG("QUERY=%s", buf);

        // Run the query to add the record
        stmt->execute(buf);

        // Free the query statement
        delete stmt;

    } catch (sql::SQLException &e) {
        LOG_ERR("mysql error: %s, error Code = %d, state = %s",
                e.what(), e.getErrorCode(), e.getSQLState().c_str() );
    }
}

void mysqlBMP::add_StatReport(tbl_stats_report &stats) {
    try {
        char buf[4096];                 // Misc working buffer

        // Build the query
        string p_hash_str;
        hash_toStr(stats.peer_hash_id, p_hash_str);

        snprintf(buf, sizeof(buf),
                "INSERT into %s (%s) values ('%s', %u, %u, %u, %u, %u)",
                TBL_NAME_STATS_REPORT,
                "peer_hash_id,prefixes_rejected,known_dup_prefixes,known_dup_withdraws,updates_invalid_by_cluster_list,updates_invalid_by_as_path_loop",
                p_hash_str.c_str(), stats.prefixes_rej,
                stats.known_dup_prefixes, stats.known_dup_withdraws,
                stats.invalid_cluster_list, stats.invalid_as_path_loop);

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
 * Compares two binary/raw hashs (16 bytes)
 *
 * The compare is done against a list of hashes that were previously cached.
 *
 * \param [in]  list    Vector list of known/cached hashes to check
 * \param [in]  hash    Hash to compare/find in the hash list
 *
 * \returns true if match is found, false if not found.
 */
bool mysqlBMP::hashCompare(vector<unsigned char*> list, unsigned char *hash) {

    for (size_t i = 0; i < list.size(); i++) {
        if (!memcmp(list[i], hash, 16)) // Match, stop now
            return true;
    }

    return false;
}

/**
 * Abstract method Implementation - See DbInterface.hpp for details
 */
// TODO: Implement method
void mysqlBMP::add_PeerUpEvent(DbInterface::tbl_peer_up_event &up_event) {

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
