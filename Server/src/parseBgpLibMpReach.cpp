/*
* Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*
*/

#include "parseBgpLibMpReach.h"
#include "parseBgpLib.h"
#include "parseBgpLibMpLinkstate.h"
#include "parseBgpLibMpEvpn.h"

#include <arpa/inet.h>

namespace parse_bgp_lib {

/**
* Constructor for class
*
* \details Handles BGP MP Reach NLRI
*
* \param [in]     logPtr                   Pointer to existing Logger for app logging
* \param [in]     enable_debug             Debug true to enable, false to disable
*/
MPReachAttr::MPReachAttr(parseBgpLib *parse_lib, Logger *logPtr, bool enable_debug)
        : logger{logPtr}, debug{enable_debug}, caller{parse_lib}{
}

MPReachAttr::~MPReachAttr() {
}

/**
* Parse the MP_REACH NLRI attribute data
*
* \details
*      Will parse the MP_REACH_NLRI data passed.  Parsed data will be stored
*      in parsed_data.
*
*      \see RFC4760 for format details.
*
* \param [in]   attr_len               Length of the attribute data
* \param [in]   data                   Pointer to the attribute data
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPReachAttr::parseReachNlriAttr(int attr_len, u_char *data, parse_bgp_lib::parseBgpLib::parsed_update &update) {
    mp_reach_nlri nlri;
    /*
     * Set the MP NLRI struct
     */
    // Read address family
    memcpy(&nlri.afi, data, 2); data += 2; attr_len -= 2;
    parse_bgp_lib::SWAP_BYTES(&nlri.afi);                     // change to host order

    nlri.safi = *data++; attr_len--;                 // Set the SAFI - 1 octet
    nlri.nh_len = *data++; attr_len--;              // Set the next-hop length - 1 octet
    nlri.next_hop = data;  data += nlri.nh_len; attr_len -= nlri.nh_len;    // Set pointer position for nh data
    nlri.reserved = *data++; attr_len--;             // Set the reserve octet
    nlri.nlri_data = data;                          // Set pointer position for nlri data
    nlri.nlri_len = attr_len;                       // Remaining attribute length is for NLRI data

    /*
     * Make sure the parsing doesn't exceed buffer
     */
    if (attr_len < 0) {
        LOG_NOTICE("MP_REACH NLRI data length is larger than attribute data length, skipping parse");
        return;
    }

    SELF_DEBUG("afi=%d safi=%d nh_len=%d reserved=%d",
               nlri.afi, nlri.safi, nlri.nh_len, nlri.reserved);

    /*
     * Next-hop and NLRI data depends on the AFI & SAFI
     *  Parse data based on AFI + SAFI
     */
    parseAfi(nlri, update);
}


