Release Notes - Version 0.11.0
==============================
Added Apache Kafka for support of multiple consumers and backends.

> #### IMPORTANT
> This release uses Apache Kafka and requires [mysql consumer](https://github.com/OpenBMP/openbmp-mysql-consumer) for MySQL integration.  Same over functionality is available in the mysql consumer.  


Change Log
----------------

### New Features/Changes

* Completely changed to use Apache Kafka (0.8.2.2)
* Initial Apache Kafka partitioning support (partition by peer)
* Added MySQL Consumer
* Added File Consumer
* Added IPv6 BMP listening/feed support
* Added docker containers for all in one (aio), collector, MySQL, and Kafka
* Added MP_REACH IPv4 support (fixes ODL peering)

### Defects/Fixes

* Fixed issue with Alcatel peering via JunOS with received capabilities being empty
* Fixed various segfaults
* Fixed parsing issue of MT-ID on links and prefixes for BGP-ls
* minor bug fixes



