/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef UPDATEMSG_H_
#define UPDATEMSG_H_

#include "Logger.h"
#include "bgp_common.h"
#include "MsgBusInterface.hpp"

#include <string>
#include <list>
#include <array>
#include <map>
#include <bmp/BMPReader.h>

namespace bgp_msg {
/**
 * Defines the attribute types
 *
 *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
 */
enum UPDATE_ATTR_TYPES {
            ATTR_TYPE_ORIGIN=1,
            ATTR_TYPE_AS_PATH,
            ATTR_TYPE_NEXT_HOP,
            ATTR_TYPE_MED,
            ATTR_TYPE_LOCAL_PREF,
            ATTR_TYPE_ATOMIC_AGGREGATE,
            ATTR_TYPE_AGGEGATOR,
            ATTR_TYPE_COMMUNITIES,
            ATTR_TYPE_ORIGINATOR_ID,
            ATTR_TYPE_CLUSTER_LIST,
            ATTR_TYPE_DPA,
            ATTR_TYPE_ADVERTISER,
            ATTR_TYPE_RCID_PATH,
            ATTR_TYPE_MP_REACH_NLRI=14,
            ATTR_TYPE_MP_UNREACH_NLRI,
            ATTR_TYPE_EXT_COMMUNITY=16,
            ATTR_TYPE_AS4_PATH=17,
            ATTR_TYPE_AS4_AGGREGATOR=18,

            ATTR_TYPE_AS_PATHLIMIT=21,              // Deprecated - draft-ietf-idr-as-pathlimit, JunOS will send this

            ATTR_TYPE_IPV6_EXT_COMMUNITY=25,
            ATTR_TYPE_AIGP,                         ///< RFC7311 - Accumulated IGP metric

            ATTR_TYPE_BGP_LS=29,                    // BGP LS attribute draft-ietf-idr-ls-distribution

            ATTR_TYPE_BGP_LINK_STATE_OLD=99,        // BGP link state Older
            ATTR_TYPE_BGP_ATTRIBUTE_SET=128,

            /*
             * Below attribute types are for internal use only... These are derived/added based on other attributes
             */
            ATTR_TYPE_INTERNAL_AS_COUNT=9000,        // AS path count - number of AS's
            ATTR_TYPE_INTERNAL_AS_ORIGIN             // The AS that originated the entry
};



/**
 * \class   UpdateMsg
 *
 * \brief   BGP update message parser
 * \details This class parses a BGP update message.  It can be extended to create messages.
 *          message.
 */
class UpdateMsg {

public:


    /**
     * Update header - defined in RFC4271
     */
    struct update_bgp_hdr {
        /**
         * indicates the total len of withdrawn routes field in octets.
         */
        uint16_t withdrawn_len;

        /**
         * Withdrawn routes data pointer
         */
        u_char *withdrawnPtr;

        /**
         * Total length of the path attributes field in octets
         *
         * A value of 0 indicates NLRI nor path attrs are present
         */
        uint16_t attr_len;

        /**
         * Attribute data pointer
         */
        u_char *attrPtr;

        /**
         * NLRI data pointer
         */
        u_char *nlriPtr;
    };

    /**
     * parsed path attributes map
     */
    std::map<bgp_msg::UPDATE_ATTR_TYPES, std::string>            parsed_attrs;
    typedef std::pair<bgp_msg::UPDATE_ATTR_TYPES, std::string>   parsed_attrs_pair;
    typedef std::map<bgp_msg::UPDATE_ATTR_TYPES, std::string>    parsed_attrs_map;

    // Parsed bgp-ls attributes map
    typedef  std::map<uint16_t, std::array<uint8_t, 255>>        parsed_ls_attrs_map;

    /**
     * Parsed data structure for BGP-LS
     */
    struct parsed_data_ls {
        std::list<MsgBusInterface::obj_ls_node>   nodes;        ///< List of Link state nodes
        std::list<MsgBusInterface::obj_ls_link>   links;        ///< List of link state links
        std::list<MsgBusInterface::obj_ls_prefix> prefixes;     ///< List of link state prefixes
    };