/**
* MP Reach NLRI parse based on AFI
*
* \details Will parse the next-hop and nlri data based on AFI.  A call to
*          the specific SAFI method will be performed to further parse the message.
*
* \param [in]   nlri           Reference to parsed NLRI struct
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPReachAttr::parseAfi(mp_reach_nlri &nlri, parse_bgp_lib::parseBgpLib::parsed_update &update) {

        std::cout << "Manish parsing AFI/SAFI: " << nlri.afi << "/" << nlri.safi << std::endl;

    switch (nlri.afi) {
        case parse_bgp_lib::BGP_AFI_IPV6 :  // IPv6
            parseAfi_IPv4IPv6(false, nlri, update);
            break;

        case parse_bgp_lib::BGP_AFI_IPV4 : // IPv4
            parseAfi_IPv4IPv6(true, nlri, update);
            break;

        case parse_bgp_lib::BGP_AFI_BGPLS : // BGP-LS (draft-ietf-idr-ls-distribution-10)
        {
                MPLinkState ls(logger, &update, debug);
                ls.parseReachLinkState(nlri);

            break;
        }

        case parse_bgp_lib::BGP_AFI_L2VPN :
        {
            u_char      ip_raw[16];
            char        ip_char[40];

            bzero(ip_raw, sizeof(ip_raw));

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            inet_ntop(nlri.nh_len == 4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            update.attrs[LIB_ATTR_NEXT_HOP].official_type = ATTR_TYPE_NEXT_HOP;
            update.attrs[LIB_ATTR_NEXT_HOP].name = parse_bgp_lib_attr_names[LIB_ATTR_NEXT_HOP];
            update.attrs[LIB_ATTR_NEXT_HOP].value.push_back(std::string(ip_char));

            // parse by safi
            switch (nlri.safi) {
                case parse_bgp_lib::BGP_SAFI_EVPN : // https://tools.ietf.org/html/rfc7432
                {
                    EVPN evpn(logger, false, &update.nlri_list, debug);
                    evpn.parseNlriData(nlri.nlri_data, nlri.nlri_len);
                    break;
                }

                default :
                    LOG_INFO("EVPN::parse SAFI=%d is not implemented yet, skipping", nlri.safi);
            }

            break;
        }



        default : // Unknown
            LOG_INFO("MP_REACH AFI=%d is not implemented yet, skipping", nlri.afi);
            return;
    }
}

/**
* MP Reach NLRI parse for BGP_AFI_IPv4 & BGP_AFI_IPV6
*
* \details Will handle parsing the SAFI's for address family ipv6 and IPv4
*
* \param [in]   isIPv4         True false to indicate if IPv4 or IPv6
* \param [in]   nlri           Reference to parsed NLRI struct
* \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
*/
void MPReachAttr::parseAfi_IPv4IPv6(bool isIPv4, mp_reach_nlri &nlri, parse_bgp_lib::parseBgpLib::parsed_update &update) {
    u_char      ip_raw[16];
    char        ip_char[40];

    bzero(ip_raw, sizeof(ip_raw));

    /*
     * Decode based on SAFI
     */
    switch (nlri.safi) {
        case parse_bgp_lib::BGP_SAFI_UNICAST: // Unicast IP address prefix

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            if (not isIPv4)
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
            update.attrs[LIB_ATTR_NEXT_HOP].official_type = ATTR_TYPE_NEXT_HOP;
            update.attrs[LIB_ATTR_NEXT_HOP].name = parse_bgp_lib_attr_names[LIB_ATTR_NEXT_HOP];
            update.attrs[LIB_ATTR_NEXT_HOP].value.push_back(std::string(ip_char));

            // Data is an IP address - parse the address and save it
            parseNlriData_IPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, update.nlri_list, caller, debug, logger);
            break;

        case parse_bgp_lib::BGP_SAFI_NLRI_LABEL:
            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            if (not isIPv4)
                inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
            else
                inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));

            update.attrs[LIB_ATTR_NEXT_HOP].official_type = ATTR_TYPE_NEXT_HOP;
            update.attrs[LIB_ATTR_NEXT_HOP].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_ATTR_NEXT_HOP];
            update.attrs[LIB_ATTR_NEXT_HOP].value.push_back(std::string(ip_char));

            // Data is an Label, IP address tuple parse and save it
            parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, update.nlri_list, caller, debug, logger, nlri.safi);
            break;

        case parse_bgp_lib::BGP_SAFI_MPLS: {

            if (isIPv4) {
                //Next hop encoded in 12 bytes, last 4 bytes = IPv4
                nlri.next_hop += 8;
                nlri.nh_len -= 8;
            }

            // Next-hop is an IP address - Change/set the next-hop attribute in parsed data to use this next-hop
            if (nlri.nh_len > 16)
                memcpy(ip_raw, nlri.next_hop, 16);
            else
                memcpy(ip_raw, nlri.next_hop, nlri.nh_len);

            inet_ntop(isIPv4 ? AF_INET : AF_INET6, ip_raw, ip_char, sizeof(ip_char));

            update.attrs[LIB_ATTR_NEXT_HOP].official_type = ATTR_TYPE_NEXT_HOP;
            update.attrs[LIB_ATTR_NEXT_HOP].name = parse_bgp_lib_attr_names[LIB_ATTR_NEXT_HOP];
            update.attrs[LIB_ATTR_NEXT_HOP].value.push_back(std::string(ip_char));

            parseNlriData_LabelIPv4IPv6(isIPv4, nlri.nlri_data, nlri.nlri_len, update.nlri_list, caller, debug, logger, nlri.safi);

            break;
        }


        default :
            LOG_INFO("MP_REACH AFI=ipv4/ipv6 (%d) SAFI=%d is not implemented yet, skipping for now",
                     isIPv4, nlri.safi);
            return;
    }
}

