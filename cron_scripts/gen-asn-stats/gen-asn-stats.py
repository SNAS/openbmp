#!/usr/bin/env python
"""
  Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v1.0 which accompanies this distribution,
  and is available at http://www.eclipse.org/legal/epl-v10.html

  .. moduleauthor:: Tim Evens <tievens@cisco.com>
"""
import mysql.connector as mysql
import subprocess
import os
from time import time

#: Interval Timestamp
#:    Multiple queries are used to update the same object for an interval run,
#:    therefore all the updates should have the timestamp of when this script started
INTERVAL_TIMESTAMP = time()

#: Record dictionary
RECORD_DICT = {}

# ----------------------------------------------------------------
# Tables schema
# ----------------------------------------------------------------

#: Insert trigger for gen_asn_stats"
TRIGGER_INSERT_STATUS_NAME = "ins_gen_asn_stats"
TRIGGER_CREATE_INSERT_STATUS_DEF = (
#        "delimiter //\n"
        "CREATE TRIGGER " + TRIGGER_INSERT_STATUS_NAME + " BEFORE INSERT ON gen_asn_stats\n"
        "FOR EACH ROW\n"
        "    BEGIN\n"
        "        declare last_ts timestamp;\n"
        "        declare v4_o_count bigint(20) unsigned;\n"
        "        declare v6_o_count bigint(20) unsigned;\n"
        "        declare v4_t_count bigint(20) unsigned;\n"
        "        declare v6_t_count bigint(20) unsigned;\n"

        "        SELECT transit_v4_prefixes,transit_v6_prefixes,origin_v4_prefixes,\n"
        "                    origin_v6_prefixes,timestamp\n"
        "            INTO v4_t_count,v6_t_count,v4_o_count,v6_o_count,last_ts\n"
        "            FROM gen_asn_stats WHERE asn = new.asn \n"
        "            ORDER BY timestamp DESC limit 1;\n"

        "        IF (new.transit_v4_prefixes = v4_t_count AND new.transit_v6_prefixes = v6_t_count\n"
        "                AND new.origin_v4_prefixes = v4_o_count AND new.origin_v6_prefixes = v6_o_count) THEN\n"

                    # everything is the same, cause the insert to fail (duplicate)
        "            set new.timestamp = last_ts;\n"
        "        END IF;\n"
        "    END;\n"
#        "delimiter ;\n"
)

#: 'gen_asn_stats' table schema
TBL_GEN_ASN_STATS_NAME = "gen_asn_stats"
TBL_GEN_ASN_STATS_SCHEMA = (
        "CREATE TABLE IF NOT EXISTS %s ("
        "  asn int unsigned not null,"
        "  isTransit tinyint not null default 0,"
        "  isOrigin tinyint not null default 0,"
        "  transit_v4_prefixes bigint unsigned not null default 0,"
        "  transit_v6_prefixes bigint unsigned not null default 0,"
        "  origin_v4_prefixes bigint unsigned not null default 0,"
        "  origin_v6_prefixes bigint unsigned not null default 0,"
         " repeats bigint unsigned not null default 0,"
        "  timestamp timestamp not null default current_timestamp on update current_timestamp,"
        "  PRIMARY KEY (asn,timestamp) "
        "  ) ENGINE=InnoDB DEFAULT CHARSET=latin1 "
        ) % (TBL_GEN_ASN_STATS_NAME)

# ----------------------------------------------------------------
# Queries to get data
# ----------------------------------------------------------------

#: returns a list of distinct prefix counts for origin and transit asn's
QUERY_AS_PREFIXES = (
        "SELECT a.asn,rib.isIPv4,if (a.asn_right = 0, 0, 1) as isTransit, count(distinct prefix_bin,prefix_len)"
        "        FROM as_path_analysis a join rib on (a.path_attr_hash_id = rib.path_attr_hash_id)"
        "        WHERE ((a.asn_left != 0 and a.asn_right != 0) or (a.asn_right = 0))"
        "               AND rib.isWithdrawn = False"
        "        GROUP BY a.asn,rib.isIPv4,isTransit"
    )


# ----------------------------------------------------------------
# Insert statements
# ----------------------------------------------------------------


