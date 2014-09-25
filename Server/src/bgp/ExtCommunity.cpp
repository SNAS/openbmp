/*
 * Copyright (c) 2014 Sungard Availability Services and others. All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 * 
 */

#include <arpa/inet.h>

#include "UpdateMsg.h"
#include "ExtCommunity.h"

namespace bgp_msg {

const ExtCommunity::subtypemap ExtCommunity:: evpnsubtype  = ExtCommunity::create_evpnsubtype();
const ExtCommunity::subtypemap ExtCommunity:: t2osubtype   = ExtCommunity::create_t2osubtype();
const ExtCommunity::subtypemap ExtCommunity:: nt2osubtype  = ExtCommunity::create_nt2osubtype();
const ExtCommunity::subtypemap ExtCommunity:: t4osubtype   = ExtCommunity::create_t4osubtype();
const ExtCommunity::subtypemap ExtCommunity:: nt4osubtype  = ExtCommunity::create_nt4osubtype();
const ExtCommunity::subtypemap ExtCommunity:: tip4subtype  = ExtCommunity::create_tip4subtype();
const ExtCommunity::subtypemap ExtCommunity:: ntip4subtype = ExtCommunity::create_ntip4subtype();
const ExtCommunity::subtypemap ExtCommunity:: topsubtype   = ExtCommunity::create_topsubtype();
const ExtCommunity::subtypemap ExtCommunity:: ntopsubtype  = ExtCommunity::create_ntopsubtype();
const ExtCommunity::subtypemap ExtCommunity:: gtesubtype   = ExtCommunity::create_gtesubtype();
const ExtCommunity::subtypemap ExtCommunity:: tafields     = ExtCommunity::create_tafields();

const ExtCommunity::v6typemap  ExtCommunity:: tip6types	   = ExtCommunity::create_tip6types();
const ExtCommunity::v6typemap  ExtCommunity:: ntip6types   = ExtCommunity::create_ntip6types();

ExtCommunity::ExtCommunity(Logger *logPtr, std::string peerAddr, bool enable_debug) {
    logger = logPtr;
    debug = enable_debug;
    peer_addr = peerAddr;
}

ExtCommunity::~ExtCommunity() {
}

ExtCommunity::subtypemap ExtCommunity::create_evpnsubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x00] = std::string("macmob");
    m[0x01] = std::string("esimpls");
    m[0x02] = std::string("esimport");
    m[0x03] = std::string("evpnrmac"); 

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_t2osubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x02] = std::string("tgt");
    m[0x03] = std::string("soo");
    m[0x05] = std::string("odi");
    m[0x08] = std::string("bdc");
    m[0x09] = std::string("sas");
    m[0x0a] = std::string("l2vpnid");
    m[0x10] = std::string("cisco-vpnd");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_nt2osubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x04] = std::string("linkbw");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_t4osubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x02] = std::string("tgt");
    m[0x03] = std::string("soo");
    m[0x05] = std::string("odi");
    m[0x08] = std::string("bdc");
    m[0x09] = std::string("sas");
    m[0x0a] = std::string("l2vpnid");
    m[0x10] = std::string("cisco-vpnd");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_nt4osubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x04] = std::string("generic"); // draft-ietf-idr-as4octet-extcomm-generic-subtype

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_tip4subtype(void) {
    ExtCommunity::subtypemap m;

    m[0x02] = std::string("tgt");
    m[0x03] = std::string("soo");
    m[0x05] = std::string("odi");
    m[0x07] = std::string("ori");
    m[0x08] = std::string("bdc");
    m[0x09] = std::string("sas");
    m[0x0a] = std::string("l2vpnid");
    m[0x0b] = std::string("vrfimport");
    m[0x10] = std::string("cisco-vpnd");
    m[0x12] = std::string("iap2mpsnh"); // draft-ietf-mpls-seamless-mcast 

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_ntip4subtype(void) {
    ExtCommunity::subtypemap m;

    // None assigned at this time.

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_topsubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x01] = std::string("cost");
    m[0x03] = std::string("cp-orf");
    m[0x06] = std::string("ort");
    m[0x0b] = std::string("color");
    m[0x0c] = std::string("encaps");
    m[0x0d] = std::string("defgw");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_ntopsubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x00] = std::string("bgp-ovs");
    m[0x01] = std::string("cost");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_gtesubtype(void) {
    ExtCommunity::subtypemap m;

    m[0x00] = std::string("ort");
    m[0x01] = std::string("ori");
    m[0x05] = std::string("odi");
    m[0x06] = std::string("flowspec-tr");
    m[0x07] = std::string("flowspec-ta");
    m[0x08] = std::string("flowspec-redir");
    m[0x09] = std::string("flowspec-remarking");
    m[0x0a] = std::string("l2info");

    return m;
}

ExtCommunity::subtypemap ExtCommunity::create_tafields(void) {
    ExtCommunity::subtypemap m;

    // Traffic action is a 48 bit field; the map isn't appropriate. Leave this here as a reminder to fix later.
    
    return m;
}

ExtCommunity::v6typemap ExtCommunity::create_tip6types(void) {
    ExtCommunity::v6typemap m;

    m[0x0002] = std::string("tgt");
    m[0x0003] = std::string("soo");
    m[0x0004] = std::string("ora");
    m[0x000b] = std::string("vrfimport");
    m[0x0010] = std::string("cisco-vpnd");
    m[0x0011] = std::string("uuid-tgt");
    m[0x0012] = std::string("iap2mpsnh");
    return m;
}

ExtCommunity::v6typemap ExtCommunity::create_ntip6types(void) {
    ExtCommunity::v6typemap m;

    // None assigned at this time.

    return m;

}

}
