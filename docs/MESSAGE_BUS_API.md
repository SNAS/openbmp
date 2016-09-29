# Message Bus API Specficiation

> #### Current Version 1.4


## Version Diff

### Diff from 1.4 to 1.2

* **ls_node**
    * Added **field 27** - Segment Routing Capabilities TLV 

### Diff from 1.3 to 1.2

* **unicast_prefix**
    * Added **field 30** - Flag indicating if unicast BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
    * Added **field 31** - Flag indicating if unicast BGP prefix is Adj-RIB-In or Adj-RIB-Out

* **ls_node**
    * Added **field 25** - Flag indicating if LS node BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
    * Added **field 26** - Flag indicating if LS node BGP prefix is Adj-RIB-In or Adj-RIB-Out

* **ls_link**
    * Added **field 44** - Flag indicating if LS link BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
    * Added **field 45** - Flag indicating if LS link BGP prefix is Adj-RIB-In or Adj-RIB-Out

* **ls_prefix**
    * Added **field 32** - Flag indicating if LS prefix BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
    * Added **field 33** - Flag indicating if LS prefix BGP prefix is Adj-RIB-In or Adj-RIB-Out


### Diff from 1.2 to 1.1

* **ls_link**
    * Added **field 39** - printed form of the Remote IGP router Id (varies in size depending on protocol)
    * Added **field 40** - Printed form of the Remote router Id.  When EPE, this is the Remote BGP Router ID.
    * Added **field 41** - Local Node descriptor ASN
    * Added **field 42** - Remote Node descriptor ASN
    * Added **field 43** - Peer node SID in the format of [L] <weight> <label/idx/ipv4>. L is only set when L flag is set.

* **router**
    * Added **field 12** - Printed form of the router local BGP ID (IP address)
    

### Diff from 1.0 to 1.1

* **unicast_prefix** 

    * Added **field 28** - Additional Paths ID - non-zero if add paths is enabled/used
    * Added **field 29** - Command delimited list of labels - used for labeled unicast
    * Changed hash_id to include path ID only if non-zero
    * Changed hash_id to include the value 1 if labels are used.


Types of Message Feeds
----------------------
Currently there are two feeds available.

### 1) Parsed Messages

* Parsed messages are BMP and BGP messages parsed in a format that can be consumed by most analytics
* Messages have two parts:
    * Headers
    * Data
* Data is conveyed in a denormalized **TSV** format (see each object data format for TSV syntax details)
* TSV (tab separated values, like CSV) records are in sequence for ordered consumption
* Hash ID's are used to correlate related records between objects

#### Topic Name Structure

**<font style="margin-left:50px" color="blue">openbmp</font>.<font color="darkgreen">parsed</font>.<font color="grey">{object}</font>**


### 2) BMP RAW Messages

* BMP RAW feed is a **binary RAW BMP feed** from the router.  No filtering or alteration is performed
* Each Kafka message is a **complete** BMP message, including all BGP data that goes along with that message

#### Topic Name Structure

**<font style="margin-left:50px" color="blue">openbmp</font>.<font color="darkgreen">bmp_raw</font>**


### Headers
* Message consists of **HEADERS** and **DATA**
* The first double newline **"<font color="blue">\\n\\n</font>"** indicates end of headers and beginning of data
* Header syntax is **VARIABLE**: **VALUE**
* Order of headers does not matter
* Variables are case insensitive
* Only mandatory header is (**V: 1**)

*See message API details for the list of headers that will be included*

Message API: Parsed Data
------------------------

### Headers

Header | Value | Description
--------|-------|-------------
**V**| 1.3 | Schema version, currently 1.3
**C\_HASH\_ID** | hash string | Collector Hash Id
**L** | length | Length of the data in bytes
**R** | count | Number of records in TSV data

### Data
Data is in **TSV** format

