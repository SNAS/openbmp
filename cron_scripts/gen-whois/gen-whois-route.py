#!/usr/bin/env python
"""
  Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v1.0 which accompanies this distribution,
  and is available at http://www.eclipse.org/legal/epl-v10.html

  .. moduleauthor:: Tim Evens <tievens@cisco.com>
"""
import sys
import os
import getopt
import dbAccess
import gzip
from collections import OrderedDict, deque
from ftplib import FTP
from shutil import rmtree

# ----------------------------------------------------------------
# RR Database download sites
# ----------------------------------------------------------------
RR_DB_FTP = OrderedDict()
RR_DB_FTP['nttcom'] = {'site': 'rr1.ntt.net', 'path': '/nttcomRR/', 'filename':'nttcom.db.gz'}
RR_DB_FTP['level3'] = {'site': 'rr.Level3.net', 'path': '/pub/rr/', 'filename':'level3.db.gz'}
RR_DB_FTP['savvis'] = {'site': 'ftp.radb.net', 'path': '/radb/dbase/', 'filename':'savvis.db.gz'}
RR_DB_FTP['radb'] = {'site': 'ftp.radb.net', 'path': '/radb/dbase/', 'filename':'radb.db.gz'}
RR_DB_FTP['arin'] = {'site': 'ftp.arin.net', 'path': '/pub/rr/', 'filename':'arin.db'}
RR_DB_FTP['afrinic'] = {'site': 'ftp.afrinic.net', 'path': '/pub/dbase/', 'filename':'afrinic.db.gz'}
RR_DB_FTP['apnic'] = {'site': 'ftp.apnic.net', 'path': '/pub/apnic/whois/', 'filename':'apnic.db.route.gz'}
RR_DB_FTP['jpirr'] = {'site': 'ftp.radb.net', 'path': '/radb/dbase/', 'filename':'jpirr.db.gz'}
RR_DB_FTP['apnic_v6'] = {'site': 'ftp.apnic.net', 'path': '/pub/apnic/whois/', 'filename':'apnic.db.route6.gz'}
RR_DB_FTP['ripe'] = {'site': 'ftp.ripe.net', 'path': '/ripe/dbase/split/', 'filename':'ripe.db.route.gz'}
RR_DB_FTP['ripe_v6'] = {'site': 'ftp.ripe.net', 'path': '/ripe/dbase/split/', 'filename':'ripe.db.route6.gz'}


# ----------------------------------------------------------------
# Whois mapping
# ----------------------------------------------------------------
WHOIS_ATTR_MAP = {
        # RADB
        'route': 'prefix',
        'descr': 'descr',
        'origin': 'origin',
    }

# ----------------------------------------------------------------
# Tables schema
# ----------------------------------------------------------------

#: 'gen_whois_route' table schema
TBL_GEN_WHOIS_ROUTE_NAME = "gen_whois_route"
TBL_GEN_WHOIS_ROUTE_SCHEMA = (
        "CREATE TABLE IF NOT EXISTS %s ("
        "  prefix varbinary(16) not null,"
        "  prefix_len int unsigned,"
        "  descr text,"
        "  origin varchar(32) not null,"
        "  timestamp timestamp not null default current_timestamp on update current_timestamp,"
        "  PRIMARY KEY (prefix,prefix_len,origin) "
        "  ) ENGINE=InnoDB DEFAULT CHARSET=latin1 "
        ) % (TBL_GEN_WHOIS_ROUTE_NAME)



#: Bulk insert queue
bulk_insert_queue = deque()
MAX_BULK_INSERT_QUEUE_SIZE = 2000

#: Temp directory
TMP_DIR = '/tmp/rr_dbase'

