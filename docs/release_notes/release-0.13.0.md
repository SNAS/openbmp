Release Notes - Version 0.13.0
==============================

This release includes Segment Routing, Labeled Unicast, L3VPN, Add-Paths, and EVPN support, among many other features listed below.   

Change Log
----------------

#### New Features

* Added Segment Routing egress peer engineering (EPE) bgp-ls link NLRI support
* Add-Paths support added; field added to unicast_prefix as unsigned 32 bit value
* Label unicast is now supported; field added to unicast_prefix as string list delimited by comma
* L2VPN EVPN decoding and topic added 
* MPLS L3VPN decoding and topic added (vpnv4 and vpnv6)
* Additional segment routing support in bgp-ls
   * Decoding for SR capabilities, SR global block information.
   * SR global block information
   * Decoding for SR Prefix SID
* Added NAT/PAT feature to support routers behind NAT as well as replaying BMP feeds
* Added connection pacing feature to prevent global sync on collector startup
* Add support for Adj-RIB-Out, O-flag and add pre/post policy boolean to unicast_prefix
* Added DNS PTR lookup on OSPF router id
* Added OSPF DR support by including the DR in the IGP_Router_ID
* Added support for default route advertisements in BMP server
* Added Kafka Producer configurations to openbmpd.conf
* Added router init TLV INIT_TYPE_ROUTER_BGP_ID=65531
* Added unreserved bandwidth parsing
* Added bgp-ls node flag decoding
* Added bgp-ls node MT-ID decode; updated message bus API for mt_id to be a list
* Added bgp-ls prefix tag decode
* Added better handling of malformed BMP/BGP messages from router
* Added configuration option to specify the binding address for both IPv4 and IPv6

#### Fixes

* Fixed issue with unicast v4 parsing causing segfault.
* Fixed issue with parsing >10G links with ieee 754 float
* Fixed TE default metric so that it prints correctly
* Fixed two-octet ASN detection when first AS path is empty
* Fixed debug and config file conflicts.  CLI will now override the config file. 
* Fixed issue with peer info not persisting peer up capabilities
