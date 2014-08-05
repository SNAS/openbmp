/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef PARSEBGP_H_
#define PARSEBGP_H_

#include <vector>
#include "DbInterface.hpp"
#include "BitByteUtils.h"
#include "Logger.h"

#define BGP_MSG_HDR_LEN 19

using namespace std;

#define BGP_SWAP_BYTES(var) reverseBytes((unsigned char *)&var, sizeof(var))

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
 * \class   parseBGP
 *
 * \brief   Parser for BGP messages
 * \details This class can be used as needed to parse a complete BGP message. This
 *          class will read directly from the socket to read the BGP message.
 */
class parseBGP {
public:
    /**
     * Below defines the common BGP header per RFC4271
     */
    enum BGP_MSG_TYPES { BGP_MSG_OPEN=1, BGP_MSG_UPDATE, BGP_MSG_NOTIFICATION, BGP_MSG_KEEPALIVE,
        BGP_MSG_ROUTE_REFRESH
    };

    struct common_bgp_hdr {
        /**
         * 16-octet field is included for compatibility
         * All ones (required).
         */
        unsigned char    marker[16];

        /**
         * Total length of the message, including the header in octets
         *
         * min length is 19, max is 4096
         */
        unsigned short   len;

        /**
         * type code of the message
         *
         * 1 - OPEN
         * 2 - UPDATE
         * 3 - NOTIFICATION
         * 4 - KEEPALIVE
         * 5 - ROUTE-REFRESH
         */
        unsigned char    type;
    } __attribute__ ((__packed__)) c_hdr;

    /**
    * defines the OPEN BGP header per RFC4271
    */
    struct open_param {
        unsigned char type;                 ///< unambiguously identifies parameters
        unsigned char len;                  ///< parameter value length in octets
        unsigned char *value;               ///< pointer to array - must be deleted
    };

    struct open_bgp_hdr {
        unsigned char     ver;              ///< Version, currently 4
        unsigned short    my_as : 16;       ///< 2 byte ASN - AS_TRANS = 23456 to indicate 4-octet ASN
    unsigned short    hold : 16;            ///< 2 byte hold time - can be zero or >= 3 seconds
        unsigned long     bgp_id : 32;      ///< 4 byte bgp id of sender - router_id
        unsigned char     param_len;        ///< optional parameter length - 0 means no params
        struct open_param *params;          ///< Array of parameters
    } __attribute__ ((__packed__));



    /**
     * struct is used for both withdrawn and nlri prefixes
     */
    struct prefix_2tuple {
        /**
         * len in bits of the IP address prefix
         *      length of 0 indicates a prefix that matches all IP addresses
         */
        unsigned char len;

        unsigned char prefix_v4[4];         ///< IP address prefix - ipv4
        unsigned char prefix_v6[16];        ///< IP address prefix - ipv6
    };

    /**
     * struct defines the MP_REACH_NLRI
     */
    struct mp_reach_nlri {
        unsigned short afi;                 ///< Address Family Identifier
        unsigned char  safi;                ///< Subsequent Address Family Identifier
        unsigned char  nh_len;              ///< Length of next hop
        unsigned char  next_hop[16];        ///< Next hop
        unsigned char  reserved;            ///< Reserved

        vector<prefix_2tuple> prefixes;     ///< Prefix tuple
    };