class dbAcccess:
    """ Database access class

        This class handles the database access methods.
    """

    #: Connection handle
    conn = None

    #: Cursor handle
    cursor = None

    #: Last query time in seconds (floating point)
    last_query_time = 0

    def __init__(self):
        pass

    def connectDb(self, user, pw, host, database):
        """
         Connect to database
        """
        try:
            self.conn = mysql.connect(user=user, password=pw,
                               host=host,
                               database=database)


            self.conn.set_autocommit(True)
            self.cursor = self.conn.cursor(buffered=True)


        except mysql.Error as err:
            if err.errno == mysql.errorcode.ER_ACCESS_DENIED_ERROR:
                print("Something is wrong with your user name or password")

            elif err.errno == mysql.errorcode.ER_BAD_DB_ERROR:
                print("Database does not exists")

            else:
                print("ERROR: Connect failed: " + str(err))
                raise err

    def close(self):
        """ Close the database connection """
        if (self.cursor):
            self.cursor.close()
            self.cursor = None

        if (self.conn):
            self.conn.close()
            self.conn = None

    def createTable(self, tableName, tableSchema, dropIfExists = True):
        """ Create table schema

            :param tablename:    The table name that is being created
            :param tableSchema:  Create table syntax as it would be to create it in SQL
            :param dropIfExists: True to drop the table, false to not drop it.

            :return: True if the table successfully was created, false otherwise
        """
        if (not self.cursor):
            print "ERROR: Looks like Mysql is not connected, try to reconnect."
            return False

        try:
            if (dropIfExists == True):
               self.cursor.execute("DROP TABLE IF EXISTS %s" % tableName)

            self.cursor.execute(tableSchema)

        except mysql.Error as err:
            print("ERROR: Failed to create table - " + str(err))
            #raise err
            return False

        return True

    def createTrigger(self, trigger_def, trigger_name, dropIfExists = True):
        """ Create trigger

            :param trigger_def:     Trigger definition
            :param trigger_name:    Trigger name

            :param dropIfExists:    True to drop the table, false to not drop it.

            :return: True if the table successfully was created, false otherwise
        """
        if (not self.cursor):
            print "ERROR: Looks like Mysql is not connected, try to reconnect."
            return False

        try:
            if (dropIfExists == True):
               self.cursor.execute("DROP TRIGGER IF EXISTS %s" % trigger_name)

            self.cursor.execute(trigger_def)

        except mysql.Error as err:
            print("ERROR: Failed to create trigger - " + str(err))
            return False

        return True

    def query(self, query, queryParams=None):
        """ Run a query and return the result set back

            :param query:       The query to run - should be a working SELECT statement
            :param queryParams: Dictionary of parameters to supply to the query for
                                variable substitution

            :return: Returns "None" if error, otherwise array list of rows
        """
        if (not self.cursor):
            print "ERROR: Looks like MySQL is not connected, try to reconnect"
            return None

        try:
            startTime = time()

            if (queryParams):
                self.cursor.execute(query % queryParams)
            else:
                self.cursor.execute(query)

            self.last_query_time = time() - startTime

            #print "Query done now fetching"
            #rows = []
            #while (True):
            #    print "Fetching"
            #    result = self.cursor.fetchmany(size=1000)
            #    if (len(result) > 0):
            #        rows += result
            #    else:
            #        break;

            #return rows
            return self.cursor.fetchall()

        except mysql.Error as err:
            print("ERROR: query failed - " + str(err))
            return None

    def queryNoResults(self, query, queryParams=None):
        """ Runs a query that would normally not have any results, such as insert, update, delete

            :param query:       The query to run - should be a working INSERT or UPDATE statement
            :param queryParams: Dictionary of parameters to supply to the query for
                                variable substitution

            :return: Returns True if successful, false if not.
        """
        if (not self.cursor):
            print "ERROR: Looks like MySQL is not connected, try to reconnect"
            return None

        try:
            startTime = time()

            if (queryParams):
                self.cursor.execute(query % queryParams)
            else:
                self.cursor.execute(query)

            self.last_query_time = time() - startTime

            return True

        except mysql.Error as err:
            print("ERROR: query failed - " + str(err))
            return None


