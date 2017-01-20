/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef BGPCOMMON_H_
#define BGPCOMMON_H_

#include <string>
#include <cstdint>
#include <sstream>
#include <cinttypes>
#include <cstring>
#include <sys/types.h>

namespace bgp {
    #define BGP_MAX_MSG_SIZE        65535                   // Max payload size - Larger than RFC4271 of 4096
    #define BGP_MSG_HDR_LEN         19                      // BGP message header size
    #define BGP_OPEN_MSG_MIN_LEN    29                      // Includes the expected header size
    #define BGP_VERSION             4
    #define BGP_CAP_PARAM_TYPE      2
    #define BGP_AS_TRANS            23456                   // BGP ASN when AS exceeds 16bits


    /**
     * defines whether the attribute is optional (if
     *    set to 1) or well-known (if set to 0)
     */
    #define ATTR_FLAG_OPT(flags)        ( flags & 0x80 )

    /**
     * defines whether an optional attribute is
     *    transitive (if set to 1) or non-transitive (if set to 0)
     */
    #define ATTR_FLAG_TRANS(flags)      ( flags & 0x40 )

    /**
     * defines whether the information contained in the
     *  (if set to 1) or complete (if set to 0)
     */
    #define ATTR_FLAG_PARTIAL(flags)    ( flags & 0x20 )

    /**
     * defines whether the Attribute Length is one octet
     *      (if set to 0) or two octets (if set to 1)
     *
     * \details
     *         If the Extended Length bit of the Attribute Flags octet is set
     *         to 0, the third octet of the Path Attribute contains the length
     *         of the attribute data in octets.
     *
     *         If the Extended Length bit of the Attribute Flags octet is set
     *         to 1, the third and fourth octets of the path attribute contain
     *         the length of the attribute data in octets.
     */
     #define ATTR_FLAG_EXTENDED(flags)   ( flags & 0x10 )

    /**
     * Defines the BGP address-families (AFI)
     *      http://www.iana.org/assignments/address-family-numbers/address-family-numbers.xhtml
     */
    enum BGP_AFI {
        BGP_AFI_IPV4=1,
        BGP_AFI_IPV6=2,
        BGP_AFI_L2VPN=25,
        BGP_AFI_BGPLS=16388
    };

    /**
     * Defines the BGP subsequent address-families (SAFI)
     *      http://www.iana.org/assignments/safi-namespace/safi-namespace.xhtml
     */
    enum BGP_SAFI {
        BGP_SAFI_UNICAST=1,
        BGP_SAFI_MULTICAST=2,

        BGP_SAFI_NLRI_LABEL=4,          // RFC3107
        BGP_SAFI_MCAST_VPN,             // RFC6514

        BGP_SAFI_VPLS=65,               // RFC4761, RFC6074
        BGP_SAFI_MDT,                   // RFC6037
        BGP_SAFI_4over6,                // RFC5747
        BGP_SAFI_6over4,                // yong cui

        BGP_SAFI_EVPN=70,               // draft-ietf-l2vpn-evpn
        BGP_SAFI_BGPLS=71,              // draft-ietf-idr-ls-distribution

        BGP_SAFI_MPLS=128,              // RFC4364
        BGP_SAFI_MCAST_MPLS_VPN,        // RFC6513, RFC6514

        BGP_SAFI_RT_CONSTRAINS=132      // RFC4684
    };

    /**
     * ENUM to define the prefix type used for prefix nlri maps in returned data
     */
    enum PREFIX_TYPE {
                PREFIX_UNICAST_V4=1,
                PREFIX_UNICAST_V6,
                PREFIX_LABEL_UNICAST_V4,
                PREFIX_LABEL_UNICAST_V6,
                PREFIX_VPN_V4,
                PREFIX_VPN_v6,
                PREFIX_MULTICAST_V4,
                // Add BGP-LS types
    };

    /**
      * struct is used for nlri prefixes
      */
    struct prefix_tuple {
        /**
        * len in bits of the IP address prefix
        *      length of 0 indicates a prefix that matches all IP addresses
        */
        PREFIX_TYPE   type;                 ///< Prefix type - RIB type
        unsigned char len;                  ///< Length of prefix in bits
        std::string   prefix;               ///< Printed form of the IP address
        uint8_t       prefix_bin[16];       ///< Prefix in binary form
        uint32_t      path_id;              ///< Path ID (add path draft-ietf-idr-add-paths-15)
        bool          isIPv4;               ///< True if IPv4, false if IPv6

        std::string   labels;               ///< Labels in the format of label, label, ...
    };

    /**
    * Struct for Route Distinguisher
    */
    struct rd_tuple {
        std::string    rd_administrator_subfield;
        std::string    rd_assigned_number;
        uint8_t        rd_type;
    };
     
    /**
    * struct is used for l3vpn
    */
    struct vpn_tuple: prefix_tuple, rd_tuple {
        // inherits
    };

