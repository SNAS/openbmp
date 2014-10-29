#!/usr/bin/env python
"""
  Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v1.0 which accompanies this distribution,
  and is available at http://www.eclipse.org/legal/epl-v10.html

  .. moduleauthor:: Tim Evens <tievens@cisco.com>
"""
import mysql.connector as mysql
from time import time,sleep
from datetime import datetime
import subprocess

# ----------------------------------------------------------------
# Whois mapping
# ----------------------------------------------------------------
WHOIS_ATTR_MAP = {
        # ARIN
        'ASName': 'as_name',
        'OrgId': 'org_id',
        'OrgName': 'org_name',
        'Address': 'address',
        'City': 'city',
        'StateProv': 'state_prov',
        'PostalCode': 'postal_code',
        'Country': 'country',
        'Comment': 'remarks',

        # RIPE and AFRINIC and APNIC (apnic does not have org values)
        #  use last aut-num
        #   Lists each contact and other items, stop after empty newline after getting address
        #   Address = Second to last is normally state/prov, last is normally the country
        'as-name': 'as_name',
        'descr' : 'remarks',
        'org' : 'org_id',
        'org-name' : 'org_name',
        'address' : 'address',

        # LACNIC (use last aut-num)
        #    stop loading attributes after empty new line after getting owner
        'owner': 'org_name',
        'ownerid': 'org_id',
        'country': 'country'
    }

# ----------------------------------------------------------------
# Tables schema
# ----------------------------------------------------------------

#: 'gen_asn_stats' table schema
TBL_GEN_ASN_STATS_NAME = "gen_whois_asn"
TBL_GEN_ASN_STATS_SCHEMA = (
        "CREATE TABLE IF NOT EXISTS %s ("
        "  asn int unsigned not null,"
        "  as_name varchar(128),"
        "  org_id varchar(64),"
        "  org_name varchar(255),"
        "  remarks text,"
        "  address varchar(255),"
        "  city varchar(64),"
        "  state_prov varchar(32),"
        "  postal_code varchar(32),"
        "  country varchar(24),"
        "  raw_output text,"
        "  timestamp timestamp not null default current_timestamp on update current_timestamp,"
        "  PRIMARY KEY (asn) "
        "  ) ENGINE=InnoDB DEFAULT CHARSET=latin1 "
        ) % (TBL_GEN_ASN_STATS_NAME)

# ----------------------------------------------------------------
# Queries to get data
# ----------------------------------------------------------------

#: Gets a list of all distinct ASN's
QUERY_AS_LIST = (
        "select distinct a.asn"
        "   from gen_asn_stats a left join gen_whois_asn w on (a.asn = w.asn)"
        "   where isnull(as_name)"
        )

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

            rows = []

            while (True):
                result = self.cursor.fetchmany(size=10000)
                if (len(result) > 0):
                    rows += result
                else:
                    break;

            return rows

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


def getASNList(db):
    """ Gets the ASN list from DB

        :param db:    instance of DbAccess class

        :return: Returns a list/array of ASN's
    """
    # Run query and store data
    rows = db.query(QUERY_AS_LIST)
    print "Query for ASN List took %r seconds" % (db.last_query_time)

    print "total rows = %d" % len(rows)

    asnList = []

    # Append only if the ASN is not a private/reserved ASN
    for row in rows:
        try:
            asn_int = long(row[0])

            if (asn_int == 0 or asn_int == 23456 or
                    (asn_int >= 64496 and asn_int <= 65535) or
                    (asn_int >= 65536 and asn_int <= 131071) or
                    asn_int >= 4200000000 ):
                pass
            else:
                asnList.append(row[0])

        except:
            pass

    return asnList


