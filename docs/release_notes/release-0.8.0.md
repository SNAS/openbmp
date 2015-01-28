Release Notes - Version 0.8.0
=============================
Fully compliant with bmp draft 07 and now backwards compatible with older version of BMP draft to support JunOS < 13.x.  


Change Log
----------------

### New Features/Changes

* Added Extended community support for most extended communities
* Restored backwards compatibility with BMPv1 (support for < JunOS 13.x)
* Added cron scripts to import whois data and to populate asn statistics
* Added cmake RPM packaging
* Added upstart script for DEB packaging
* Added TCP keepalive support - enabled by default now
* Added DNS resolution for peer and router IP addresses
* Added extended communities, isPeerVPN, isPeerIPv4 to both v_routes and v_routes_history
* Changed term reason and code to be empty when router is connected
* Updated database indexes and partitioning for faster queries.
* Changed requirement of boost 0.49 to 0.46 to support Ubuntu 12.04 builds
* Updated deb_package to support install prefix
* Changed database schema to use latin1 consistently to support reduced key sizes
* Changed path_attr_log to support history of all changed updates - this supports better reporting like seen with withdrawn_log


### Defects/Fixes

* Fixed issue where malformed BMP messages result in router entries in the DB
* Fixed issue where openbmpd would core if the log file couldn't be accessed for read/write
* Fixed peer_rd parsing to correctly parse VPN peer RD types
* Fixed issue where extended communities were not being updated in the DB on change
* Fixed issues with router disconnects not being recorded correctly in DB
* Fixed issue with BMP peer down notification type 2 causing thread close
* Fixed minor issues with the various views - this includes better optimization for v_routes_history
* Corrected CMakeList.txt MPReachAttr to support older versions of cmake, was a typo.
* Fixed various file mode permissions in the source tree
* Corrected some code to support C++0x and OSX builds




