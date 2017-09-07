# Using Logstash with OpenBMP

## Installing Logstash
[Visit download page](https://www.elastic.co/downloads/logstash)

## Starting logstash
Go to your logstash installation location, and run

`logstash -f openbmp-logstash.conf`

## Expanding
With logstash, you can easily get a variety of [possible outputs](https://www.elastic.co/guide/en/logstash/current/output-plugins.html). Here we provide elasticsearch output configuration with openBMP kafka input. To use other outputs or add custom data processing, add other plugins to **filter** section and **output** section. Note that **plugins execute in the order they appear**.

## Logstash config
```
input{
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.collector"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.collector"
			"TYPE"=>"collector"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.router"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.router"
			"TYPE"=>"router"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.peer"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.peer"
			"TYPE"=>"peer"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.bmp_stat"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.bmp_stat"
			"TYPE"=>"bmp-stat"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.base_attribute"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.base_attribute"
			"TYPE"=>"base-attribute"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.unicast_prefix"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.unicast_prefix"
			"TYPE"=>"unicast-prefix"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.ls_node"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.ls_node"
			"TYPE"=>"ls-node"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.ls_link"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.ls_link"
			"TYPE"=>"ls-link"
		}
	}
	kafka{
		bootstrap_servers=>"localhost:9092"
		topics=>"openbmp.parsed.ls_prefix"
		group_id=>"cra-logstash"
		codec=>plain
		add_field=>{
			"TOPIC"=>"openbmp.parsed.ls_prefix"
			"TYPE"=>"ls-prefix"
		}
	}
}

filter{
	mutate{
		split=>["message","

"]
		add_field=>{"HEADER"=>"%{message[0]}"}
	}
	split{
		field=>"message[1]"
		terminator=>"
"
	}
	if [TOPIC] == "openbmp.parsed.collector" {
		csv{
			columns=>["action","sequence","admin_id","hash","routers","router_count","timestamp"]
			convert=>{
				"sequence"=>"integer"
				"router_count"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.router" {
		csv{
			columns=>["action","sequence","name","hash","ip_address","description","term_code","term_reason","init_data","term_data","timestamp"]
			convert=>{
				"sequence"=>"integer"
				"term_code"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.peer" {
		csv{
			columns=>["action","sequence","hash","router_hash","name","remote_bgp_id","router_ip","timestamp","remote_asn","remote_ip","peer_rd","remote_port","local_asn","local_ip","local_port","local_bgp_id","info_data","adv_cap","recv_cap","remote_holddown","adv_holddown","bmp_reason","bgp_error_code","bgp_error_subcode","error_text","is_L3VPN","is_pre_policy","is_IPv4"]
			convert=>{
				"sequence"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.bmp_stat" {
		csv{
			columns=>["action","sequence","router_hash","router_ip","peer_hash","peer_ip","peer_asn","timestamp","prefixes_rejected","known_dup_updates","known_dup_withdraws","invalid_cluster_list","invalid_as_path","invalid_originator_id","invalid_as_confed","prefixes_pre_policy","prefixes_post_policy"]
			convert=>{
				"sequence"=>"integer"
				"prefixes_rejected"=>"integer"
				"known_dup_updates"=>"integer"
				"known_dup_withdraws"=>"integer"
				"invalid_cluster_list"=>"integer"
				"invalid_as_path"=>"integer"
				"invalid_originator_id"=>"integer"
				"invalid_as_confed"=>"integer"
				"prefixes_pre_policy"=>"integer"
				"prefixes_post_policy"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.base_attribute" {
		csv{
			columns=>["action","sequence","hash","router_hash","router_ip","peer_hash","peer_ip","peer_asn","timestamp","origin","as_path","as_path_count","origin_as","next_hop","MED","local_pref","aggregator","community_list","ext_community_list","cluster_list","is_atomic_agg","is_next_hop_IPv4","originator_id"]
			convert=>{
				"sequence"=>"integer"
				"as_path_count"=>"integer"
				"MED"=>"integer"
				"local_pref"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.unicast_prefix" {
		csv{
			columns=>["action","sequence","hash","router_hash","router_ip","base_attr_hash","peer_hash","peer_ip","peer_asn","timestamp","prefix","prefix_len","is_IPv4","origin","as_path","as_path_count","origin_as","next_hop","MED","local_pref","aggregator","community_list","ext_community_list","cluster_list","is_atomic_agg","is_next_hop_IPv4","originator_id"]
			convert=>{
				"sequence"=>"integer"
				"prefix_len"=>"integer"
				"as_path_count"=>"integer"
				"MED"=>"integer"
				"local_pref"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			add_field=>{"prefix_full"=>"%{prefix}/%{prefix_len}"}
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.ls_node" {
		csv{
			columns=>["action","sequence","hash","base_attr_hash","router_hash","router_ip","peer_hash","peer_ip","peer_asn","timestamp","igp_router_id","router_id","routing_id","ls_id","mt_id","ospf_area_id","isis_area_id","protocol","igp_flags","as_path","local_pref","MED","next_hop","name"]
			convert=>{
				"sequence"=>"integer"
				"MED"=>"integer"
				"local_pref"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.ls_link" {
		csv{
			columns=>["action","sequence","hash","base_attr_hash","router_hash","router_ip","peer_hash","peer_ip","peer_asn","timestamp","igp_router_id","router_id","routing_id","ls_id","ospf_area_id","isis_area_id","protocol","as_path","local_pref","MED","next_hop","mt_id","local_link_id","remote_link_id","interface_ip","neighbor_ip","igp_metric","admin_group","max_link_bw","max_resv_bw","unreserved_bw","te_default_metric","link_protection","mpls_proto_mask","srlg","link_name","remote_node_hash","local_node_hash"]
			convert=>{
				"sequence"=>"integer"
				"MED"=>"integer"
				"local_pref"=>"integer"
				"igp_metric"=>"integer"
				"te_default_metric"=>"integer"
				"max_link_bw"=>"float"
				"max_resv_bw"=>"float"
			}
			separator=>"	"
			source=>"message[1]"
			remove_field=>["message"]
		}
	}
	if [TOPIC] == "openbmp.parsed.ls_prefix" {
		csv{
			columns=>["action","sequence","hash","base_attr_hash","router_hash","router_ip","peer_hash","peer_ip","peer_asn","timestamp","igp_router_id","router_id","routing_id","ls_id","ospf_area_id","isis_area_id","protocol","as_path","local_pref","MED","next_hop","local_node_hash","mt_id","ospf_route_type","igp_flags","route_tag","ext_route_tag","ospf_fwd_addr","igp_metric","prefix","prefix_len"]
			convert=>{
				"sequence"=>"integer"
				"MED"=>"integer"
				"local_pref"=>"integer"
				"igp_metric"=>"integer"
				"prefix_len"=>"integer"
				"route_tag"=>"integer"
				"ext_route_tag"=>"integer"
			}
			separator=>"	"
			source=>"message[1]"
			add_field=>{"prefix_full"=>"%{prefix}/%{prefix_len}"}
			remove_field=>["message"]
		}
	}

	date{
		match => ["timestamp", "YYYY-MM-dd HH:mm:ss.SSSSSS"]
		remove_field => ["timestamp"]
	}
}

output{
#	stdout{
#	codec=>"json_lines"
#	}
	elasticsearch{
		index=>"openbmp"
		document_type=>"%{TYPE}"
	}
}
```
