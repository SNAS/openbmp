Interaction with the Database
=============================

OpenBMP stores the parsed BMP messages in a database. The DB is updated realtime as messages are received.

The design allows for admins, network engineers, scripts/programs, etc. to interact with the Database in a read-only fashion.   A single database instance running with 8G of RAM and 4 vCPU's can handle several routers with several full Internet routing bgp peers. 

Primary Keys
------------
OpenBMP is not just logging BMP/BGP messages, instead it is actively maintaining the information.   Therefore, there is a need for OpenBMP to update existing objects, such as NLRI and timestamps.   To facilitate this, each table includes a **hash_id** which is currently a MD5 hash of various columns.  Each table hash_id is computed on column information instead of requiring multiple column primary keys or unique key constraints.   

To facilitate table linking, **hash_id's** of other tables are referenced.  Each table defines the reference to other table hash_id's.

Schema for Views
----------------
Views are pre-defined queries in a nice table format, making it easier for integration.   Views look just like tables.  You can run normal select queries against them. 

### v_routes
The routes table is similar to looking at the routing table on a router.  The difference is that you can look at them across all routers, including pre-policy. 

Column | DataType | Description
------ | -------- | -----------
RouterName | varchar | DNS/custom name or BMP initiate message (sysName)
PeerName | varchar | Name or IP address of peer
Prefix | varchar | Prefix
PrefixLen | int | Prefix length in bits
NH | varchar | Next-hop IP address in printed format
Origin | varchar| BGP origin in string format
Origin_AS | int | First AS PATh entry, origin of the prefix
MED | int | BGP MED value
LocalPref | int | BGP local preference
AS_Path | varchar | AS Path in string format
ASPath_Count | int | Count of ASN's in the path
Communities | varchar | Standard Communities in string format
ClusterList | varchar | Cluster list in string format
Aggregator | varchar | Aggregator AS and IP address in printed format
PeerAddress | varchar | Peer IP address in printed format
LastModified | timestamp | Timestamp of when last modified/udpated


### v_routes_history
Routes history is just like the routes table, but this will return prefixes with their history of path attributes.  You can use this view to show the prefix history over time.   For example, show a prefix as it converges or how it has moved from one transit to another in a given time period. 

Column | DataType | Description
------ | -------- | -----------
RouterName | varchar | DNS/custom name or BMP initiate message (sysName)
PeerName | varchar | Name or IP address of peer
Prefix | varchar | Prefix
PrefixLen | int | Prefix length in bits
NH | varchar | Next-hop IP address in printed format
Origin | varchar| BGP origin in string format
Origin_AS | int | First AS PATh entry, origin of the prefix
MED | int | BGP MED value
LocalPref | int | BGP local preference
AS_Path | varchar | AS Path in string format
ASPath_Count | int | Count of ASN's in the path
Communities | varchar | Standard Communities in string format
ClusterList | varchar | Cluster list in string format
Aggregator | varchar | Aggregator AS and IP address in printed format
PeerAddress | varchar | Peer IP address in printed format
LastModified | timestamp | Timestamp of when last modified/udpated

### v_peers
View of BGP peers with session information and details of last down and up notifications. This is similar to looking at the bgp peer on the router.  Like with routes, you can see a much larger list of peers by looking at all routers, not just one. 

For example, one could use this view to report on public peers in different peering locations.  Another report could be to track peering stability. 

Column | DataType | Description
------ | -------- | -----------
RouterName | varchar | DNS/custom name or BMP initiate message (sysName)
LocalIP | varchar | IP Address of the BMP router local peering address
LocalPort | int | Local port number of the BMP router local peering session
LocalASN | int | Local ASN for peering sessions
PeerName | varchar | Name or IP address of peer
PeerAddress | varchar | Peer IP address in printed format
PeerPort | int | Remote port number of the peer
PeerASN | int | Peer ASN for session
holdTime | int | BGP holdtime being used, if up
isUp | boolean | True if peer is up, or false if peer is down
isBMPConnected | boolean | True if the router BMP session is connected, false if not
SentParams | varchar(variable) | String list of open params sent to peer
RecvParams | varchar(variable) | String list of open params received from peer
LastDownCode | int | BGP error code of the last down notification
LastDownSubCode | int | BGP error subcode of the last down notification
LastDownMessage | varchar | Meaning of last peer down notification
LastDownTimestamp | timestamp | Timestamp of the last time peer down was sent
LastUpTimestamp | timestamp | Timestamp of the last time peer up was sent