    /**
     * Defines the attribute types
     *
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum UPDATE_ATTR_TYPES {
                ATTR_TYPE_ORIGIN=1,             ///< ATTR_TYPE_ORIGIN
                ATTR_TYPE_AS_PATH,              ///< ATTR_TYPE_AS_PATH
                ATTR_TYPE_NEXT_HOP,             ///< ATTR_TYPE_NEXT_HOP
                ATTR_TYPE_MED,                  ///< ATTR_TYPE_MED
                ATTR_TYPE_LOCAL_PREF,           ///< ATTR_TYPE_LOCAL_PREF
                ATTR_TYPE_ATOMIC_AGGREGATE,     ///< ATTR_TYPE_ATOMIC_AGGREGATE
                ATTR_TYPE_AGGEGATOR,            ///< ATTR_TYPE_AGGEGATOR
                ATTR_TYPE_COMMUNITIES,          ///< ATTR_TYPE_COMMUNITIES
                ATTR_TYPE_ORIGINATOR_ID,        ///< ATTR_TYPE_ORIGINATOR_ID
                ATTR_TYPE_CLUSTER_LIST,         ///< ATTR_TYPE_CLUSTER_LIST
                ATTR_TYPE_DPA,                  ///< ATTR_TYPE_DPA
                ATTR_TYPE_ADVERTISER,           ///< ATTR_TYPE_ADVERTISER
                ATTR_TYPE_RCID_PATH,            ///< ATTR_TYPE_RCID_PATH
                ATTR_TYPE_MP_REACH_NLRI,        ///< ATTR_TYPE_MP_REACH_NLRI
                ATTR_TYPE_MP_UNREACH_NLRI,      ///< ATTR_TYPE_MP_UNREACH_NLRI
                ATTR_TYPE_EXT_COMMUNITY=16,     ///< ATTR_TYPE_EXT_COMMUNITY
                ATTR_TYPE_AS4_PATH=17,          ///< ATTR_TYPE_AS4_PATH
                ATTR_TYPE_AS4_AGGREGATOR=18     ///< ATTR_TYPE_AS4_AGGREGATOR
    };


    /**
     * Struct defining the path attributes hdr
     */
    struct update_path_attrs {
        unsigned char flags;                ///< Update path attr flags
        unsigned char type;                 ///< Attribute type code

        unsigned short len;                 ///< Length of attribute data in octets
        unsigned char *data;                ///< Attribute data - must be deleted
    };

    /**
     * Update header
     */
    struct update_bgp_hdr {
        /**
         * indicates the total len of withdrawn routes field in octets.
         */
        uint16_t withdrawn_len;

        /**
         * list of IP address prefixes for routes to be withdrawn.
         */
        vector<prefix_2tuple> *withdrawn_routes;

        /**
         * Total length of the path attributes field in octets
         *
         * A value of 0 indicates NLRI nor path attrs are present
         */
        uint16_t pathAttr_len;

        /**
         * list of path attributes - must be deleted
         */
        vector<update_path_attrs> *path_attrs;

        /**
         * NLRI is variable
         *  This variable length field contains a list of IP address
         *  prefixes.  The length, in octets, of the Network Layer
         *  Reachability Information is not encoded explicitly, but can be
         *  calculated as:
         *
         *      UPDATE message Length - 23 - Total Path Attributes Length
         *              - Withdrawn Routes Length
         *
         *  where UPDATE message Length is the value encoded in the fixed-
         *  size BGP header, Total Path Attribute Length, and Withdrawn
         *  Routes Length are the values encoded in the variable part of
         *  the UPDATE message, and 23 is a combined length of the fixed-
         *  size BGP header, the Total Path Attribute Length field, and the
         *  Withdrawn Routes Length field.
         */
        vector<prefix_2tuple> *nlri_routes;  // Network layer reachability information/routes - must be deleted

    } upd_hdr;


