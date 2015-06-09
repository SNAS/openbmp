Release Notes - Version 0.9.0
=============================
Significant performance improvements to support routers with millions of pre-rib (10M+) prefixes with hundreds of peers.  Improvements are for large edge peering routers and route-reflectors.

> #### Important
> Update your MySQL configuration to increase the **max\_allowed\_packet** to at least **384M**.  This is to support the large bulk inserts/updates and transactions.
> 
> Update your schema (no alter table syntax currently)


Change Log
----------------

### New Features/Changes

* Schema updates to improve large tables and complex queries
* Introduced post parsed queueing to improve performance and to support situations where the DB may be offline or if tables are locked
* Added BGP-LS (Linkstate IS-IS/OSPF) support
* Added withdrawn view to enable looking at past withdrawn prefixes
* Added support for microsecond tracking on updates and withdraws
* Updated whois import to support DB name and wide characters
* Added support for retries for deadlocks and failed updates/inserts (possible now with the queueing)
* Changed RIB table to use "isWithdrawn" column to mark prefix as withdrawn instead of deleting the prefix.  This enables better performance while also supporting better past reporting of prefixes that have been deleted.
* Added binary IP forms to support range searching with GEO data
* Added 2-octet ASN support

### Defects/Fixes

* Fixed issue with inserts/updates for MySQL strict modes
* Temporarily disabled  hashing peer bgp_id because XR has an issue where it sends 0.0.0.0 on subsequent peer up events.
* Fixed issue with IPv6 next-hop parsing
* Fixed issues with ASN statistics generation
* 

Notes
----------------

### Router Up/Down via conn_count
To properly set conn_count in the DB, openbmpd should be stopped via the init script.   If you use SIGKILL, the db will not get updated by openbmpd when its shutdown.

### 2-Octet ASN Support
Cisco IOS XR/XE will send unmodified UPDATES, which will contain 2-octet ASN's if 4-octet is not negotiated.  [Section-5](https://tools.ietf.org/html/draft-ietf-grow-bmp-07#section-5) specifies that all UPDATES should use 4-octet ASN regardless of sent/received capabilities.  Most of the time 4-octet ASN's are used, so this is normally not an issue.  Legacy hardware/software still exists and therefore 2-octet ASN peering will be used.  This update attempts to auto-detect if 2-octet ASN parsing should be used by using the following logic:
    
Default to 4-octet and attempt to parse the ASN's (path), regardless of the negotiated peering capabilities as per draft-07.   If this fails, then check both sent/received capability to confirm 4-octet ASN support is not available.   If not 4-octet, attempt to parse using 2-octet.
    
AS4_PATH/aggregator is not currently implemented, but will be if this is really needed.
 
