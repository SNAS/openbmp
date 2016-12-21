/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include "parseBgpLib.h"

#include <string>
#include <cstring>
#include <sstream>
#include <arpa/inet.h>
//TODO:Remove
#include "Logger.h"

namespace parse_bgp_lib {

/**
 * Constructor for class
 *
 * \details Handles bgp update messages
 *
 */
parseBgpLib::parseBgpLib(Logger *logPtr, bool enable_debug)
        : logger(logPtr),
          debug(enable_debug) {
}

parseBgpLib::~parseBgpLib(){
}

/**
 * Addpath capability for a peer
 *
 * \details
 * Enable Addpath capability for a peer which sent the Update message to be parsed
 * \param [in]   afi           AFI
 * \param [in]   safi          SAFI
 *
 * \return void
 */
void enableAddpathCapability(parse_bgp_lib::BGP_AFI, parse_bgp_lib::BGP_SAFI){
        
    }

/**
 * Addpath capability for a peer
 *
 * \details
 * Disable Addpath capability for a peer which sent the Update message to be parsed
 * \param [in]   afi           AFI
 * \param [in]   safi          SAFI
 *
 * \return void
 */
void disableAddpathCapability(parse_bgp_lib::BGP_AFI, parse_bgp_lib::BGP_SAFI){

    }

/**
  * Parses the update message
 *
 * \details
 * Parse BGP update message
 * \param [in]   data           Pointer to raw bgp payload data
 * \param [in]   size           Size of the data available to read; prevent overrun when reading
 * \param [out]  parsed_update  Reference to parsed_update; will be updated with all parsed data
 *
 * \return ZERO is error, otherwise a positive value indicating the number of bytes read from update message
 */
size_t parseBgpLib::parseBgpUpdate(u_char *data, size_t size, parsed_update &update) {
    size_t read_size = 0;
    u_char *bufPtr = data;
    uint16_t withdrawn_len;
    u_char *withdrawnPtr;
    uint16_t attr_len;
    u_char *attrPtr;
    u_char *nlriPtr;


    // Clear the parsed_data
    update.nlri_list.clear();
    update.withdrawn_nlri_list.clear();
    update.attr.clear();

    SELF_DEBUG("Parsing update message of size %d", size);

    if (size < 2) {
        LOG_WARN("Update message is too short to parse header");
        return 0;
    }

    // Get the withdrawn length
    memcpy(&withdrawn_len, bufPtr, sizeof(withdrawn_len));
    bufPtr += sizeof(withdrawn_len);
    read_size += sizeof(withdrawn_len);
    parse_bgp_lib::SWAP_BYTES(&withdrawn_len);

    // Set the withdrawn data pointer
    if ((size - read_size) < withdrawn_len) {
        LOG_WARN("Update message is too short to parse withdrawn data");
        return 0;
    }

    withdrawnPtr = bufPtr;
    bufPtr += withdrawn_len;
    read_size += withdrawn_len;

    SELF_DEBUG("Withdrawn len = %hu", withdrawn_len);

    // Get the attributes length
    memcpy(&attr_len, bufPtr, sizeof(attr_len));
    bufPtr += sizeof(attr_len);
    read_size += sizeof(attr_len);
    parse_bgp_lib::SWAP_BYTES(&attr_len);
    SELF_DEBUG("Attribute len = %hu", attr_len);

    // Set the attributes data pointer
    if ((size - read_size) < attr_len) {
        LOG_WARN("Update message is too short to parse attr data");
        return 0;
    }
    attrPtr = bufPtr;
    bufPtr += attr_len;
    read_size += attr_len;

    // Set the NLRI data pointer
    nlriPtr = bufPtr;

    /*
     * Check if End-Of-RIB
     */
    if (not withdrawn_len and (size - read_size) <= 0 and not attr_len) {
        LOG_INFO("End-Of-RIB marker");
    } else {
        /* ---------------------------------------------------------
         * Parse the withdrawn prefixes
         */
        SELF_DEBUG("Getting the IPv4 withdrawn data");
        if (withdrawn_len > 0)
            parseBgpNlri_v4(withdrawnPtr, withdrawn_len, update.withdrawn_nlri_list);


        /* ---------------------------------------------------------
         * Parse the attributes
         *      Handles MP_REACH/MP_UNREACH parsing as well
         */
        if (attr_len > 0)
            parseBgpAttr(attrPtr, attr_len, update);

        /* ---------------------------------------------------------
         * Parse the NLRI data
         */
        SELF_DEBUG("Getting the IPv4 NLRI data, size = %d", (size - read_size));
        if ((size - read_size) > 0) {
            parseBgpNlri_v4(nlriPtr, (size - read_size), update.nlri_list);
            read_size = size;
        }
    }
    return read_size;
}


/**
 * Parses the BGP attributes in the update
 *
 * \details
 *     Parses all attributes.  Decoded values are updated in 'parsed_data'
 *
 * \param [in]   data       Pointer to the start of the prefixes to be parsed
 * \param [in]   len        Length of the data in bytes to be read
 * \param [in]  parsed_data    Reference to parsed_update;
 */
void parseBgpLib::parseBgpAttr(u_char *data, uint16_t len, parsed_update &update){

    }

/**
 * Parses the BGP prefixes (advertised and withdrawn) in the update
 *
 * \details
 *     Parses all attributes.  Decoded values are updated in 'parsed_data'
 *
 * \param [in]   data       Pointer to the start of the prefixes to be parsed
 * \param [in]   len        Length of the data in bytes to be read
 * \param [in]   prefix_list Reference to parsed_update_data;
 */
void parseBgpLib::parseBgpNlri_v4(u_char *data, uint16_t len, std::list<parse_bgp_lib_nlri> &nlri_list){
        u_char       ipv4_raw[4];
        char         ipv4_char[16];
        u_char       addr_bytes;

        parse_bgp_lib::parse_bgp_lib_nlri nlri;


        if (len <= 0 or data == NULL)
            return;

        // Set the type for all to be unicast V4
        nlri.afi = parse_bgp_lib::BGP_AFI_IPV4;
        nlri.safi = parse_bgp_lib::BGP_SAFI_UNICAST;

        // Loop through all prefixes
        for (size_t read_size=0; read_size < len; read_size++) {

            bzero(ipv4_raw, sizeof(ipv4_raw));
            bzero(nlri.prefix_bin, sizeof(nlri.prefix_bin));

            // Parse add-paths if enabled
            if (peer_info->add_path_capability.isAddPathEnabled(bgp::BGP_AFI_IPV4, bgp::BGP_SAFI_UNICAST)
                and (len - read_size) >= 4) {
                memcpy(&tuple.path_id, data, 4);
                bgp::SWAP_BYTES(&tuple.path_id);
                data += 4; read_size += 4;
            } else
                tuple.path_id = 0;

            // set the address in bits length
            tuple.len = *data++;

            // Figure out how many bytes the bits requires
            addr_bytes = tuple.len / 8;
            if (tuple.len % 8)
                ++addr_bytes;

            SELF_DEBUG("%s: rtr=%s: Reading NLRI data prefix bits=%d bytes=%d", peer_addr.c_str(),
                       router_addr.c_str(), tuple.len, addr_bytes);

            if (addr_bytes <= 4) {
                memcpy(ipv4_raw, data, addr_bytes);
                read_size += addr_bytes;
                data += addr_bytes;

                // Convert the IP to string printed format
                inet_ntop(AF_INET, ipv4_raw, ipv4_char, sizeof(ipv4_char));
                tuple.prefix.assign(ipv4_char);
                SELF_DEBUG("%s: rtr=%s: Adding prefix %s len %d", peer_addr.c_str(),
                           router_addr.c_str(), ipv4_char, tuple.len);

                // set the raw/binary address
                memcpy(tuple.prefix_bin, ipv4_raw, sizeof(ipv4_raw));

                // Add tuple to prefix list
                prefixes.push_back(tuple);

            } else if (addr_bytes > 4) {
                LOG_NOTICE("%s: rtr=%s: NRLI v4 address is larger than 4 bytes bytes=%d len=%d",
                           peer_addr.c_str(), router_addr.c_str(), addr_bytes, tuple.len);
            }
        }
    }

}