def walkWhois(db, asnList):
    """ Walks through the ASN list

        The walk will pace each query and will add a delay ever 100 queries
        in order to not cause abuse.  The whois starts with arin and
        follows the referral.

        :param db:         DbAccess reference
        :param asnList:    ASN List to

        :return: Returns a list/array of ASN's (rows from return set)
    """
    asnList_size = len(asnList)
    asnList_processed = 0

    # Max number of requests before requiring a delay
    MAX_REQUESTS_PER_INTERVAL = 200

    requests = 0

    for asn in asnList:
        record = {}
        prev_attr = ""
        raw_output = ""

        requests += 1
        asnList_processed += 1

        proc = subprocess.Popen(["whois", "-h", "whois.arin.net", "AS%s" % asn],
                         stdout= subprocess.PIPE, stdin=None)

        getMore = True
        firstLineBreak = False
        for line in proc.stdout:
            # Skip lines with a comment
            if (len(line) == 0 or line[0] == '#' or line[0] == '%'):
                continue

            # add to raw output
            raw_output += line

            line = line.rstrip('\n')
            line = line.strip()

            # Skip empty lines
            if (len(line) == 0):
                firstLineBreak = True
                continue

            # Parse the attributes and build record
            try:
                (attr,value) = line.split(': ', 1)
                attr = attr.strip()
                value = value.strip()
                value = value.replace("'", "")
                value = value.replace("\\", "")

                #print "Attr = %s Value = %s" % (attr,value)

                # Reset the record if a more specific aut-num is found
                if (attr == 'aut-num'):
                    getMore = True
                    firstLineBreak = False
                    record = {}
                    prev_attr = ""
                    raw_output = "%s\n" % line


                # Add attributes to record
                if (getMore == True and attr in WHOIS_ATTR_MAP):
                    if (WHOIS_ATTR_MAP[attr] in record and attr != 'country'):
                        record[WHOIS_ATTR_MAP[attr]] += "\n%s" % (value)
                    else:
                        record[WHOIS_ATTR_MAP[attr]] = value

                    #print "Attr = %s Value = %s" % (attr,value)

                # Stop updating attributes for current aut-num entry if last address line found
                #   This is a bit tricky due to each RIR storing this in different orders
                #print "   %r  %r == %s" % (getMore, prev_attr, attr)
                if (getMore == False or
                            ('country' in record and 'address' in record and prev_attr == 'address' and attr != 'address')
                            or (attr == 'country' and 'address' in record)
                            or (firstLineBreak and 'address' in record and prev_attr == 'address' and attr != 'address')):
                    getMore = False
                    prev_attr = attr
                    continue

                else:
                    prev_attr = attr

            except:
                pass

        proc.communicate()

        # Update record to add country and state if it has an address
        if ('address' in record):
            addr = record['address'].split('\n')
            if (not 'country' in record):
                record['country'] = addr[len(addr)-1]
            if (not 'state_prov' in record):
                record['state_prov'] = addr[len(addr)-2]

        # debug
        #print ("----------------------------------------------------------------------")
        #print "AS%s" % asn
        #for key in record:
        #    print "attr = %s, value = %s" % (key, record[key])

        # add raw output to record
        record['raw_output'] = raw_output.replace("'", "").strip()

        # Check if as_name is missing, if so use org_name
        if (not 'as_name' in record and 'org_id' in record):
            record['as_name'] = record['org_id']

        # Update database with required
        UpdateWhoisDb(db, asn, record)

        # delay between queries
        if (requests >= MAX_REQUESTS_PER_INTERVAL):
            print "%s: Processed %d of %d" % (datetime.utcnow(), asnList_processed, asnList_size)
            sleep(5)
            requests = 0


def UpdateWhoisDb(db, asn, record):
    """ Update the whois info in the DB

        :param db:          DbAccess reference
        :param asn:         ASN to update in the DB
        :param record:      Dictionary of column names and values
                            Key names must match the column names in DB/table

        :return: True if updated, False if error
    """
    total_columns = len(record)

    # get query column list and value list
    columns = ''
    values = ''
    for idx,name in enumerate(record,start=1):
        columns += name
        values += '\'' + record[name] + '\''

        if (idx != total_columns):
            columns += ','
            values += ','

    # Build the query
    query = ("REPLACE INTO %s "
              "    (asn,%s) VALUES ('%s',%s) ") % (TBL_GEN_ASN_STATS_NAME, columns, asn, values)

    #print "QUERY = %s" % query
    db.queryNoResults(query)


def script_exit(status=0):
    """ Simple wrapper to exit the script cleanly """
    exit(status)


def main():
    """
    """
    db = dbAcccess()
    db.connectDb("openbmp", "openbmpNow", "db2.openbmp.org", "openBMP")

    # Create the table
    db.createTable(TBL_GEN_ASN_STATS_NAME, TBL_GEN_ASN_STATS_SCHEMA, False)

    asnList = getASNList(db)
    walkWhois(db,asnList)

    db.close()


if __name__ == '__main__':
    main()