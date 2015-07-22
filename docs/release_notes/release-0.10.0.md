Release Notes - Version 0.10.0
=============================
Added ingress buffering of BMP, which is more efficient than buffering parsed data.  

> #### IMPORTANT
> This is the last release with direct MySQL integration.  From now 
> one integration with MySQL is via the Kafka consumer. 
> This will continue to be supported via branch **0.10.0-mysql**


Change Log
----------------

### New Features/Changes

* Added circular buffer for ingress BMP buffering
* Added new openbmpd option -b and OPENBMP_BUFFER to configure the buffer
* Added parallel query support for gen-asn-stats

### Defects/Fixes

* Fixed issue with withdrawn prefixes not having correct hash ID