Schema for Stored Procedures
----------------------------
Currently there are no stored procedures.

Schema for Data Tables
----------------------

### Routers
This table is a list of all BMP devices.  Normally this table is populated based on connections made to the OpenBMP server.  This table is used for provisioning allowed BMP devices OpenBMP will accept, as well as to indicate which BMP devices are in *passive* mode requiring *active* tcp connections to be made by OpenBMP. 

Column | DataType | Description
------ | -------- | -----------
hash_id | char(32) | Hash ID for this table
name | varchar(255) | DNS/custom name or if empty the BMP initiate message (sysName)
descr | varchar(255) | Description of router/BMP device (if empty will be sysDescr learned)
ip_address | varchar(40) | IPv4/IPv6 address of the BMP device
asn | unsigned int 32bit | ASN of the BMP device
isConnected | boolean | BMP connection state ; true is established
isPassive | boolean | Indicates if OpenBMP is passive or active
term_reason_code | int | BMP termination reason code  for last termination (isConnected=false)
term_reason_text | varchar(255) | Text description of the reason code meaning
term_data | blob/text | Attribute value pairs provided in termination data 
initiate_data| blob/text | Attribute value paris provided in initiation message
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH


### bgp_peers
BGP peers are added to this table as BMP devices send information.  The **router_hash_id** provides a way to link which BMP device the peer belongs to. 

Column | DataType | Description
------ | -------- | -----------
hash_id | char(32) | Hash ID of this table
router_hash_id | char(32) | Hash ID of the routers table
name | varchar(255) | BGP peer name
peer_rd | varchar(32) | Route distinguisher ID in printed format
peer_addr | varchar(40) | Peer IP address (IPv4/IPv6) in printed format
peer_bgp_id | varchar(15) | Peer BGP ID in printed format
peer_as | unsigned int 32bit | Peer ASN
isL3VPN | boolean | Peer is VPNv4 if true and global if false
isIPv4 | boolean | Peer IP type is IPv4 if true and IPv6 if false
isPrePolicy | boolean | True if pre-policy (adj-rib-in) or false if post-policy (loc-rib)
state | int | Peer state is 0=down, 1=up, 2=receiving initial dump via bmp
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH


### path_attrs
BGP path attributes table primarily holds the path attributes for NLRI entries.  This includes MP_REACH.   BGP-LS attributes will be stored in separate (*new*) tables. 

Column | DataType | Description
------ | -------- | -----------
hash_id | char(32) | Hash ID of this table
peer_hash_id | char (32) | Hash ID of the bgp_peers table
origin | varchar(16) | BGP Origin in printed format
as_path | varchar(variable) | AS_PATH in string format
as_path_count | int | Count of AS's in the path, including in sets
origin_as | unsigned int 32bit | Origin AS - first ASN in path
nexthop_isIPv4 | boolean | Next-hop is IPv4 if true and IPv6 if false
next_hop | varchar(40) | IP address of next-hop in printed format
aggregator | varchar(40) | Aggregator AS and IP in printed format
originator_id| varchar(15) | Originator ID in printed format
atomic_agg | boolean | True if atomic aggregate, false if not
med | unsigned int 32bit | BGP med value
local_pref | unsigned int 32bit | BGP local preference
community_list | varchar(variable) | Standard community list in string format
ext_community_list | varchar(variable) | Extended community list in string format
cluster_list | varchar(variable) | Cluster list in string format
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH


