#!/usr/bin/env python
"""
  Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v1.0 which accompanies this distribution,
  and is available at http://www.eclipse.org/legal/epl-v10.html

  .. moduleauthor:: Tim Evens <tievens@cisco.com>
"""
import sys
import getopt
import dbAccess
from datetime import datetime
from collections import OrderedDict
from time import sleep
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
        'source': 'source',

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
        'ownerid': 'as_name',
        #'ownerid': 'org_id',
        'country': 'country'
    }

# ----------------------------------------------------------------
# Whois source commands
# ----------------------------------------------------------------

WHOIS_SOURCES = OrderedDict()
WHOIS_SOURCES['arin'] =  "whois.arin.net"
WHOIS_SOURCES['ripe'] = "whois.ripe.net"
WHOIS_SOURCES['apnic'] = "whois.apnic.net"
WHOIS_SOURCES['afrinic'] =  "whois.afrinic.net"
WHOIS_SOURCES['lacnic'] = "whois.lacnic.net"
WHOIS_SOURCES['ntt'] =  "rr.ntt.net"

# ----------------------------------------------------------------
# Tables schema
# ----------------------------------------------------------------

#: 'gen_whois_asn' table schema
TBL_GEN_WHOIS_ASN_NAME = "gen_whois_asn"
TBL_GEN_WHOIS_ASN_SCHEMA = (
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
        "  source varchar(64),"
        "  timestamp timestamp not null default current_timestamp on update current_timestamp,"
        "  PRIMARY KEY (asn) "
        "  ) ENGINE=InnoDB DEFAULT CHARSET=latin1 "
        ) % (TBL_GEN_WHOIS_ASN_NAME)

# ----------------------------------------------------------------
# Queries to get data
# ----------------------------------------------------------------

#: Gets a list of all distinct ASN's
QUERY_AS_LIST = (
        "select distinct a.asn"
        "   from gen_asn_stats a left join gen_whois_asn w on (a.asn = w.asn)"
        #"   where isnull(as_name)"
        )

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

def whois(asn, source):
    """ whois the source for the given ASN

    :param asn:
    :param source:
    :return: dict of parsed attribute/values
    """
    record = {}
    raw_output = ""

    WHOIS_CMD = ["whois", "-h", WHOIS_SOURCES[source],"AS%s" % asn]

    proc = subprocess.Popen(WHOIS_CMD,
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

    # add raw output to record
    record['raw_output'] = raw_output.replace("'", "").strip()

    return record


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
        requests += 1
        asnList_processed += 1

        # Try all sources
        for source in WHOIS_SOURCES:
            record = whois(asn, source)
            if ('as_name' in record):
                record['source'] = source
                break

        # Only process the record if whois responded
        if ('as_name' in record):
            # debug
            #print ("----------------------------------------------------------------------")
            #print "AS%s source=%s" % (asn, record['source'])
            #for key in record:
            #    print "attr = %s, value = %s" % (key, record[key])

            # Update record to add country and state if it has an address
            if ('address' in record):
                addr = record['address'].split('\n')
                if (not 'country' in record):
                    record['country'] = addr[len(addr)-1]
                if (not 'state_prov' in record):
                    record['state_prov'] = addr[len(addr)-2]

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
              "    (asn,%s) VALUES ('%s',%s) ") % (TBL_GEN_WHOIS_ASN_NAME, columns, asn, values)

    #print "QUERY = %s" % query
    db.queryNoResults(query)


def script_exit(status=0):
    """ Simple wrapper to exit the script cleanly """
    exit(status)


def parseCmdArgs(argv):
    """ Parse commandline arguments

        Usage is printed and program is terminated if there is an error.

        :param argv:   ARGV as provided by sys.argv.  Arg 0 is the program name

        :returns:  dictionary defined as::
                {
                    user:       <username>,
                    password:   <password>,
                    db_host:    <database host>
                }
    """
    REQUIRED_ARGS = 3
    found_req_args = 0
    cmd_args = { 'user': None,
                 'password': None,
                 'db_host': None }

    if (len(argv) < 3):
        usage(argv[0])
        sys.exit(1)

    try:
        (opts, args) = getopt.getopt(argv[1:], "hu:p:",
                                       ["help", "user", "password"])

        for o, a in opts:
            if o in ("-h", "--help"):
                usage(argv[0])
                sys.exit(0)

            elif o in ("-u", "--user"):
                found_req_args += 1
                cmd_args['user'] = a

            elif o in ("-p", "--password"):
                found_req_args += 1
                cmd_args['password'] = a

            else:
                usage(argv[0])
                sys.exit(1)

        # The last arg should be the command
        if (len(args) <= 0):
            print "ERROR: Missing the database host/IP"
            usage(argv[0])
            sys.exit(1)

        else:
            found_req_args += 1
            cmd_args['db_host'] = args[0]


        # The last arg should be the command
        if (found_req_args < REQUIRED_ARGS):
            print "ERROR: Missing required args, found %d required %d" % (found_req_args, REQUIRED_ARGS)
            usage(argv[0])
            sys.exit(1)

        return cmd_args

    except (getopt.GetoptError, TypeError), err:
        print str(err)  # will print something like "option -a not recognized"
        usage(argv[0])
        sys.exit(2)


def usage(prog):
    """ Usage - Prints the usage for this program.

        :param prog:  Program name
    """
    print ""
    print "Usage: %s [OPTIONS] <database host/ip address>" % prog
    print ""
    print "  -u, --user".ljust(30) + "Database username"
    print "  -p, --password".ljust(30) + "Database password"
    print ""

    print "OPTIONAL OPTIONS:"
    print "  -h, --help".ljust(30) + "Print this help menu"



def main():
    """
    """
    cfg = parseCmdArgs(sys.argv)

    db = dbAccess.dbAcccess()
    db.connectDb(cfg['user'], cfg['password'], cfg['db_host'], "openBMP")

    # Create the table
    db.createTable(TBL_GEN_WHOIS_ASN_NAME, TBL_GEN_WHOIS_ASN_SCHEMA, False)

    asnList = getASNList(db)
    walkWhois(db,asnList)

    db.close()


if __name__ == '__main__':
    main()