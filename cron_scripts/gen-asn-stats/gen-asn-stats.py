#!/usr/bin/env python
"""
  Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v1.0 which accompanies this distribution,
  and is available at http://www.eclipse.org/legal/epl-v10.html

  .. moduleauthor:: Tim Evens <tievens@cisco.com>
"""
import mysql.connector as mysql
from time import time

#: Interval Timestamp
#:    Multiple queries are used to update the same object for an interval run,
#:    therefore all the updates should have the timestamp of when this script started
INTERVAL_TIMESTAMP = time()

# ----------------------------------------------------------------
# Tables schema
# ----------------------------------------------------------------

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
        "  timestamp timestamp not null default current_timestamp on update current_timestamp,"
        "  PRIMARY KEY (asn,timestamp) "
        "  ) ENGINE=InnoDB DEFAULT CHARSET=latin1 "
        ) % (TBL_GEN_ASN_STATS_NAME)

# ----------------------------------------------------------------
# Queries to get data
# ----------------------------------------------------------------

#: IPv4 or IPv6 unique prefixes originated by AS
#:
#: Below are the query parameters that must be supplied. (e.g. { 'type':'IPv4'})
#:    %(type)s         - Replaced by IPv4 or IPv6
QUERY_ORIGIN_AS_PREFIXES = (
        "select origin_as as asn,count(prefix) as prefixes"
        "  from (select origin_as,prefix,prefix_len,if(prefix regexp '^[0-9a-f]:', 'IPv6', 'IPv4') as Type"
        "        from rib join path_attrs force index (primary) ON (rib.path_attr_hash_id = path_attrs.hash_id)"
        "        where if(prefix  regexp '^[0-9a-f]+:', 'IPv6', 'IPv4') = '%(type)s'"
        "        group by prefix,prefix_len order by null) s"
        "  group by origin_as"
        )

#: Returns a list of all distinct prefixes with the origin trimmed off
#:
#: Below are the query parameters that must be supplied. (e.g. { 'type':'IPv4'})
#:    %(type)s         - Replaced by IPv4 or IPv6
QUERY_ASPATH_DISTINCT_TRIM = (
        "select distinct trim(trim(trailing origin_as from as_path)) as as_path_trim,"
        "          if(prefix regexp '^[0-9a-f]+:', 'IPv6', 'IPv4') as Type,prefix,prefix_len"
        "   from rib straight_join path_attrs p"
        "   ON (rib.path_attr_hash_id = p.hash_id and rib.peer_hash_id = p.peer_hash_id)"
        "   where if(prefix regexp '^[0-9a-f]+:', 'IPv6', 'IPv4') = '%(type)s'"
        )

#: Below are the query parameters that must be supplied. (e.g. { 'type':'IPv4'})
#:    %(type)s         - Replaced by IPv4 or IPv6
QUERY_ASPATH_DISTINCT_TRIM2 = (
        "select distinct trim(substr(trim(trailing origin_as from as_path), LOCATE(' ', as_path, 2))) as as_path_trim,"
        "          if(prefix regexp '^[0-9a-f]:', 'IPv6', 'IPv4') as Type"
        "   from rib straight_join path_attrs p"
        "   ON (rib.path_attr_hash_id = p.hash_id and rib.peer_hash_id = p.peer_hash_id)"
        "   where if(prefix regexp '^[0-9a-f]:', 'IPv6', 'IPv4') = '%(type)s'"
        )