def import_rr_db_file(db, db_filename):
    """ Reads RR DB file and imports into database

    ..see: http://irr.net/docs/list.html for details of RR FTP/DB files

    :param db:          DbAccess reference
    :param db_filename:     Filename of DB file to import
    """
    record = {}
    inf = None

    print "Parsing %s" % db_filename
    if (db_filename.endswith(".gz")):
        inf = gzip.open(db_filename, 'rb')
    else:
        inf = open(db_filename, 'r')

    if (inf != None):
        recordComplete = False
        prev_attr = ""

        for line in inf:
            # Skip lines with a comment
            if (len(line) == 0 or line[0] == '#' or line[0] == '%'):
                continue

            line = line.rstrip('\n')

            # empty line means record is complete
            if (len(line) == 0):
                recordComplete = True

            elif (line[0] == ' '):
                # Line is a continuation of previous attribute
                try:
                    value = line.strip()
                    value = value.replace("'", "")
                    value = value.replace("\\", "")

                    record[WHOIS_ATTR_MAP[prev_attr]] += "\n" +  value

                except:
                    pass
            else:

                # Parse the attributes and build record
                try:
                    (attr,value) = line.split(': ', 1)
                    attr = attr.strip()
                    value = value.strip()
                    value = value.replace("'", "")
                    value = value.replace("\\", "")

                    if (attr == 'origin'):
                        # Strip off characters 'AS'
                        value = int(value[2:])

                    elif (attr == 'route'):
                        # Extract out the prefix_len
                        a = value.split('/')
                        record['prefix_len'] = int(a[1])
                        value = a[0]

                    record[WHOIS_ATTR_MAP[attr]] = value

                    prev_attr = attr

                except:
                    pass

            if (recordComplete):
                recordComplete = False

                if ('prefix' in record):
                    addRouteToDb(db, record)
                    ##print "record: %r" % record

                record = {}

        # Commit any pending items
        addRouteToDb(db, {}, commit=True)

        # Close the file
        inf.close()


def addRouteToDb(db, record, commit=False):
    """ Adds/updates route in DB

    :param db:          DbAccess reference
    :param record:      Dictionary of column names and values
    :param commit:      True to flush/commit the queue and this record, False to queue
                        and perform bulk insert.

    :return: True if updated, False if error
    """
    # Add entry to queue
    if (len(record) > 3):
        try:
            bulk_insert_queue.append("(inet6_aton('%s'),%d,%u,'%s')" % (record['prefix'],
                                                         record['prefix_len'],
                                                         record['origin'],
                                                         record['descr']))
        except:
            pass

    # Insert/commit the queue if commit is True or if reached max queue size
    if ((commit == True or len(bulk_insert_queue) > MAX_BULK_INSERT_QUEUE_SIZE) and
        len(bulk_insert_queue)):
        query = ("REPLACE INTO %s (prefix,prefix_len,origin,descr) VALUES " % TBL_GEN_WHOIS_ROUTE_NAME)

        try:
            while bulk_insert_queue:
                query += "%s," % bulk_insert_queue.popleft();

        except IndexError:
            # No more entries
            pass

        # Remove the last comma if present
        if (query.endswith(',')):
            query = query[:-1]

        #print "QUERY = %s" % query
        #print "----------------------------------------------------------------"
        db.queryNoResults(query)


def downloadDataFile():
    """ Download the RR data files
    """
    if (not os.path.exists(TMP_DIR)):
        os.makedirs(TMP_DIR)

    for source in RR_DB_FTP:
        print "Downloading %s..." % source
        ftp = FTP(RR_DB_FTP[source]['site'])
        ftp.login()
        ftp.cwd(RR_DB_FTP[source]['path'])
        ftp.retrbinary("RETR %s" % RR_DB_FTP[source]['filename'],
                       open("%s/%s" % (TMP_DIR, RR_DB_FTP[source]['filename']), 'wb').write)
        ftp.quit()
        print "      Done downloading %s" % source


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

    # Download the RR data files
    downloadDataFile()

    db = dbAccess.dbAcccess()
    db.connectDb(cfg['user'], cfg['password'], cfg['db_host'], "openBMP")

    # Create the table
    db.createTable(TBL_GEN_WHOIS_ROUTE_NAME, TBL_GEN_WHOIS_ROUTE_SCHEMA, False)

    for source in RR_DB_FTP:
        import_rr_db_file(db, "%s/%s" % (TMP_DIR, RR_DB_FTP[source]['filename']))

    rmtree(TMP_DIR)

    db.close()


if __name__ == '__main__':
    main()