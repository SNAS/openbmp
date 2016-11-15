Router BGP-LS Configurations
=========================
Below are bgp-ls example router configurations for IOS-XR and Junos. 




IOS XR 5.2.2 (or greater)
-------------------------
Link to vendor docs: [IOS XR](http://www.cisco.com/c/en/us/td/docs/routers/asr9000/software/asr9k_r5-2/routing/configuration/guide/b_routing_cg52xasr9k/b_routing_cg52xasr9k_chapter_010.html)

#### Enable IGP to BGP distribution:

```
For ISIS :

router isis <name>
 distribute bgp-ls
 address-family ipv4 unicast
    mpls traffic-eng level-2-only
 !
! 
 
For OSPF:

router ospf <name>
distribute bgp-ls
!
```

#### Enable link-state address family to advertise bgp-ls routes:

```
router bgp <nnnn>
 !
address-family link-state link-state 
 neighbor <d.d.d.d>
  address-family link-state link-state
 !
!
```

JunOS 14.2 (or greater)
------------------------
Link to vendor docs: [JunOS/Overview](http://www.juniper.net/documentation/en_US/junos14.2/topics/concept/bgp-link-state-distribution-overview.html)

Link to vendor docs: [JunOS/Example](https://www.juniper.net/documentation/en_US/junos16.1/topics/example/example-bgp-link-state-distribution-configuring.html)
#### Configure rsvp to initialize TE:
```
protocols {
    rsvp {
        interface all;
        interface fxp0.0 {
            disable;
        }
    }
```

#### Import IGP routes to Lsdist.0 table:

```
protocols {
    mpls {
        traffic-engineering {
            database {
                import {
                    policy nlri2bgp;
                }
            }
        }
        interface all;
        interface fxp0.0 {
            disable;
        }
    }
}
policy-options {
    policy-statement nlri2bgp {
        term 1 {
            from family traffic-engineering;
            then accept;
        }
    }
}
```

#### Turn on bgp-ls address family and export bgp-ls routes:

```
protocols {
    bgp {
        group <tttt> {
            neighbor d.d.d.d {
                family traffic-engineering {
                    unicast;
                }
                export nlri2bgp;
            }
         }
     }
}

```