* Field delimiter is TAB (**\\t**)
* Fields are **NOT** optionally enclosed - this isn't needed and its more work for the consumer to implement it
* Instead of having to deal with escaping TAB and NEWLINE, values with TAB will be replaced with space and newlines (\\n) will be replaced with a carriage return (**\\r**)
* Records are delimited by unix line feed (**\\n**)
* Action is always the first field.  This defines how to handle the data, such as add or delete
* Timestamps are always from the BMP header if non-zero.  If zero, the timestamp will be from the collector from when the message was received.  Timestamps include microseconds and should be in UTC
* Both reachable and withdraw NLRI maybe within the same message. Order of the records (and sequence number) indicate which comes first


### Object: <font color="blue">collector</font> (openbmp.parsed.collector)
Collector details.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **started** = Collector started<br>**change** = Collector had a router connection change<br>**heartbeat** = Collector periodic heartbeat<br>**stopped** = Collector was stopped/shutdown
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number. This increments for each collector record and restarts on collector restart or number wrap.
3 | Admin Id | String | 64 | Administrative Id (variable length string); can be IP, hostname, etc.
4 | Hash | String | 32 | Hash Id for this entry; Hash of fields [ admin id ]
5 | Routers | String | 4K | List of router IP's connected (delimited by comma if more than one exists)
6 | Router Count | Int | 4 | Number of routers connected
7 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff

* Collector sends messages on collector startup, on router change, and every heartbeat interval (default is 4 hours)
* IP address is not part of the data set because there can be multiple IP addresses (v4/v6 and other interfaces)
* Heartbeat is only sent if no other messages have been sent since last period


### Object: <font color="blue">router</font> (openbmp.parsed.router)
One or more BMP routers.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **first** = first message received by the router, before the INIT message<br>**init** = Initiation message received<br>**term** = Termination message received or connection was closed
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each router record by collector and restarts on collector restart or number wrap.
3 | Name | String | 64 | String name of router (from BMP init message or dns PTR)
4 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ IP address, collector hash ]
5 | IP Address | String | 46 | Printed form of the router source IP address
6 | Description | String | 255 | BMP init message description
7 | Term Code | Int | 4 | BMP termination code
8 | Term Reason | String | 255 | BMP Termination reason text
9 | Init Data | String | 4K | BMP initiation data
10 | Term Data | String | 4K | BMP termination data)
11 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
12 | BGP-ID | String | 46 | Printed form of the router local BGP ID (IP address)

### Object: <font color="blue">peer</font> (openbmp.parsed.peer)
One or more BGP peers.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **first** = first message received by the router, before the INIT message<br>**up** = PEER\_UP message received<br>**down** = PEER\_DOWN message received or connection was closed
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each peer record by collector and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ remote/peer ip, peer RD, router hash ]
4 | Router Hash | String | 32 | Hash Id of router
5 | Name | String | 64 | String name of peer (from BMP peer up message or dns PTR)
6 | Remote BGP-ID | String | 46 | printed form of the BGP ID (IP address) for the peer
7 | Router IP | String | 46 | Router BMP source IP address
8 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
9 | Remote ASN | Int | 4 | ASN of the peer (aka remote ASN)
10 | Remote IP | String | 46 | Printed format of the IP address for the peer
11 | Peer RD | String | 64 | Route distinguisher of peer
12 | \*Remote Port | Int | 2 | Peer TCP port number for remote end
13 | \*Local ASN | Int | 4 | Local ASN of the peer used by the router
14 | \*Local IP | String | 46 | Printed format of the IP address for the peer (local side)
15 | \*Local Port | Int | 4 | Peer TCP port number used on local side
16 | \*Local BGP-ID | String | 46 | printed form of the BGP ID (IP address) for BMP router
17 | \*Info Data | String | 4K | BMP Peer UP message informational data
18 | \*Adv Cap | String | 4K | Advertised capabilities
19 | \*Recv Cap | String | 4K | Received capabilities
20 | \*Remote Holddown | Int | 2 | Received holddown from peer
21 | \*Adv Holddown | Int | 2 | Advertised holddown sent to peer
22 | \*\*BMP Reason | Int | 1 | BMP reason code/value detailing the reason for the peer down event
23 | \*\*BGP Error Code | Int | 1 | BGP notification error code value
24 | \*\*BGP Error Subcode | Int | 1 | BGP notification error sub code value
25 | \*\*Error Text | String | 255 | Short description of the notification error message
26 | isL3VPN | Bool | 1 | Indicates if the peer is an L3VPN peer or not
27 | isPrePolicy | Bool | 1 | Indicates if the peer will convey pre-policy (true) information or post-policy (false)
28 | isIPv4 | Bool | 1 | Indicates if the peer is IPv4 or IPv6


