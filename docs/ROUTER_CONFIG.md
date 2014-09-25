Router Configurations
=====================
Below are some various example router configurations. 


IOS XE 3.12.0/15.4.2 (or greater)
---------------------------------
Link to vendor docs: [IOS XE](http://www.cisco.com/c/en/us/td/docs/ios-xml/ios/iproute_bgp/configuration/xe-3s/irg-xe-3s-book/bgp-monitor-protocol.html)

> ### WARNING
> See **IOS XR/XE refresh note** for more deatils
> 


```
router bgp <nnnn>
 bmp server 1
  address 10.20.254.245 port-number 5000
  description "BMP Server - primary"
  initial-delay 10
  failure-retry-delay 120
  flapping-delay 120
  stats-reporting-period 300
  update-source GigabitEthernet1
  activate
 exit-bmp-server-mode
 !
 bmp buffer-size 100
 !
 neighbor <ip/group> bmp-activate all
 neighbor ...
```


IOS XR 5.2.2 (or greater)
-------------------------
Link to vendor docs: [IOS XR]()

> ### WARNING
> See [IOS XR/XE refresh note](#iosRefresh)
> 

```
router bgp <nnnn>
 !
 neighbor <d.d.d.d>
  bmp-activate server 1
  ...
  !
 !
!
bmp server 1
 host 10.20.254.245 port 5000
 description BMP Server - primary
 update-source GigabitEthernet0/0/0/0
 initial-delay 10
 stats-reporting-period 300
!
```

JunOS 13.3 (or greater)
-----------------------
Link to vendor docs: [JunOS](http://www.juniper.net/techpubs/en_US/junos13.3/topics/task/configuration/bgp-monitoring-protocol-v3.html)

```
routing-options {
    bmp {
        station BMPServer1 {
            initiation-message "Development/LAB";
            connection-mode active;
            monitor enable;
            route-monitoring {
                pre-policy;
            }
            station-address 10.20.254.245;
            station-port 5000;
            statistics-timeout 300;
        }
    }
```


IOS XE/XR Policy Refresh Note
--------------------------------
Pre-Policy updates currently require a refresh to be sent to each monitored
peer when the BMP server connection is established.  

In a stable environment the BMP Server connection is persistent and does
not go up and down. 

Stopping the BMP server or breaking the connectivity/access to the BMP server can 
result in reconnections.  Reconnection/flapping is controlled by various/configurable 
timers/delays. 
 
When the BMP session is initially established or reconnected, IOS XE/XR will send a refresh
to monitored peers one at a time.  Spacing a delay between each peer can be done by
configuring the **initial-delay** parameter.   The default is to wait 30 seconds 
between each peer.  
 
Other options exist for this command, including the ability
to skip the refresh altogether, which basically disables initial router table dump on
BMP server connection.  In the case of **skip** only incremental updates will be sent, 
but at any time the **admin/operator** can **initiate a bgp refresh on-demand**, which 
will in turn forward all BGP UPDATES to the BMP server (full pre-policy dump).