/**
* Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
*
* \details
*      Will parse the NLRI encoding as defined in RFC4760 Section 5 (NLRI Encoding).
*
* \param [in]   isIPv4                 True false to indicate if IPv4 or IPv6
* \param [in]   data                   Pointer to the start of the prefixes to be parsed
* \param [in]   len                    Length of the data in bytes to be read
* \param [out]  nlri_list              Reference to a list<parse_bgp_lib_nlri> to be updated with entries
*/
void MPReachAttr::parseNlriData_IPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                         std::list<parseBgpLib::parse_bgp_lib_nlri> &nlri_list,
                                         parseBgpLib *parser, bool debug, Logger *logger) {
    u_char            ip_raw[16];
    char              ip_char[40];
    u_char            addr_bytes;
    uint32_t          path_id;
    u_char            prefix_len;
    std::ostringstream numString;


    if (len <= 0 or data == NULL)
        return;

        // Loop through all prefixes
    for (size_t read_size=0; read_size < len; read_size++) {
        parseBgpLib::parse_bgp_lib_nlri nlri;
        // TODO: Can extend this to support multicast, but right now we set it to unicast v4/v6
        nlri.afi = isIPv4 ? parse_bgp_lib::BGP_AFI_IPV4 : parse_bgp_lib::BGP_AFI_IPV6;
        nlri.safi = parse_bgp_lib::BGP_SAFI_UNICAST;
        nlri.type = parse_bgp_lib::LIB_NLRI_TYPE_NONE;

        // Generate the hash
        MD5 hash;

        bzero(ip_raw, sizeof(ip_raw));

        // Parse add-paths if enabled
        if (parser->getAddpathCapability(nlri.afi, nlri.safi)
            and (len - read_size) >= 4) {
            memcpy(&path_id, data, 4);
            parse_bgp_lib::SWAP_BYTES(&path_id);
            data += 4;
            read_size += 4;
        } else
            path_id = 0;

        numString.str(std::string());
        numString << path_id;
        nlri.nlri[LIB_NLRI_PATH_ID].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PATH_ID];
        nlri.nlri[LIB_NLRI_PATH_ID].value.push_back(numString.str());

        if (path_id > 0)
            update_hash(&nlri.nlri[LIB_NLRI_PATH_ID].value, &hash);

        // set the address in bits length
        prefix_len = *data++;
        numString.str(std::string());
        numString << static_cast<unsigned>(prefix_len);
        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_LENGTH];
        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value.push_back(numString.str());
        update_hash(&nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value, &hash);

        // Figure out how many bytes the bits requires
        addr_bytes = prefix_len / 8;
        if (prefix_len % 8)
            ++addr_bytes;

        SELF_DEBUG("Reading NLRI data prefix bits=%d bytes=%d", prefix_len, addr_bytes);

        memcpy(ip_raw, data, addr_bytes);
        data += addr_bytes;
        read_size += addr_bytes;

        // Convert the IP to string printed format
        if (isIPv4)
            inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
        else
            inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
        nlri.nlri[LIB_NLRI_PREFIX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX];
        nlri.nlri[LIB_NLRI_PREFIX].value.push_back(ip_char);
        update_hash(&nlri.nlri[LIB_NLRI_PREFIX].value, &hash);

        SELF_DEBUG("Adding prefix %s len %d", ip_char, prefix_len);

        // set the raw/binary address
        nlri.nlri[LIB_NLRI_PREFIX_BIN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_BIN];
        nlri.nlri[LIB_NLRI_PREFIX_BIN].value.push_back(std::string(ip_raw, ip_raw + 4));

        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        nlri.nlri[LIB_NLRI_HASH].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_NLRI_HASH];
        nlri.nlri[LIB_NLRI_HASH].value.push_back(parse_bgp_lib::hash_toStr(hash_raw));
        delete[] hash_raw;

        // Add tuple to prefix list
        nlri_list.push_back(nlri);
    }
}