\* *Only available in PEER_UP (action=up), other actions will set these fields to null/empty*

\*\* *Only available in PEER_DOWN (action=down), other actions will set these fields to null/empty*

### Object: <font color="blue">bmp\_stat</font> (openbmp.parsed.bmp\_stat)
One or more bmp stat reports.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New stat report entry
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each record by peer and restarts on collector restart or number wrap.
3 | Router Hash | String | 32 | Hash Id of router
4 | Router IP | String | 46 | Router BMP source IP address
5 | Peer Hash | String | 32 | Hash Id of the peer
6 | Peer IP | String | 46 | Peer remote IP address
7 | Peer ASN | Int | 4 | Peer remote ASN
8 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
9 | Prefixes Rejected | Int | 4 | Prefixes rejected
10 | Known Dup Prefixes | Int | 4 | Known duplicate prefixes
11 | Known Dup Withdraws | Int | 4 | Known duplicate withdraws
12 | Invalid Cluster List | Int | 4 | Updates invalid by cluster list
13 | Invalid As Path | Int | 4 | Updates invalid by AS Path
14 | Invalid Originator Id | Int | 4 | Updates invalid by originator Id
15 | Invalid As Confed | Int | 4 | Updates invalid by AS confed loop
16 | Prefixes Pre Policy | Int | 8 | Prefixes pre-policy (Adj-RIB-In) - All address families
17 | Prefixes Post Policy | Int | 8 | Prefixes post-policy (Adj-RIB-In) - All address families


### Object: <font color="blue">base\_attribute</font> (openbmp.parsed.base\_attribute)
One or more attribute sets (does not include the NLRI's)

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New/Update entry<br>*There is no delete action since attributes are not withdrawn.  Attribute is considered stale/old when no RIB entries contain this hash id paired with peer and router hash id's*
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each attribute record by peer and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ as path, next hop, aggregator, origin, med, local pref, community list, ext community list, peer hash ]
4 | Router Hash | String | 32 | Hash Id of router
5 | Router IP | String | 46 | Router BMP source IP address
6 | Peer Hash | String | 32 | Hash Id of the peer
7 | Peer IP | String | 46 | Peer remote IP address
8 | Peer ASN | Int | 4 | Peer remote ASN
9 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
10 | Origin | String | 32 | Origin of the prefix (igp, egp, incomplete)
11 | AS Path | String | 8K | AS Path string
12 | AS Path Count | Int | 2 | Count of ASN's in the path
13 | Origin AS | Int | 4 | Originating ASN (right most)
14 | Next Hop | String | 46 | Printed form of the next hop IP address
15 | MED | Int | 4 | MED value
16 | Local Pref | Int | 4 | Local preference value
17 | Aggregator | String | 64 | Aggregator in printed form {as} {IP}
18 | Community List | String | 8K | String form of the communities
19 | Ext Community List | String | 8K | String from of the extended communities
20 | Cluster List | String | 1K | String form of the cluster id's
21 | isAtomicAgg | Bool | 1 | Indicates if the aggregate is atomic
22 | isNextHopIPv4 | Bool | 1 | Indicates if the next hop address is IPv4 or not
23 | Originator Id | String | 46 | Originator ID in printed form (IP)

