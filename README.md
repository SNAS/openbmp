Open BGP Monitoring Protocol (OpenBMP) Collector
================================================
![Build Status](http://build-jenkins.openbmp.org/buildStatus/icon?job=openbmp-server-ubuntu-trusty)

OpenBMP is an open source project that implements **draft-ietf-grow-bmp-14**.  BMP protocol version 3 is defined in draft 08, while versions 1 and 2 are defined in the previous revisions of the draft.

JunOS 10.4 implements the older versions of BMP.   Cisco IOS XE 3.12, IOS XR, and JunOS 13.3 implement version 3 (draft 07).


### Daemon
OpenBMP daemon is a BMP receiver for devices/software that implement BMP, such as Cisco and Juniper routers. The collector is a **producer** to Apache Kafka.   Both RAW BMP messages and parsed messages are produced for Kafka consumer consumption.  

Using Logstash with OpenBMP
-------------------
> Logstash is a flexible, open source, data collection, enrichment, and transport pipeline designed to efficiently process a growing list of log, event, and unstructured data sources for distribution into a variety of outputs, including Elasticsearch.

With logstash, you can easily get a variety of [possible outputs](https://www.elastic.co/guide/en/logstash/current/output-plugins.html). Here we provide elasticsearch output configuration with openBMP kafka input.

### Installing Logstash
[Visit download page](https://www.elastic.co/downloads/logstash)

### Configuration
[Config file for OpenBMP](openbmp/docs/LOGSTASH-CONF.md)

### Starting logstash
Go to your logstash installation location, and run

`logstash -f openbmp-logstash.conf`

### Expanding
To use other outputs or add custom data processing, add other plugins to **filter** section and **output** section. Note that **plugins execute in the order they appear**.


### Flat File Consumer
A basic file consumer of OpenBMP parsed and RAW BMP Kafka streams. You can use this file consumer in the following ways:

* Working example to develop your own consumer that works with either parsed or RAW BMP binary messages
* Record BMP feeds (identical as they are sent by the router) so they can be replayed to other BMP parsers/receivers
* Log parsed BMP and BGP messages in plain flat files

See [file-consumer](http://www.openbmp.org/#!docs/FILE_CONSUMER.md) for more details.

### MySQL Consumer
The MySQL consumer implements the OpenBMP Message Bus API parsed messages API to collect and store BMP/BGP data of all collectors, routers, and peers in real-time. The consumer provides the same data storage that OpenBMP collector versions 0.10.x and less implemented.

See [mysql-consumer](http://www.openbmp.org/#!docs/MYSQL_CONSUMER.md) for more details about the MySQL consumer.

### Message Bus (Kafka)
Starting in release 0.11.x Apache Kafka is used as the centralized bus for collector message streams.   The collector no longer forwards direct to MySQL. Instead, database consumers are used to integrate the data into MySQL, Cassandra, MongoDb, Postgres, flat files, etc.  Anyone can now can interact with the BGP parsed and RAW data in a centralized fashion via Kafka or via one of the consumers.   A single BMP feed from one router can be made available to many consumers without the collector having to be aware of that.  


OpenBMP Flow
------------

![OpenBMP High Level Flow](docs/images/openbmp-flow.png "OpenBMP High Level Flow")

* BMP devices (e.g. routers) send BMP messages to a OpenBMP collector/daemon.   One OpenBMP daemon can handle many routers and bgp peers, but in a large network with transit links and full internet routing tables, multiple OpenBMP collectors are recommended.   Simply configure on the BMP device (router) which BMP server that should be used.  

* Apache Kafka enables many applications the ability to tap into the existing BMP feeds from any number of routers.  A single BMP feed via OpenBMP can feed data into hundreds of consumer apps, such as MySQL, Cassandra, Real-time monitors, Flat file, ELK, Apache Spark, etc.

* Open Daylight (ODL) controller plugins can integrate Kafka feed in both parsed and RAW formats into ODL data store to enable ODL APP's/plugins, making the data available via Netconf/RESTconf.

* Admins, Network Engineers, automated programs/scripts, etc. interact via ODL northbound interfaces to run various BMP analytics.

* Admins, Network Engineers, automated programs/scripts, etc. can go direct to Kafka, BMP database, RA APi's, etc.

Supported Features Highlights
-----------------------------
Below is a list of some key features supported today in OpenBMP.  Many more features exist.

Feature | Description
-------: | -----------
draft-ietf-grow-bmp-14 | BMP Version 3 with backwards compatibility with older drafts
Apache Kafka | Producer of parsed and RAW BMP feeds, multiple consumers available
Database | Access to all collected data via standard ODBC/DB drivers (openbmp-mysql-consumer)
File Logging | All parsed messages can be logged to files, including BMP binary streams (openbmp-file-consumer)
IPv4 | IPv4 Unicast routing table information
IPv6 | IPv6 Unicast routing table information
VPNv4 | L3VPN routing information (within VRF)
bgp-ls| draft-ietf-idr-ls-distribution
Extended Communities | Roughly all of them

So much more...

News
----
### Nov-18-2015
**Released version 0.11.0** with Apache Kafka integration.  See release notes at [Release 0.11.0](docs/release_notes/release-0.11.0.md)

Mysql and file consumers are available.

### Aug-11-2015
**Apache Kafka integration available** <br>
The collector now fully supports Apache Kafka by producing both parsed and BMP raw messages.  [openbmp-mysql-consumer](https://github.com/OpenBMP/openbmp-mysql-consumer) and [openbmp-file-consumer](https://github.com/OpenBMP/openbmp-file-consumer) are available for immediate use.  Please report any bugs/issues via github.


### Jul-23-2015
**New release 0.10.0 is available** Starting in **0.11.0** the collector will forward all messages (parsed and raw) to Apache Kafka.  Anyone wishing to interact with the data can do so via simple kafka consumer clients.  MySQL is being moved into a consumer app, so same over functionality with MySQL will be maintained.   In addition to MySQL, a flat file example app will be created so others can see how easy it is to interact with the data.  Other apps can be written by anyone, which includes Cassandra, Postgres, Apache Spark, etc.

New branch [0.10-0-mysql](https://github.com/OpenBMP/openbmp/tree/0.10.0-mysql) has been created for support/bug fixes only.  New features will be in the master branch.

Kafka integration is available today via the development branch [0.11.0-kafka-dev](https://github.com/OpenBMP/openbmp/tree/0.11.0-kafka-dev). This will be merged into the master branch after MySQL consumer app is available.

### Jun-04-2015
> #### UPGRADE YOUR SCHEMA for this release
New release 0.9.0 is available.   See [release-0.9.0](docs/release_notes/release-0.9.0.md) for more details.  

This release includes significant improvements with performance to handle routers with 10 million plus pre-rib prefixes.  Number of peers can be in the hundreds per router.  

### Mar-27-2015
> #### UPGRADE YOUR SCHEMA if using BGP-LS (link-state)

BGP-LS is now supported.   New tables and views have been created for BGP LS data.

### Jan-27-2015
**Release 0.8.0 is available.**   See [release-0.8.0](docs/release_notes/release-0.8.0.md) for more details.

> #### UPGRADE YOUR SCHEMA
> There have been schema changes, so please update your database.  Currently there isn't a migration
> script, so upgrading will require a drop of the current database. Routers will resend all data
> so all current/active info will come back, but the history will be lost.
>
> If you are concerned with the history being lost, please email me with the schema version you are
> using and I can provide you the alter table syntax to migrate the tables without loss.


### Nov-1-2014
Added back BMPv1 support.  BMPv1 is supported best effort since it's missing the INIT, PEER UP, and TERM messages. Most things will work, but some of the DB views might need to be updated.  We'll update those as needed/requested.

### Oct-29-2014
Added DNS PTR lookup for peers and routers.  Fixed minor issues and updated docs.  
Added [DB REST](http://www.openbmp.org/#!docs/DBREST.md)


### Sep-10-2014
**Released version 0.7.1**   See [release-0.7.1](docs/release_notes/release-0.7.1.md) for more details.

> OpenBMP now fully supports draft-ietf-grow-bmp-07

**Upcoming Changes:**

  * Add BGP-LS support - IGP tables/views
  * OpenBMP UI is being revised using ODL


Use-Cases
---------
There are many reasons to use OpenBMP, but to highlight a few common ones:

* **Centralized BMP Collector** - OpenBMP is a producer to Apache Kafka.  You can write your own consumer or use an existing one.  Other products can interact with OpenBMP via Apache Kafka for RAW BMP streams or the parsed messages.   See [Message Bus API Specification](docs/MESSAGE_BUS_API.md) for more details.

* **Real-Time Topology Monitoring** - Can monitor and alert on topology change, policy changes or lack of enforcement, route-leaking, hijacking, etc.

* **BGP/Route Security** - Route leaking, hijacking by origination, by better transit paths, or deviation from baseline

* **Looking Glasses**  - IPv4, IPv6, and VPN4

* **Route Analytics** - Track convergence times, history of prefixes as they change over time, monitor and track BGP policy changes, etc...

* **Traffic Engineering Analytics**  - Adapt dynamically to change and know what is the best shift

* **BGP pre-policy What-Ifs** - Pre-policy routing information provides insight into all path attributes from various points in the network allowing nonintrusive what-if topology views for new policy validations

* **IGP Topology** - BGP-LS (link-state) provides the complete topology of the IGP (OSPF and/or IS-IS).  The IGP topology provides node, link, and prefix level information.  This includes all BGP next-hops.   It is now possible to do a BGP best path selection with IGP metric for **Adj-In-RIB** information.  It is also possible to monitor the IGP itself as it pertains to links, nodes, prefixes, and BGP.

* *many more*

Installation and Configuration
------------------------------
See the [docs/INSTALL.md](docs/INSTALL.md) documentation for detailed information on how to install and configure OpenBMP daemon and UI.

The installation documentation provides step by step instructions for how to install and configure OpenBMP, including the database.  

Instructions are for Ubuntu and CentOS/RHEL.   Other Linux distributions should work, but instructions might vary.


Using Kafka for Collector Integration
-------------------------------------
See the following docs for more details:

* [CONSUMER\_DEVELOPER\_INTEGRATION](docs/CONSUMER_DEVELOPER_INTEGRATION.md) - Details about Kafka and why it was chosen over AMQP.
* [MESSAGE\_BUS\_API](docs/MESSAGE_BUS_API.md) - Detailed API spec for Parsed and rAW BMP Messages via Kafka

In the future, other feeds can be made available.  We are thinking of adding RAW BGP feeds as well (BMP headers stripped leaving only BGP RAW messages).  This may be useful but currently nobody has requested this. If you are interested in other types of feeds, please contact **tim@openbmp.org**.

Interfacing with the Database
-----------------------------
See the [DB_SCHEMA](http://www.openbmp.org/#!docs/DB_SCHEMA.md) documentation for the database schema and how to interact with it.    


Using Open Daylight
-------------------
ODL integration has been open for awhile due to placement of the collector and analytics via ODL.  With the recent change to Kafka, this makes it very clear and easy on how ODL will be able to integrate.  A simple ODL plugin that implements the OpenBMP Kafka parsed message API can expose real-time BMP messages from hundreds of routers/peers via MD-SAL/Datastore.  Making the data available via Netconf/RESTconf.  ODL can also implement the BMP RAW API to interact natively with the BMP messages for parsing in bgpcep.  

See the [ODL](docs/ODL.md) documentation for detailed information on how to use Open Daylight with OpenBMP.  

Building from Source
--------------------
See the [BUILD](docs/BUILD.md) document for details on how to build OpenBMP from source.  Includes how to create DEB and RPM packages.
