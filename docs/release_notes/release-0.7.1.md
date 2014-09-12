Release Notes - Version 0.7.1
=============================
**Fully compliant with bmp draft 07**

> *BMP version 1 (older draft versions) will no longer be supported* **after this release**.  This release still supports older BMP versions, but future releases will not.  JunOS >= 13.1, IOS XE >= 3.12, and IOS XR >= 5.2.2 fully support the latest BMP draft.   It is advised that going forward current router images be used to support the latest BMP version. 

Change Log
----------------

* Added initiation and termination message support (fully compliant with bmp draft 07)
* Added peer_up message support (compliant with bmp draft 07)
* Added v_peers VIEW to display BGP peer information (including open/sent message info)
* Added initial deb package support
* Changed field name in routers to init_data
* Fixed some typos
* Fixed v_routes view router and peer name to display IP address if name is not set
* Fixed other minor issues - see commit log.