### Object: <font color="blue">unicast\_prefix</font> (openbmp.parsed.unicast\_prefix)
One or more IPv4/IPv6 unicast prefixes.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New/Update entry<br>**del** = Delete entry (withdrawn) - *Attributes are null/empty for withdrawn prefixes*
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each prefix record by peer and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ prefix, prefix length, peer hash, path_id, 1 if has label(s) ]
4 | Router Hash | String | 32 | Hash Id of router
5 | Router IP | String | 46 | Router BMP source IP address
6 | Base Attr Hash | String | 32 | Hash Id of the base attribute set
7 | Peer Hash | String | 32 | Hash Id of the peer
8 | Peer IP | String | 46 | Peer remote IP address
9 | Peer ASN | Int | 4 | Peer remote ASN
10 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
11 | Prefix | String | 46 | Printed form of the Prefix IP address
12 | Length | Int | 1 | Length of the prefix in bits
13 | isIPv4 | Bool | 1 | Indicates if prefix is IPv4 or IPv6
14 | Origin | String | 32 | Origin of the prefix (igp, egp, incomplete)
15 | AS Path | String | 8K | AS Path string
16 | AS Path Count | Int | 2 | Count of ASN's in the path
17 | Origin AS | Int | 4 | Originating ASN (right most)
18 | Next Hop | String | 46 | Printed form of the next hop IP address
19 | MED | Int | 4 | MED value
20 | Local Pref | Int | 4 | Local preference value
21 | Aggregator | String | 64 | Aggregator in printed form {as} {IP}
22 | Community List | String | 8K | String form of the communities
23 | Ext Community List | String | 8K | String from of the extended communities
24 | Cluster List | String | 1K | String form of the cluster id's
25 | isAtomicAgg | Bool | 1 | Indicates if the aggregate is atomic
26 | isNextHopIPv4 | Bool | 1 | Indicates if the next hop address is IPv4 or not
27 | Originator Id | String | 46 | Originator ID in printed form (IP)
28 | Path ID | Int | 4 | Unsigned 32 bit value for the path ID (draft-ietf-idr-add-paths-15).  Zero means add paths is not enabled/used.
29 | Labels | String | 255 | Comma delimited list of 32bit unsigned values that represent the received labels.
30 | isPrePolicy | Bool | 1 | Indicates if unicast BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
31 | isAdjIn | Bool | 1 | Indicates if unicast BGP prefix is Adj-RIB-In or Adj-RIB-Out



