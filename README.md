Open BGP Monitoring Protocol (OpenBMP) Collector
================================================
![Build Status](http://build-jenkins.openbmp.org/buildStatus/icon?job=openbmp-server-ubuntu-trusty)

OpenBMP is an open source project that implements **draft-ietf-grow-bmp-08**.  BMP protocol version 3 is defined in draft 08, while versions 1 and 2 are defined in the previous revisions of the draft.

JunOS 10.4 implements the older versions of BMP.   Cisco IOS XE 3.12, IOS XR, and JunOS 13.3 implement version 3 (draft 07).


### Daemon
OpenBMP daemon is a BMP receiver for devices that implement BMP, such as Cisco and Juniper routers.  Collected BMP messages are decoded and stored in a SQL database.

### Message Bus (Kafka)
Starting in release 0.11.x Apache Kafka is used as the centralized bus for collector message streams.   The collector no longer forwards direct to MySQL. Instead, database consumers are used to integrate the data into MySQL, Cassandra, Postgres, flat files, etc.  Anyone can now can interact with the BGP parsed and RAW data in a centralized fashion via Kafka or via one of the consumers.   A single BMP feed from one router can be made available to many consumers without the collector having to be aware of that.  

> Please stay tuned for more updates on this topic, including complete API specs for the collector feeds (parsed, raw, ...)


### Database
The SQL/transactional database is designed to be flexible for all types of reporting on the collected data by simply linking tables or by creating views.

The database is tuned to support high transactional rates and storage for millions of prefixes and other BGP information.   OpenBMP statistics track how well the database is performing and will alert if there are any issues.


News
----
### Jul-23-2015
**New release 0.10.0 is available.** Starting in **0.11.0** the collector will forward all messages (parsed and raw) to Apache Kafka.  Anyone wishing to interact with the data can do so via simple kafka consumer clients.  MySQL is being moved into a consumer app, so same over functionality with MySQL will be maintained.   In addition to MySQL, a flat file example app will be created so others can see how easy it is to interact with the data.  Other apps can be written by anyone, which includes Cassandra, Postgres, Apache Spark, etc.

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
Added [DB REST](docs/DBREST.md)


### Sep-10-2014
**Released version 0.7.1**   See [release-0.7.1](docs/release_notes/release-0.7.1.md) for more details.

> OpenBMP now fully supports draft-ietf-grow-bmp-07

**Upcoming Changes:**

  * Add BGP-LS support - IGP tables/views
  * OpenBMP UI is being revised using ODL


OpenBMP Flow
------------

![OpenBMP High Level Flow](docs/images/openbmp-flow.png "OpenBMP High Level Flow")

* BMP devices (e.g. routers) send BMP messages to a OpenBMP collector/daemon.   One OpenBMP daemon can handle many routers and bgp peers, but in a large network with transit links and full internet routing tables, multiple OpenBMP daemons is recommended.   Simply configure on the BMP device (router) which BMP server that should be used.  

* Apache Kafka enables many applications the ability to tap into the existing BMP feeds from any number of routers.  A single BMP feed via OpenBMP can feed data into hundreds of consumer apps, such as MySQL, Cassandra, Real-time monitors, Flat file, ELK, Apache Spark, etc.

* Open Daylight (ODL) controller plugins can integrate Kafka feed in both parsed and RAW formats into ODL data store to enable ODL APP's/plugins, making the data available via Netconf/RESTconf. 

* Admins, Network Engineers, automated programs/scripts, etc. interact via ODL northbound interfaces to run various BMP analytics.

* Admins, Network Engineers, automated programs/scripts, etc. can go direct to Kafka, BMP database, RA APi's, etc.

Supported Features
------------------
Below is a list of features supported today in OpenBMP.  Many more features are on the roadmap, including BGP-LS (draft-ietf-idr-ls-distribution).   See the **roadmap** for more details. 

Feature | Description
-------: | -----------
draft-ietf-grow-bmp-07| BMP Version 3
Database | Access to all collected data via standard ODBC/DB drivers
IPv4 | IPv4 Unicast routing table information
IPv6 | IPv6 Unicast routing table information
VPNv4 | L3VPN routing information
bgp-ls| draft-ietf-idr-ls-distribution
Extended Communities | Roughly all of them
Prefix Log| Tracking of withdraws and updates by prefix, including path attributes
Advanced Reporting| Built-in views for common reports, such as route tables, prefixes as paths, and route table history of changes

Use-Cases
---------
There are many reasons to use OpenBMP, but to highlight a few common ones:

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

* [docs/MSGBUS.md](docs/MSGBUS.md) - Details about Kafka and why it was chosen over AMQP. 
* [docs/MSGBUS-PARSED.md](docs/MSGBUS-PARSED.md) - Detailed API spec for Parsed Messages via Kafka
* [docs/MSGBUS-BMP.md](docs/MSGBUS-BMP.md) - Detailed API spec for RAW BMP feed messages via Kafka

In the future, other feeds can be made available.  We are thinking of adding RAW BGP feeds as well (BMP headers stripped leaving only BGP RAW messages).  This may be useful but currently nobody has requested this. If you are interested in other types of feeds, please contact **tim@openbmp.org**. 

Interfacing with the Database
-----------------------------
See the [docs/DATABASE.md](docs/DATABASE.md) documentation for the database schema and how to interact with it.    


Using Open Daylight
-------------------
ODL integration has been open for awhile due to placement of the collector and analytics via ODL.  With the recent change to Kafka, this makes it very clear and easy on how ODL will be able to integrate.  A simple ODL plugin that implements the OpenBMP Kafka parsed message API can expose real-time BMP messages from hundreds of routers/peers via MD-SAL/Datastore.  Making the data available via Netconf/RESTconf.  ODL can also implement the BMP RAW API to interact natively with the BMP messages for parsing in bgpcep.  

See the [docs/ODL.md](docs/ODL.md) documentation for detailed information on how to use Open Daylight with OpenBMP.  


### OpenBMP Daemon

### BMP UI
A BMP UI exists as part of Cisco Value add.  Demos and docker installs are available for trying it out. 
Please contact **tievens@cisco.com** or **serpil@cisco.com** for more information. 

Building from Source
--------------------
See the [docs/BUILD.md](docs/BUILD.md) document for details on how to build OpenBMP from source.  Includes how to create DEB and RPM packages. 