QUERY_ASPATH_DISTINCT_TRIM3 = (
        "select distinct trim(substr(trim(trailing origin_as from as_path), LOCATE(' ', as_path, 2))) as as_path_trim,"
        "          if(prefix regexp '^[0-9a-f]+:', 'IPv6', 'IPv4') as Type,prefix,prefix_len"
        "   from rib straight_join path_attrs p"
        "   ON (rib.path_attr_hash_id = p.hash_id and rib.peer_hash_id = p.peer_hash_id)"
        "   where if(prefix regexp '^[0-9a-f]+:', 'IPv6', 'IPv4') = '%(type)s'"
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

            self.cursor = self.conn.cursor()

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


        return True

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

            self.conn.commit()

            self.last_query_time = time() - startTime

            return True

        except mysql.Error as err:
            print("ERROR: query failed - " + str(err))
            return None


def UpdateOriginPrefixesCounts(db, IPv4=True):
    """ Update the origin prefix counts

        :param IPv4:    True if IPv4, set to False to update the IPv6 counts
    """
    colName = "origin_v4_prefixes"
    type = "IPv4"

    if (IPv4 == False):
        colName = "origin_v6_prefixes"
        type = "IPv6"

    # Run query and store data
    rows = db.query(QUERY_ORIGIN_AS_PREFIXES, {'type': type})
    print "Query for %s took %r seconds" % (colName, db.last_query_time)

    # Process the data and update the gen table
    totalRows = len(rows)

    query = ("INSERT INTO %s "
              "    (asn, %s,isOrigin,timestamp) VALUES ") % (TBL_GEN_ASN_STATS_NAME, colName)

    VALUE_fmt=" ('%(asn)s','%(value)s',1,from_unixtime(%(timestamp)s))"

    for idx,row in enumerate(rows):
        query += VALUE_fmt % { 'asn': str(row[0]), 'value': str(row[1]), 'timestamp': str(INTERVAL_TIMESTAMP)}
        if (idx < totalRows -1):
            query += ','

    query += "   ON DUPLICATE KEY UPDATE %s=values(%s),isOrigin=1,timestamp=values(timestamp)" % (colName, colName)

    print "Running bulk insert/update for %s" % colName

    db.queryNoResults(query)

    print "Insert/Update for %s took %r seconds" % (colName, db.last_query_time)


def UpdateTransitPrefixesCounts(db, IPv4=True):
    """ Update the transit prefix counts

        A Transit AS is any AS right of the right most ASN, excluding private ASN's

        :param IPv4:    True if IPv4, set to False to update the IPv6 counts
    """
    colName = "transit_v4_prefixes"
    type = "IPv4"

    if (IPv4 == False):
        colName = "transit_v6_prefixes"
        type = "IPv6"

    # Run query and store data
    rows = db.query(QUERY_ASPATH_DISTINCT_TRIM, {'type': type})
    print "Query for %s took %r seconds" % (colName, db.last_query_time)

    asnCounts = { }

    # Loop through the results and create a new dictionary of ASN's and a count
    for row in rows:
        as_path = cleanAsPath(str(row[0]))

        for idx,asn in enumerate(as_path.split(' ')):
            # Make sure ASN is not empty and skip the first/peering ASN (left most)
            if (len(asn) and idx > 0):
                try:
                    if (not asn in asnCounts):
                        asnCounts[asn] = 1
                    else:
                        asnCounts[asn] += 1

                except Exception as e:
                    print "problem : %r" % e
                    pass

    # Process the data and update the gen table
    totalAsn = len(asnCounts)

    query = ("INSERT INTO %s "
              "    (asn, %s,isTransit,timestamp) VALUES ") % (TBL_GEN_ASN_STATS_NAME, colName)

    VALUE_fmt=" ('%(asn)s','%(value)s',1,from_unixtime(%(timestamp)s))"

    for idx,asn in enumerate(asnCounts):
        query += VALUE_fmt % { 'asn': asn, 'value': str(asnCounts[asn]), 'timestamp': str(INTERVAL_TIMESTAMP)}
        if (idx < totalAsn -1):
            query += ','

    query += "   ON DUPLICATE KEY UPDATE %s=values(%s),isTransit=1,timestamp=values(timestamp)" % (colName, colName)

    print "Running bulk insert/update for %s" % colName

    db.queryNoResults(query)

    print "Insert/Update for %s took %r seconds" % (colName, db.last_query_time)


def cleanAsPath(as_path):
    """ Cleans up the AS PATH

        Removes AS-SET chars, prepended ASN's, and private/reserved ASN's

        :param as_path:     The AS_PATH in string format

        :return: AS path with the privates removed
    """
    # Remove AS-SET '{' and '}' from path
    as_path = as_path.translate(None, '{}')

    new_as_path = ""
    prev_asn = ""

    for asn in as_path.split(' '):
        if (prev_asn != asn):
            try:
                asn_int = long(asn)

                if (asn_int == 0 or asn_int == 23456 or
                        (asn_int >= 64496 and asn_int <= 65535) or
                        (asn_int >= 65536 and asn_int <= 131071) or
                        asn_int >= 4200000000 ):
                    pass
                else:
                    new_as_path += " " + asn

            except:
                pass

        prev_asn = asn

    return new_as_path.strip()

def main():
    """
    """
    db = dbAcccess()
    db.connectDb("openbmp", "openbmpNow", "db2.openbmp.org", "openBMP")

    # Create the table
    db.createTable(TBL_GEN_ASN_STATS_NAME, TBL_GEN_ASN_STATS_SCHEMA, False)

    UpdateOriginPrefixesCounts(db, IPv4=True)
    UpdateOriginPrefixesCounts(db, IPv4=False)  # IPv6
    UpdateTransitPrefixesCounts(db, IPv4=True)
    UpdateTransitPrefixesCounts(db, IPv4=False) # IPv6

    db.close()


def script_exit(status=0):
    """ Simple wrapper to exit the script cleanly """
    exit(status)

if __name__ == '__main__':
    main()