### Object: <font color="blue">ls\_node</font> (openbmp.parsed.ls\_node)
One or more link-state nodes.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New/Update entry<br>**del** = Delete entry (withdrawn) - *Attributes are null/empty for withdrawn prefixes*
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each  record by peer and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ igp router id, bgp ls id, asn, ospf area id ]
4 | Base Attr Hash | String | 32 | Hash value of the base attribute set
5 | Router Hash | String | 32 | Hash Id of router
6 | Router IP | String | 46 | Router BMP source IP address
7 | Peer Hash | String | 32 | Hash Id of the peer
8 | Peer IP | String | 46 | Peer remote IP address
9 | Peer ASN | Int | 4 | Peer remote ASN
10 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
11 | IGP Router Id | String | 46 | printed form of the IGP router Id (varies in size depending on protocol)
12 | Router Id | String | 46 | Printed form of the router Id (either null/empty, IPv4 or IPv6)
13 | Routing Id | Int | 8 | Routing universe Id
14 | LS Id | Int | 4 | Link state Id in Hex
15 | Mt Id | String | 256 | Multi-Topology Id list of hex values delimited by comma
16 | Ospf Area Id | String | 16 | Printed form of the OSPF Area Id (IP format)
17 | Isis Area Id | String | 32 | Hex string of the area Id
18 | Protocol | String | 32 | String name of the protocol (Direct, Static, IS-IS\_L1, IS-IS\_L2, OSPFv2, OSPFv3)
19 | Flags | String | 32 | String representation of the flags
20 | AS Path | String | 8K | BGP AS Path string
21 | Local Pref | Int | 4 | BGP Local preference
22 | MED | Int | 4 | BGP MED value
23 | Next Hop | String | 46 | BGP next hop IP address in printed form
24 | Node Name | String | 255 | ISIS hostname
25 | isPrePolicy | Bool | 1 | Indicates if LS node BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
26 | isAdjIn | Bool | 1 | Indicates if LS node BGP prefix is Adj-RIB-In or Adj-RIB-Out
27 | SR-Capabilities TLV | String | 255 | SR-Capabilities TLV in the format of **[R][N][P][E][V][L] [Range Size] [SID/Label Type]** R, N, P, E, V, L are set only when corresponding flags are set. (More about flags: https://tools.ietf.org/html/draft-gredler-idr-bgp-ls-segment-routing-ext-03#section-2.3.7.2)


### Object: <font color="blue">ls\_link</font> (openbmp.parsed.ls\_link)
One or more link-state links.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New/Update entry<br>**del** = Delete entry (withdrawn) - *Attributes are null/empty for withdrawn prefixes*
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each  record by peer and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ interface ip, neighbor ip, link id, local node hash, remote node hash, local link id, remote link id, peer hash ]
4 | Base Attr Hash | String | 32 | Hash value of the base attribute set
5 | Router Hash | String | 32 | Hash Id of router
6 | Router IP | String | 46 | Router BMP source IP address
7 | Peer Hash | String | 32 | Hash Id of the peer
8 | Peer IP | String | 46 | Peer remote IP address
9 | Peer ASN | Int | 4 | Peer remote ASN
10 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
11 | IGP Router Id | String | 46 | printed form of the Local IGP router Id (varies in size depending on protocol)
12 | Router Id | String | 46 | Printed form of the Local router Id. When EPE, this is Local BGP Router ID.
13 | Routing Id | Int | 8 | Routing universe Id
14 | LS Id | Int | 4 | Link state Id in Hex
15 | Ospf Area Id | String | 16 | Printed form of the OSPF Area Id (IP format)
16 | Isis Area Id | String | 32 | Hex string of the area Id
17 | Protocol | String | 32 | String name of the protocol (Direct, Static, IS-IS\_L1, IS-IS\_L2, OSPFv2, OSPFv3)
18 | AS Path | String | 8K | BGP AS Path string
19 | Local Pref | Int | 4 | BGP Local preference
20 | MED | Int | 4 | BGP MED value
21 | Next Hop | String | 46 | BGP next hop IP address in printed form
22 | Mt Id | Int | 4 | Multi-Topology Id in Hex
23 | Local Link Id | Int | 4 | Unsigned 32bit local link id
24 | remote Link Id | Int | 4 | Unsigned 32bit remote link id
25 | Interface IP | String | 46 | Printed form of the local interface IP (ospf)
26 | Neighbor IP | String | 46 | Printed form of the remote interface IP (ospf)
27 | IGP Metric | Int | 4 | Unsigned 32bit IGP metric value
28 | Admin Group | Int | 4 | Admin Group
29 | Max Link BW | Int | 4 | Int value of the max link BW in Kbits
30 | Max Resv BW | Int | 4 | Int value of the max reserved BW in Kbits
31 | Unreserved BW | String | 100 | String value for 8 uint32 values (8 priorities) delimited by comma (each value is in Kbits)
32 | TE Default Metric | Int | 4 | Unsigned 32bit value of the TE metric
33 | Link Protection | String | 64 | Textual representation of the link protection type
34 | MPLS Proto Mask | String | 64 | Textual representation of the protocols enabled (LDP, RSVP)
35 | SRLG | String | 255 | HEX value list SRLG values (delimited by comma if more than one exists)
36 | Link Name | String | 255 | Link name
37 | Remote Node Hash | String | 32 | Remote node hash Id
38 | Local Node Hash | String | 32 | Local node hash Id
39 | Remote IGP Router Id | String | 46 | printed form of the Remote IGP router Id (varies in size depending on protocol)
40 | Remote Router Id | String | 46 | Printed form of the Remote router Id.  When EPE, this is the Remote BGP Router ID.
41 | Local Node ASN | Int | 4 | Local Node descriptor ASN
42 | Remote Node ASN | Int | 4 | Remote Node descriptor ASN
43 | EPE Peer Node SID | String | 128 | Peer node SID in the format of [L] **weight** **label/idx/ipv4**. L is only set when L flag is set.
44 | isPrePolicy | Bool | 1 | Indicates if LS link BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
45 | isAdjIn | Bool | 1 | Indicates if LS link BGP prefix is Adj-RIB-In or Adj-RIB-Out