    /**
     * defines the NOTIFICATION BGP header per RFC4271
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum NOTIFY_ERROR_CODES { NOTIFY_MSG_HDR_ERR=1, NOTIFY_OPEN_MSG_ERR, NOTIFY_UPDATE_MSG_ERR,
                NOTIFY_HOLD_TIMER_EXPIRED, NOTIFY_FSM_ERR, NOTIFY_CEASE };


    /**
     * Defines header error codes
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum MSG_HDR_SUBCODES {
                MSG_HDR_CONN_NOT_SYNC=1,
                MSG_HDR_BAD_MSG_LEN,
                MSG_HDR_BAD_MSG_TYPE };

    /**
     * Defines open error codes
     *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
     */
    enum OPEN_SUBCODES {
                OPEN_UNSUPPORTED_VER=1,
                OPEN_BAD_PEER_AS,
                OPEN_BAD_BGP_ID,
                OPEN_UNSUPPORTED_OPT_PARAM,
                OPEN_code5_deprecated,
                OPEN_UNACCEPTABLE_HOLD_TIME };
     /**
      * Defines open error codes
      *  \see http://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml
      */
    enum UPDATE_SUBCODES {
                UPDATE_MALFORMED_ATTR_LIST=1,
                UPDATE_UNRECOGNIZED_WELL_KNOWN_ATTR,
                UPDATE_MISSING_WELL_KNOWN_ATTR,
                UPDATE_ATTR_FLAGS_ERROR,
                UPDATE_ATTR_LEN_ERROR,
                UPDATE_INVALID_NEXT_HOP_ATTR,
                UPDATE_OPT_ATTR_ERROR,
                UPDATE_INVALID_NET_FIELD,
                UPDATE_MALFORMED_AS_PATH };

    /**
     * Per RFC4486 - cease subcodes
     */
    enum CEASE_SUBCODES {
                CEASE_MAX_PREFIXES=1,
                CEASE_ADMIN_SHUT,
                CEASE_PEER_DECONFIG,
                CEASE_ADMIN_RESET,
                CEASE_CONN_REJECT,
                CEASE_OTHER_CONFIG_CHG,
                CEASE_CONN_COLLISION,
                CEASE_OUT_OF_RESOURCES
    };

    /**
     * Notification header
     */
    struct notify_bgp_hdr {
        /**
         * Indicates the type of error NOTIFY_ERROR_CODES enum for errors
         */
        unsigned char error_code;

        /**
         * specific info about the nature of the reported error values depend on the error code
         */
        unsigned char error_subcode;

        /**
         * The length of the Data field can be determined from
         * the message Length field by the formula:
         *
         *    Message Length = 21 + Data Length
         *
         */
        unsigned char *data;                    // reason for the notification; must be deleted
    };

    /**
     * Constructor for class -
     *
     * \details
     *    This class parses the BGP message and updates DB.  The
     *    'mysql_ptr' must be a pointer reference to an open mysql connection.
     *    'peer_entry' must be a pointer to the peer_entry table structure that
     *    has already been populated.
     *
     * \param [in]     logPtr      Pointer to existing Logger for app logging
     * \param [in]     dbi_ptr     Pointer to exiting dB implementation
     * \param [in,out] peer_entry  Pointer to peer entry
     */
    parseBGP(Logger *logPtr, DbInterface *dbi_ptr, DbInterface::tbl_bgp_peer *peer_entry);

    virtual ~parseBGP();


    /**
     * Handle the incoming BGP message directly from the open socket.
     *
     * \details
     *   This function will read and parse the BGP message from the socket.
     *
     * \param [in]     sock             Socket to read BGP message from
     */
    void handleMessage(int sock);

    /**
     * Same as handleMessage, except it enables the notify method to update the
     *   calling memory for the table data.
     *
     * \details
     *  The notify message does not directly add to Db, so the calling
     *  method/function must handle that.
     *
     * \param [in]     sock             Socket to read BGP message from
     * \param [in,out] peer_down_entry  Updated with details from notification msg
     */
    void handleMessage(int sock, DbInterface::tbl_peer_down_event *peer_down_entry);

    /*
     * Debug methods
     */
    void enableDebug();
    void disableDebug();


private:
    /**
     * bgp_bytes_remaining is a counter that starts at the message size and then is
     * decremented as the message is read.
     *
     * We can use this counter to jump to the end of the bgp message.
     */
    unsigned int bgp_bytes_remaining;

    DbInterface::tbl_bgp_peer        *p_entry;       ///< peer table entry - will be updated with BMP info
    DbInterface::tbl_peer_down_event *notify_tbl;    ///< pointer used for notify messages
    DbInterface                      *dbi_ptr;       ///< Pointer to open DB implementation