/**
* Parses mp_reach_nlri and mp_unreach_nlri (IPv4/IPv6)
*
* \details
*      Will parse the NLRI encoding as defined in RFC3107 Section 3 (Carrying Label Mapping information).
*
* \param [in]   isIPv4                 True false to indicate if IPv4 or IPv6
* \param [in]   data                   Pointer to the start of the label + prefixes to be parsed
* \param [in]   len                    Length of the data in bytes to be read
* \param [out]  nlri_list              Reference to a list<parse_bgp_lib_nlri> to be updated with entries
*/
void MPReachAttr::parseNlriData_LabelIPv4IPv6(bool isIPv4, u_char *data, uint16_t len,
                                              std::list<parseBgpLib::parse_bgp_lib_nlri> &nlri_list,
                                              parseBgpLib *parser, bool debug, Logger *logger, int safi) {
    u_char ip_raw[16];
    char ip_char[40];
    int addr_bytes;
    uint32_t path_id;
    u_char prefix_len;
    std::ostringstream numString;
    char    buf2;                         // Second working buffer


        typedef union {
        struct {
            uint8_t ttl     : 8;          // TTL - not present since only 3 octets are used
            uint8_t bos     : 1;          // Bottom of stack
            uint8_t exp     : 3;          // EXP - not really used
            uint32_t value   : 20;         // Label value
        } decode;
        uint32_t data;                 // Raw label - 3 octets only per RFC3107
    } mpls_label;

    mpls_label label;

    if (len <= 0 or data == NULL)
        return;

    int parsed_bytes = 0;

    // Loop through all prefixes
    for (size_t read_size = 0; read_size < len; read_size++) {
        parseBgpLib::parse_bgp_lib_nlri nlri;
        nlri.afi = isIPv4 ? parse_bgp_lib::BGP_AFI_IPV4 : parse_bgp_lib::BGP_AFI_IPV6;
        nlri.safi = (BGP_SAFI) safi;
        nlri.type = parse_bgp_lib::LIB_NLRI_TYPE_NONE;

        // Generate the hash
        MD5 hash;

        // Parse add-paths if enabled
        if ((safi != parse_bgp_lib::BGP_SAFI_MPLS) and parser->getAddpathCapability(nlri.afi, nlri.safi)
            and (len - read_size) >= 4) {
            memcpy(&path_id, data, 4);
            parse_bgp_lib::SWAP_BYTES(&path_id);
            data += 4;
            read_size += 4;
        } else
            path_id = 0;

        numString.str(std::string());
        numString << path_id;
        nlri.nlri[LIB_NLRI_PATH_ID].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PATH_ID];
        nlri.nlri[LIB_NLRI_PATH_ID].value.push_back(numString.str());
        if (path_id > 0)
            update_hash(&nlri.nlri[LIB_NLRI_PATH_ID].value, &hash);

        if (parsed_bytes == len) {
            break;
        }

        bzero(&label, sizeof(label));
        bzero(ip_raw, sizeof(ip_raw));

        // set the address in bits length
        prefix_len = *data++;

        // Figure out how many bytes the bits requires
        addr_bytes = prefix_len / 8;
        if (prefix_len % 8)
            ++addr_bytes;

        SELF_DEBUG("Reading NLRI data prefix bits=%d bytes=%d", prefix_len, addr_bytes);

        nlri.nlri[LIB_NLRI_LABELS].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_LABELS];
        // the label is 3 octets long
        while (addr_bytes >= 3) {
            memcpy(&label.data, data, 3);
            parse_bgp_lib::SWAP_BYTES(&label.data);     // change to host order

            data += 3;
            addr_bytes -= 3;
            read_size += 3;
            prefix_len -= 24;        // Update prefix len to not include the label just parsed
            std::ostringstream convert;
            convert << label.decode.value;
            nlri.nlri[LIB_NLRI_LABELS].value.push_back(convert.str());

            if (label.decode.bos == 1 or label.data == 0x80000000 /* withdrawn label as 32bits instead of 24 */) {
                break;               // Reached EoS

            }
        }

        /*
         * Add constant "1" to hash if labels are present
         *      Withdrawn and updated NLRI's do not carry the original label, therefore we cannot
         *      hash on the label string.  Instead, we has on a constant value of 1.
         */
        if (nlri.nlri[LIB_NLRI_LABELS].value.size() != 0) {
            buf2 = 1;
            hash.update((unsigned char *) &buf2, 1);
            buf2 = 0;
        }

        // Parse RD if VPN
        if ((safi == parse_bgp_lib::BGP_SAFI_MPLS) and addr_bytes >= 8) {
            EVPN evpn(logger, false, &nlri_list, debug);
            evpn.parseRouteDistinguisher(data, nlri);
            data += 8;
            addr_bytes -= 8;
            read_size += 8;
            prefix_len -= 24;        // Update prefix len to not include the label just parsed
            update_hash(&nlri.nlri[LIB_NLRI_VPN_RD_ADMINISTRATOR_SUBFIELD].value, &hash);
            update_hash(&nlri.nlri[LIB_NLRI_VPN_RD_ASSIGNED_NUMBER].value, &hash);
        }

        numString.str(std::string());
        numString << static_cast<unsigned>(prefix_len);
        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_LENGTH];
        nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value.push_back(numString.str());
        update_hash(&nlri.nlri[LIB_NLRI_PREFIX_LENGTH].value, &hash);

        memcpy(ip_raw, data, addr_bytes);
        data += addr_bytes;
        read_size += addr_bytes;

        // Convert the IP to string printed format
        if (isIPv4)
            inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
        else
            inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));

        nlri.nlri[LIB_NLRI_PREFIX].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX];
        nlri.nlri[LIB_NLRI_PREFIX].value.push_back(ip_char);
        update_hash(&nlri.nlri[LIB_NLRI_PREFIX].value, &hash);

        SELF_DEBUG("Adding prefix %s len %d", ip_char, prefix_len);

        // set the raw/binary address
        nlri.nlri[LIB_NLRI_PREFIX_BIN].name = parse_bgp_lib::parse_bgp_lib_nlri_names[LIB_NLRI_PREFIX_BIN];
        nlri.nlri[LIB_NLRI_PREFIX_BIN].value.push_back(std::string(ip_raw, ip_raw + 4));

        //Now save the generate hash
        hash.finalize();

        // Save the hash
        unsigned char *hash_raw = hash.raw_digest();
        nlri.nlri[LIB_NLRI_HASH].name = parse_bgp_lib::parse_bgp_lib_attr_names[LIB_NLRI_HASH];
        nlri.nlri[LIB_NLRI_HASH].value.push_back(parse_bgp_lib::hash_toStr(hash_raw));
        delete[] hash_raw;
        // Add tuple to prefix list
        nlri_list.push_back(nlri);
    }
}

} /* namespace parse_bgp_lib */