### Object: <font color="blue">ls\_prefix</font> (openbmp.parsed.ls\_prefix)
One or more link-state prefixes.

\# | Field | Data Type | Size in Bytes | Details
---|-------|-----------|---------------|---------
1 | Action | String | 32 | **add** = New/Update entry<br>**del** = Delete entry (withdrawn) - *Attributes are null/empty for withdrawn prefixes*
2 | Sequence | Int | 8 | 64bit unsigned number indicating the sequence number.  This increments for each  record by peer and restarts on collector restart or number wrap.
3 | Hash | String | 32 | Hash ID for this entry; Hash of fields [ igp router id, bgp ls id, asn, ospf area id ]
4 | Base Attr Hash | String | 32 | Hash value of the base attribute set
5 | Router Hash | String | 32 | Hash Id of router
6 | Router IP | String | 46 | Router BMP source IP address
7 | Peer Hash | String | 32 | Hash Id of the peer
8 | Peer IP | String | 46 | Peer remote IP address
9 | Peer ASN | Int | 4 | Peer remote ASN
10 | Timestamp | String | 26 | In the format of: YYYY-MM-dd HH:MM:SS.ffffff
11 | IGP Router Id | String | 46 | printed form of the IGP router Id (varies in size depending on protocol)
12 | Router Id | String | 46 | Printed form of the router Id (either null/empty, IPv4 or IPv6)
13 | Routing Id | Int | 8 | Routing universe Id
14 | LS Id | Int | 4 | Link state Id in Hex
15 | Ospf Area Id | String | 16 | Printed form of the OSPF Area Id (IP format)
16 | Isis Area Id | String | 32 | Hex string of the area Id
17 | Protocol | String | 32 | String name of the protocol (Direct, Static, IS-IS\_L1, IS-IS\_L2, OSPFv2, OSPFv3)
18 | AS Path | String | 8K | BGP AS Path string
19 | Local Pref | Int | 4 | BGP Local preference
20 | MED | Int | 4 | BGP MED value
21 | Next Hop | String | 46 | BGP next hop IP address in printed form
22 | Local node Hash | String | 32 | Local node hash Id
23 | Mt Id | Int | 4 | Multi-Topology Id in hex
24 | Ospf Route Type | String | 64 | Textual representation of the OSPF route type (Intra, Inter, Ext-1, Ext-2, NSSA-1, NSSA-2)
25 | IGP Flags | String | 32 | String representation of the IGP flags
26 | Route Tag | Int | 4 | Route tag value
27 | External Route Tag | Int | 8 | External route tag value (hex)
28 | Ospf Forwarding Addr | String | 46 | Printed form o f the forwarding IP address
29 | IGP Metric | Int | 4 | Unsigned 32bit igp metric value
30 | Prefix | String | 46 | Printed form of the IP address/prefix
31 | Prefix Length | Int | 1 | Prefix length in bits
32 | isPrePolicy | Bool | 1 | Indicates if LS prefix BGP prefix is Pre-Policy Adj-RIB-In or Post-Policy Adj-RIB-In
33 | isAdjIn | Bool | 1 | Indicates if LS prefix BGP prefix is Adj-RIB-In or Adj-RIB-Out

Message API: BMP RAW Data
------------------------------------

### Headers
Header | Value | Description
--------|-------|-------------
**V**| 1.1 | Schema version, currently 1.1
**C\_HASH\_ID** | hash string | Collector Hash Id
**R\_HASH\_ID** | hash string | Router Hash Id
**L** | length | Length of the data in bytes


### Data
Data is the binary BMP message data.  This includes the BMP headers and the BGP data (if present in the BMP message).

Binary data begins immediately after the headers and the double newline ("**\\n\\n**")

The binary data can be replayed and consumed by any BMP receiver. Data is unaltered and is an identical copy from what was received by the router.  This means the BMP version is relative to the router implementation.  Monitor **openbmp.parsed.router** to get router details, including the **r\_hash\_id**.