    unsigned char path_hash_id[16];                  ///< current path hash ID
    unsigned char peer_asn_len;                      ///< The PEER asn length in octets (either 2 or 4 (RFC4893))

    bool            debug;                           ///< debug flag to indicate debugging
    Logger          *log;                            ///< Logging class pointer

    /**
     * Parses the update message
     *
     * \details
     *      Reads the update message from socket and parses it.  The parsed output will
     *      be added to the DB.
     *
     * \param [in]     sock     Socket to read BGP message from
     */
    void parseUpdateMsg(int sock);

    /**
     * Parses a notification message
     *
     * \details
     *      Reads the notification message from the socket.  The parsed output
     *      will be added to the DB.
     *
     * \param [in]     sock     Socket to read BGP message from
     */
    void parseNotifyMsg(int sock);

    /**
     * Parses the BGP attributes in the update
     *
     * \details
     *      Reads the update message from the socket.  The parsed output
     *      will be added to the DB.
     *
     * \param [in]     sock     Socket to read BGP message from
     */
    void getPathAttr(int sock);

    /**
     * Parses NLRI info (IPv4) from the BGP message
     *
     * \details
     *      Will get the NLRI and WithDrawn routes prefix entries
     *      from the open socket data stream.  As per RFC, this is only for
     *       v4.  V6/mpls is via mpbgp attributes (RFC4760)
     *
     * \param [in]     sock     Socket to read BGP message from
     * \param [out]    plist    pointer to prefix_2tuple vector in upd_hdr
     * \param [in]     len      length of NLRI or withdrawn routes in field
     */
    void getPrefixes_v4(int sock, vector<prefix_2tuple> *plist, size_t len);


    /**
     * Parses mp_reach_nlri and mp_unreach_nlri
     *
     * \details
     *      Will get the MPBGP NLRI and WithDrawn routes prefix entries
     *      from the a data buffer. Specifically gets IPv6 prefixes.
     *
     * \param [out]    plist    pointer to prefix_2tuple vector in upd_hdr
     * \param [in]     len      length of NLRI or withdrawn routes in field
     * \param [in]     data     for MP_REACH/MP_UNREACH NLRI
     */
    void getPrefixes_v6(vector<prefix_2tuple> *plist, size_t len, unsigned char *data);

    /**
     * Process all path attributes and add them to the DB
     *
     * \details
     *      Parses and adds the path attributes to the DB.  This will call other
     *      methods to do the acutal reading/parsing.
     */
    void processPathAttrs();

    /**
     * Process the NLRI field in the BGP message
     *
     * \details
     *      Will handle the NLRI field by calling other methods to parse
     *      and add data to the DB.
     *
     * \param [in]  sock         Socket to read BGP message from
     * \param [in]  nlri_len     Length of the NRLI field
     */
    void processNLRI(int sock, unsigned short nlri_len);


    /**
     * Process the Withdrawn routes field in the BGP message
     *
     * \details
     *      Will handle the withdrawn field by calling other methods to parse
     *      and add data to the DB.
     *
     * \param [in]  sock         Socket to read BGP message from
     */
    void processWithdrawnRoutes(int sock);


    /**
     * Process the MP_REACH NLRI field data in the BGP message
     *
     * \details
     *      Will handle processing the mp_reach nlri data by calling other methods
     *      to parse the data so it can be added to the DB.
     *
     * \param [out]  nlri         Reference to storage where the NLRI data will be added
     * \param [in]   nlri_len     Length of the NLRI data
     * \param [in]   data         RAW mp_reach NLRI data buffer
     */
    void process_MP_REACH_NLRI(mp_reach_nlri &nlri, unsigned short nlri_len, unsigned char *data);


    /**
     * Add MP_REACH NLRI data to the DB
     *
     * \param [out]  nlri         Reference to the NLRI data will be added to the DB
     */
    void MP_REACH_NLRI_toDB(mp_reach_nlri &nlri);
};

#endif /* PARSEBGP_H_ */