    /**
     * Parsed update data - decoded data from complete update parse
     */
    struct parsed_update_data {
        parsed_attrs_map              attrs;              ///< Parsed attrbutes
        std::list<bgp::prefix_tuple>  withdrawn;          ///< List of withdrawn prefixes
        std::list<bgp::prefix_tuple>  advertised;         ///< List of advertised prefixes

        parsed_ls_attrs_map           ls_attrs;           ///< BGP-LS specific attributes
        parsed_data_ls                ls;                 ///< REACH: Link state parsed data
        parsed_data_ls                ls_withdrawn;       ///< UNREACH: Parsed Withdrawn data
    };


    /**
     * Constructor for class
     *
     * \details Handles bgp update messages
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     pperAddr     Printed form of peer address used for logging
     * \param [in]     routerAddr  The router IP address - used for logging
     * \param [in,out] peer_info   Persistent peer information
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
     UpdateMsg(Logger *logPtr, std::string peerAddr, std::string routerAddr, BMPReader::peer_info *peer_info,
                bool enable_debug=false);
     virtual ~UpdateMsg();

     /**
      * Parses the update message
      *
      * \details
      *      Reads the update message from socket and parses it.  The parsed output will
      *      be added to the DB.
      *
      * \param [in]   data           Pointer to raw bgp payload data, starting at the notification message
      * \param [in]   size           Size of the data available to read; prevent overrun when reading
      * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
      *
      * \return ZERO is error, otherwise a positive value indicating the number of bytes read from update message
      */
     size_t parseUpdateMsg(u_char *data, size_t size, parsed_update_data &parsed_data);


private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer
    std::string      peer_addr;                       ///< Printed form of the peer address for logging
    std::string      router_addr;                     ///< Router IP address - used for logging
    bool             four_octet_asn;                  ///< Indicates true if 4 octets or false if 2
    BMPReader::peer_info *peer_info;                  ///< Persistent Peer info pointer


    /**
     * Parses NLRI info (IPv4) from the BGP message
     *
     * \details
     *      Will get the NLRI and Withdrawn prefix entries from the data buffer.  As per RFC,
     *      this is only for v4.  V6/mpls is via mpbgp attributes (RFC4760)
     *
     * \param [in]   data       Pointer to the start of the prefixes to be parsed
     * \param [in]   len        Length of the data in bytes to be read
     * \param [out]  prefixes   Reference to a list<prefix_tuple> to be updated with entries
     */
    void parseNlriData_v4(u_char *data, uint16_t len, std::list<bgp::prefix_tuple> &prefixes);

    /**
     * Parses the BGP attributes in the update
     *
     * \details
     *     Parses all attributes.  Decoded values are updated in 'parsed_data'
     *
     * \param [in]   data       Pointer to the start of the prefixes to be parsed
     * \param [in]   len        Length of the data in bytes to be read
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAttributes(u_char *data, uint16_t len, parsed_update_data &parsed_data);

    /**
     * Parse attribute data based on attribute type
     *
     * \details
     *      Parses the attribute data based on the passed attribute type.
     *      Parsed_data will be updated based on the attribute data parsed.
     *
     * \param [in]   attr_type      Attribute type
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  parsed_data    Reference to parsed_update_data; will be updated with all parsed data
     */
    void parseAttrData(u_char attr_type, uint16_t attr_len, u_char *data, parsed_update_data &parsed_data);

    /**
     * Parse attribute AS_PATH data
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  attrs          Reference to the parsed attr map - will be updated
     */
    void parseAttr_AsPath(uint16_t attr_len, u_char *data, parsed_attrs_map &attrs);

    /**
     * Parse attribute AGGEGATOR data
     *
     * \param [in]   attr_len       Length of the attribute data
     * \param [in]   data           Pointer to the attribute data
     * \param [out]  attrs          Reference to the parsed attr map - will be updated
     */
    void parseAttr_Aggegator(uint16_t attr_len, u_char *data, parsed_attrs_map &attrs);

};

} /* namespace bgp_msg */

#endif /* UPDATEMSG_H_ */