    /**
    * Struct is used for evpn
    */
    struct evpn_tuple: prefix_tuple, rd_tuple {
        std::string     ethernet_segment_identifier;
        std::string     ethernet_tag_id_hex;
        uint8_t         mac_len;
        std::string     mac;
        uint8_t         ip_len;
        std::string     ip;
        int             mpls_label_1;
        int             mpls_label_2;
        uint8_t         originating_router_ip_len;
        std::string     originating_router_ip;
    };

    /*********************************************************************//**
     * Simple function to swap bytes around from network to host or
     *  host to networking.  This method will convert any size byte variable,
     *  unlike ntohs and ntohl.
     *
     * @param [in/out] var   Variable containing data to update
     * @param [in]     size  Size of var - Default is size of var
     *********************************************************************/
    inline std::string parse_mac(u_char *data_pointer) {
        u_char *pointer = data_pointer;

        std::ostringstream mac_stringstream;

        for (int i = 0; i < 6; ++i) {
            if (i != 0) mac_stringstream << ':';
            mac_stringstream.width(2);
            mac_stringstream.fill('0');
            mac_stringstream << std::hex << (int)(pointer[i]);
        }

        return mac_stringstream.str();
    }

    /*********************************************************************//**
     * Simple function to swap bytes around from network to host or
     *  host to networking.  This method will convert any size byte variable,
     *  unlike ntohs and ntohl.
     *
     * @param [in/out] var   Variable containing data to update
     * @param [in]     size  Size of var - Default is size of var
     *********************************************************************/
    template <typename VarT>
    void SWAP_BYTES(VarT *var, int size=sizeof(VarT)) {
        if (size <= 1)
            return;

        u_char *v = (u_char *)var;

        // Allocate a working buffer
        u_char buf[size];

        // Make a copy
        memcpy(buf, var, size);

        int i2 = 0;
        for (int i=size-1; i >= 0; i--)
            v[i2++] = buf[i];

    }

    /**
     * Function to get string representation of AFI code.
     * @param code AFI http://www.iana.org/assignments/address-family-numbers/address-family-numbers.xhtml
     * @return string like "IPv6" or "IPv4"
     */
    inline std::string GET_AFI_STRING_BY_CODE(int code) {
        std::string afi_string;

        switch (code) {
            case bgp::BGP_AFI_IPV4 :
                afi_string = "IPv4";
                break;

            case bgp::BGP_AFI_IPV6 :
                afi_string = "IPv6";
                break;

            case bgp::BGP_AFI_BGPLS :
                afi_string = "BGP-LS";
                break;

            default:
                afi_string = "unknown";
                break;
        }
        return afi_string;
    }

    /**
     * Function to get string representation of SAFI code.
     * @param code SAFI http://www.iana.org/assignments/safi-namespace/safi-namespace.xhtml
     * @return string like "Unicast" or "Multicast"
     */
    inline std::string GET_SAFI_STRING_BY_CODE(int code) {
        std::string safi_string;

        switch (code) {
            case bgp::BGP_SAFI_UNICAST : // Unicast IP forwarding
                safi_string = "Unicast";
                break;

            case bgp::BGP_SAFI_MULTICAST : // Multicast IP forwarding
                safi_string = "Multicast";
                break;

            case bgp::BGP_SAFI_NLRI_LABEL : // NLRI with MPLS Labels
                safi_string = "Labeled Unicast";
                break;

            case bgp::BGP_SAFI_MCAST_VPN : // MCAST VPN
                safi_string = "MCAST VPN";
                break;

            case bgp::BGP_SAFI_VPLS : // VPLS
                safi_string = "VPLS";
                break;

            case bgp::BGP_SAFI_MDT : // BGP MDT
                safi_string = "BGP MDT";
                break;

            case bgp::BGP_SAFI_4over6 : // BGP 4over6
                safi_string = "BGP 4over6";
                break;

            case bgp::BGP_SAFI_6over4 : // BGP 6over4
                safi_string = "BGP 6over4";
                break;

            case bgp::BGP_SAFI_EVPN : // BGP EVPNs
                safi_string = "BGP EVPNs";
                break;

            case bgp::BGP_SAFI_BGPLS : // BGP-LS
                safi_string = "BGP-LS";
                break;

            case bgp::BGP_SAFI_MPLS : // MPLS-Labeled VPN
                safi_string = "MPLS-Labeled VPN";
                break;

            case bgp::BGP_SAFI_MCAST_MPLS_VPN : // Multicast BGP/MPLS VPN
                safi_string = "Multicast BGP/MPLS VPN";
                break;

            case bgp::BGP_SAFI_RT_CONSTRAINS : // Route target constrains
                safi_string = "RT constrains";
                break;

        }
        return safi_string;
    }

}

#endif /* BGPCOMMON_H */
