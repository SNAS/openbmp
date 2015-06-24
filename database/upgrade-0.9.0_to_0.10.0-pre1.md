DB Alter Changes for 0.10.1 from 0.9.0
--------------------------------------
Below are the details for upgrading the live database from 0.9.0-x to0.10.0-pre1.  Pre release do not always contain DB schema updates.   This document applies to other 0.10.0-preX releases till a more specific is provided.  

### SQL commands to perform the schema upgrade
> It's best to **disconnect the collectors** from the DB (shut them down) before starting the upgrade. This will avoid locking issues.

Login to mysql and run the below:

    alter table gen_whois_route change column descr descr blob;
  
    # Below will take a while if there are many records since the table is rebuilt
    # Make sure you have enough disk space for the rebuild.
    alter table path_attr_log ROW_FORMAT=DYNAMIC;

    # Below will also take a while if there are many records
    alter table rib add column origin_as int(10) unsigned NOT NULL, 
            add index idx_isWithdrawn (isWithdrawn),
            add index idx_origin_as (origin_as); 

    alter table path_attr_log add column peer_hash_id char(32) NOT NULL DEFAULT '', add index idx_peer_hash_id (peer_hash_id);

    drop view v_peer_prefix_report_last_id;
    create view v_peer_prefix_report_last_id AS
    SELECT max(id) as id,peer_hash_id
              FROM stat_reports 
              WHERE timestamp >= date_sub(current_timestamp, interval 72 hour) 
              GROUP BY peer_hash_id;
    
    drop table if exists as_path_analysis;
    CREATE TABLE as_path_analysis (
        asn int(10) unsigned not null,
        asn_left int(10) unsigned not null default '0',
        asn_right int(10) unsigned not null default '0',
        path_attr_hash_id char(32) NOT NULL,
        peer_hash_id char(32) NOT NULL,
        timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
        PRIMARY KEY (asn,path_attr_hash_id),
        KEY idx_asn_left (asn_left),
        KEY idx_asn_right (asn_right),
        KEY idx_path_id (path_attr_hash_id),
        KEY idx_peer_id (peer_hash_id)
    ) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

    
    drop view v_routes;
    CREATE  VIEW `v_routes` AS 
           SELECT  if (length(rtr.name) > 0, rtr.name, rtr.ip_address) AS RouterName, 
                    if(length(p.name) > 0, p.name, p.peer_addr) AS `PeerName`,
                    `r`.`prefix` AS `Prefix`,`r`.`prefix_len` AS `PrefixLen`,
                    `path`.`origin` AS `Origin`,`r`.`origin_as` AS `Origin_AS`,`path`.`med` AS `MED`,
                    `path`.`local_pref` AS `LocalPref`,`path`.`next_hop` AS `NH`,`path`.`as_path` AS `AS_Path`,
                    `path`.`as_path_count` AS `ASPath_Count`,`path`.`community_list` AS `Communities`,
                    `path`.`ext_community_list` AS `ExtCommunities`,
                    `path`.`cluster_list` AS `ClusterList`,`path`.`aggregator` AS `Aggregator`,`p`.`peer_addr` AS `PeerAddress`, `p`.`peer_as` AS `PeerASN`,r.isIPv4 as isIPv4,
                     p.isIPv4 as isPeerIPv4, p.isL3VPNpeer as isPeerVPN,
                    `r`.`timestamp` AS `LastModified`, r.db_timestamp as DBLastModified,r.prefix_bin as prefix_bin,
                     r.hash_id as rib_hash_id,
                     r.path_attr_hash_id as path_hash_id, r.peer_hash_id, rtr.hash_id as router_hash_id,r.isWithdrawn
            FROM bgp_peers p JOIN rib r ON (r.peer_hash_id = p.hash_id) 
                JOIN path_attrs path ON (path.hash_id = r.path_attr_hash_id)
                JOIN routers rtr ON (p.router_hash_id = rtr.hash_id)
           WHERE r.isWithdrawn = False;

    drop trigger rib_pre_update;
    delimiter //
    CREATE TRIGGER rib_pre_update BEFORE UPDATE on rib
         FOR EACH ROW

         # Make sure we are updating a duplicate
         IF (new.hash_id = old.hash_id AND new.peer_hash_id = old.peer_hash_id) THEN
              IF (new.isWithdrawn = False AND old.path_attr_hash_id != new.path_attr_hash_id) THEN
                 # Add path log if the path has changed
                  INSERT IGNORE INTO path_attr_log (rib_hash_id,path_attr_hash_id,peer_hash_id,timestamp) 
                              VALUES (old.hash_id,old.path_attr_hash_id,old.peer_hash_id,
                                      current_timestamp(6));

              ELSEIF (new.isWithdrawn = True) THEN
                 # Add log entry for withdrawn prefix
                  INSERT IGNORE INTO withdrawn_log
                          (prefix,prefix_len,peer_hash_id,path_attr_hash_id,timestamp) 
                              VALUES (old.prefix,old.prefix_len,old.peer_hash_id,
                                      old.path_attr_hash_id,current_timestamp(6));
                 
              END IF;       
          END IF //
    delimiter ;