def UpdatePrefixesCounts(db):
    """ Update prefix counts

        :param db:      Pointer to DB access class (db should already be connected and ready)
    """
    colName_origin_v4 = "origin_v4_prefixes"
    colName_origin_v6 = "origin_v6_prefixes"
    colName_transit_v4 = "transit_v4_prefixes"
    colName_transit_v6 = "transit_v6_prefixes"


    # Run query and store data
    rows = db.query(QUERY_AS_PREFIXES)

    print "Query took %r seconds" % (db.last_query_time)

    # Process the data and update the gen table
    totalRows = len(rows)

    for row in rows:
        # Skip private ASN's
        asn_int = int(row[0])
        if (asn_int == 0 or asn_int == 23456 or
            (asn_int >= 64496 and asn_int <= 65535) or
            (asn_int >= 65536 and asn_int <= 131071) or
             asn_int >= 4200000000):
            continue

        if (not row[0] in RECORD_DICT):
            RECORD_DICT[row[0]] = {'transit_v4_prefixes': 0,
                                   'transit_v6_prefixes': 0,
                                   'origin_v4_prefixes': 0,
                                   'origin_v6_prefixes': 0}
        if (row[2] == 0):  # Origin
            if (row[1] == 1):   # IPv4
                RECORD_DICT[row[0]][colName_origin_v4] = int(row[3])
            else: # IPv6
                RECORD_DICT[row[0]][colName_origin_v6] = int(row[3])

        else:  # Transit
            if (row[1] == 1):   # IPv4
                RECORD_DICT[row[0]][colName_transit_v4] = int(row[3])
            else: # IPv6
                RECORD_DICT[row[0]][colName_transit_v6] = int(row[3])


def UpdateDB(db):
    """ Update DB with RECORD_DICT information

        Updates the DB for every record in RECORD_DICT

        :param db:  Pointer to DB access class (db should already be connected and ready)
    """
    query = ("INSERT IGNORE INTO %s "
              "    (asn, isTransit,isOrigin,transit_v4_prefixes,transit_v6_prefixes,"
                    "origin_v4_prefixes,origin_v6_prefixes,timestamp,repeats) "
                  " VALUES ") % (TBL_GEN_ASN_STATS_NAME)

    VALUE_fmt=" ('%s',%d,%d,'%s','%s','%s','%s',from_unixtime(%s),0)"

    isTransit = 0
    isOrigin = 0
    totalRecords = len(RECORD_DICT)

    for idx,asn in enumerate(RECORD_DICT):
        if (RECORD_DICT[asn]['transit_v4_prefixes'] > 0 or RECORD_DICT[asn]['transit_v6_prefixes'] > 0):
            isTransit = 1
        else:
            isTransit = 0

        if (RECORD_DICT[asn]['origin_v4_prefixes'] > 0 or RECORD_DICT[asn]['origin_v6_prefixes'] > 0):
            isOrigin = 1
        else:
            isOrigin = 0

        query += VALUE_fmt % (asn,isTransit,isOrigin, RECORD_DICT[asn]['transit_v4_prefixes'], 
                              RECORD_DICT[asn]['transit_v6_prefixes'],RECORD_DICT[asn]['origin_v4_prefixes'],
                              RECORD_DICT[asn]['origin_v6_prefixes'], str(INTERVAL_TIMESTAMP))
        if (idx <  totalRecords-1):
            query += ','

    query += " ON DUPLICATE KEY UPDATE repeats=repeats+1"

    print "Running bulk insert/update"

    db.queryNoResults(query)

def main():
    """
    """
    cmd = ['bash', '-c', "source /etc/default/openbmpd && set"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    for line in proc.stdout:
       (key, _, value) = line.partition("=")
       os.environ[key] = value.rstrip()

    proc.communicate()

    db = dbAcccess()
    db.connectDb(os.environ["OPENBMP_DB_USER"], os.environ["OPENBMP_DB_PASSWORD"], "localhost", os.environ["OPENBMP_DB_NAME"])

    # Create the table
    db.createTable(TBL_GEN_ASN_STATS_NAME, TBL_GEN_ASN_STATS_SCHEMA, False)

    # Create trigger
    db.createTrigger(TRIGGER_CREATE_INSERT_STATUS_DEF, TRIGGER_INSERT_STATUS_NAME, False)

    UpdatePrefixesCounts(db)

    # RECORD_DICT now has all information, ready to update DB
    UpdateDB(db)

    db.close()


def script_exit(status=0):
    """ Simple wrapper to exit the script cleanly """
    exit(status)

if __name__ == '__main__':
    main()