### rib
The rib table details the prefixes, both IPv4, IPv6, and VPNv4.  Linking the path_attrs table is needed in order to determine the attributes of prefix.   The **v_routes** view provides a simple view of routes with their attributes. 

Column | DataType | Description
------ | -------- | -----------
hash_id | char(32) | Hash ID of this table
path_attr_hash_id | char(32) | Hash ID of the path_attrs table
peer_hash_id | char(32) | Hash ID of the bgp_peers table
prefix | varchar(40) | Prefix in printed format
prefix_len | int | Length of prefix in bits
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH


### peer_down_events
Peer down events are logged whenever received in this table. 

Column | DataType | Description
------ | -------- | -----------
peer_hash_id | char(32) | Hash ID of bgp_peers table
bmp_reason | varchar(64) | Short text description of the BMP reason code
bgp_error_code | int | BGP notification error code (see RFC4271 Section 4.5)
bgp_error_subcode | int | BGP notification error subcode (see RFC4271 Section 4.5)
error_text | varchar(255) | Text description of bgp error code and subcode meaning
timestamp| timestamp | BMP recorded time  - seconds since EPOCH

### peer_up_events
Peer up events are logged whenever bmp device is established with OpenBMP and when the peer transitions from down to up.

> It is possible to link this table to the peer table to get a full picture of 
> the peering session.   See **v_peers** view for peering session information.

Column | DataType | Description
------ | -------- | -----------
peer_hash_id | char(32) | Hash ID of the bgp_peers table
local_ip | varchar(40) | printed form of the Local BMP device peer IP address (IPv4 or IPv6)
local_port | int | Local port number for the peer session
remote_port | int | Remote port number for the peer session
hold_time | int | BGP hold time for the peer session
sent_open_params | varchar(variable) | String list of open params sent to peer
recv_open_params | varchar(variable) | String list of open params received from peer
timestamp| timestamp | BMP recorded time  - seconds since EPOCH

### stat_reports

Status reports are metrics based on periods defined/configured on the BMP device.  Each time the BMP device sends a report, an entry will be placed in this table. 

Column | DataType | Description
------ | -------- | -----------
peer_hash_id | char(32) | Hash ID of bgp_peers table
prefixes_rej | unsigned int 32bit | Number of prefixes rejected by inbound policy
known_dup_prefixes | unsigned int 32bit | Number of (known) duplicate prefix advertisements
known_dup_withdraws | unsigned int 32bit | Number of (known) duplicate withdraws
updates_invalid_cluster_list | unsigned int 32bit | Number of updates invalidated due to CLUSTER_LIST loop
updates_invalid_by_as_path_loop | unsigned int 32bit | Number of updates invalidated due to AS_PATH loop
updates_invalid_by_originator_id | unsigned int 32bit | Number of updates invalidated due to ORIGINATOR_ID
updates_invalid_by_as_confed_loop | unsigned int 32bit | Number of updates invalidated due to AS_CONFED loop
num_routes_adj_rib_in | unsigned int 64bit | Number of routes in Adj-RIBs-In
num_routes_local_rib | unsigned int 64bit | Number of routes in Loc-RIB
timestamp| timestamp | BMP recorded time  - seconds since EPOCH

> Other metrics can be added

### withdrawn_log
Every time a prefix is withdrawn a log entry is created in this table. 

Column | DataType | Description
------ | -------- | -----------
id | unsigned int 64bit | Incrementing number for log entry
peer_hash_id | char (32) | Hash ID of the bgp_peers table
prefix | varchar(40) | Prefix IP address in printed format
prefix_len | int | Length of prefix in bits
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH


### path_attr_log
Every time a prefix path attribute is changed, a log entry is added to this table. 

Column | DataType | Description
------ | -------- | -----------
rib_hash_id | char(32) | Hash ID of the rib table
path_attr_hash_id | char (32) | Hash ID of the path_attrs table for the current attrs, before update
timestamp| timestamp | Last time the record was updated  - seconds since EPOCH



