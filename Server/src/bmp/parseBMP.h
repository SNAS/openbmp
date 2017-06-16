/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */


#ifndef PARSEBMP_H_
#define PARSEBMP_H_

#include "MsgBusInterface.hpp"
#include "Logger.h"


/*
 * BMP Header lengths, not counting the version in the common hdr
 */
#define BMP_HDRv3_LEN 5             ///< BMP v3 header length, not counting the version
#define BMP_HDRv1v2_LEN 43
#define BMP_PEER_HDR_LEN 42         ///< BMP peer header length
#define BMP_INFO_TLV_HDR_LEN 4          ///< BMP init message header length, does not count the info field
#define BMP_TERM_MSG_LEN 4          ///< BMP term message header length, does not count the info field
#define BMP_PEER_UP_HDR_LEN 20      ///< BMP peer up event header size not including the recv/sent open param message
#define BMP_PACKET_BUF_SIZE 68000   ///< Size of the BMP packet buffer (memory)

/**
 * \class   parseBMP
 *
 * \brief   Parser for BMP messages
 * \details This class can be used as needed to parse BMP messages. This
 *          class will read directly from the socket to read the BMP message.
 */
class parseBMP {
public:
    /**
     * BMP common header types
     */
     enum BMP_TYPE { TYPE_ROUTE_MON=0, TYPE_STATS_REPORT, TYPE_PEER_DOWN,
                    TYPE_PEER_UP, TYPE_INIT_MSG, TYPE_TERM_MSG };

     /**
      * BMP stats types
      */
     enum BMP_STATS { STATS_PREFIX_REJ=0, STATS_DUP_PREFIX, STATS_DUP_WITHDRAW, STATS_INVALID_CLUSTER_LIST,
                     STATS_INVALID_AS_PATH_LOOP, STATS_INVALID_ORIGINATOR_ID, STATS_INVALID_AS_CONFED_LOOP,
                     STATS_NUM_ROUTES_ADJ_RIB_IN, STATS_NUM_ROUTES_LOC_RIB };

     /**
      * BMP Peer Information TLV's
      */
     enum BMP_PEER_INFO_TYPES { INFO_TLV_PEER_VRF_TABLE=3 };

     /**
      * BMP Initiation Info TLV Message Types
      */
     enum BMP_INIT_TYPES { INIT_TYPE_FREE_FORM_STRING=0, INIT_TYPE_SYSDESCR, INIT_TYPE_SYSNAME,
                           INIT_TYPE_ROUTER_BGP_ID=65531 };

     /**
      * BMP Termination Message Types
      */
     enum BMP_TERM_TYPES { TERM_TYPE_FREE_FORM_STRING=0, TERM_TYPE_REASON };

     /**
      * BMP Termination Message reasons for type=1
      */
     enum BMP_TERM_TYPE1_REASON { TERM_REASON_ADMIN_CLOSE=0, TERM_REASON_UNSPECIFIED, TERM_REASON_OUT_OF_RESOURCES,
                     TERM_REASON_REDUNDANT_CONN,
                     TERM_REASON_OPENBMP_CONN_CLOSED=65533, TERM_REASON_OPENBMP_CONN_ERR=65534 };


     /**
      * BMP common header
      */
     struct common_hdr_v3 {
        // 4 bytes total for the common header
        //u_char      ver;                // 1 byte; BMP version -- Not part of struct since it's read before

        uint32_t    len;                ///< 4 bytes; BMP msg length in bytes including all headers

        /**
         * Type is defined by enum BMP_TYPE
         */
        u_char      type;

     } __attribute__ ((__packed__));

    /**
     * BMP peer header
     */
    struct peer_hdr_v3 {
        unsigned char peer_type;           ///< 1 byte
        unsigned char peer_flags;          ///< 1 byte

        unsigned char peer_dist_id[8];     ///< 8 byte peer route distinguisher
        unsigned char peer_addr[16];       ///< 16 bytes
        unsigned char peer_as[4];          ///< 4 byte
        unsigned char peer_bgp_id[4];      ///< 4 byte peer bgp id
        uint32_t      ts_secs;             ///< 4 byte timestamp in seconds
        uint32_t      ts_usecs;            ///< 4 byte timestamp microseconds

     } __attribute__ ((__packed__));


     /**
     * BMP Info TLV
     */
     struct info_tlv_msg {
         uint16_t        type;              ///< 2 bytes - Information type
         uint16_t        len;               ///< 2 bytes - Length of the information that follows

         char           *info;              ///< Information - variable

     } __attribute__ ((__packed__));


     /**
      * BMP termination message
      */
     struct term_msg_v3 {
         uint16_t        type;              ///< 2 bytes - Information type
         uint16_t        len;               ///< 2 bytes - Length of the information that follows

         char           *info;              ///< Information - variable

     } __attribute__ ((__packed__));

    /**
    *  BMP headers for older versions (BMPv1)
    */
    struct common_hdr_old {
        //unsigned char ver;               // 1 byte -- Not part of struct since it's read before
        unsigned char type;                // 1 byte
        unsigned char peer_type;           // 1 byte
        unsigned char peer_flags;          // 1 byte

        unsigned char peer_dist_id[8];     // 8 byte peer distinguisher
        unsigned char peer_addr[16];       // 16 bytes
        unsigned char peer_as[4];          // 4 byte
        unsigned char peer_bgp_id[4];      // 4 byte peer bgp id
        unsigned long ts_secs : 32;        // 4 byte timestamp in seconds
        unsigned long ts_usecs : 32;       // 4 byte timestamp microseconds
    } __attribute__ ((__packed__));


    /**
     * BMP message buffer (normally only contains the BGP message)
     *      BMP data message is read into this buffer so that it can be passed to the BGP parser for handling.
     *      Complete BGP message is read, otherwise error is generated.
     */
    u_char      bmp_data[BMP_PACKET_BUF_SIZE + 1];
    size_t      bmp_data_len;              ///< Length/size of data in the data buffer

    /**
     * BMP packet buffer - This is a copy of the BMP packet.
     *
     * Only BMPv3 messages get stored in the packet buffer since it wasn't until
     * BMPv3 that the length was specified.
     *
     * Length of packet is the common header message length (bytes)
     */
    u_char      bmp_packet[BMP_PACKET_BUF_SIZE + 1];
    size_t      bmp_packet_len;

    /**
     * Constructor for class
     *
     * \note
     *  This class will allocate via 'new' the bgp_peers variables
     *        as needed.  The calling method/class/function should check each var
     *        in the structure for non-NULL pointers.  Non-NULL pointers need to be
     *        freed with 'delete'
     *
     * \param [in]     logPtr      Pointer to existing Logger for app logging
     * \param [in,out] peer_entry  Pointer to the peer entry
     */
    parseBMP(Logger *logPtr, MsgBusInterface::obj_bgp_peer *peer_entry);

    // destructor
    virtual ~parseBMP();

    /**
     * Recv wrapper for recv() to enable packet buffering
     */
    ssize_t Recv(int sockfd, void *buf, size_t len, int flags);

    /**
     * Process the incoming BMP message
     *
     * \returns
     *      returns the BMP message type. A type of >= 0 is normal,
     *      < 0 indicates an error
     *
     * \param [in] sock     Socket to read the BMP message from
     *
     * \throws (const char *) on error.   String will detail error message.
     */
    char handleMessage(int sock);

    /**
     * Parse and return back the stats report
     *
     * \param [in]  sock        Socket to read the stats message from
     * \param [out] stats       Reference to stats report data
     *
     * \return true if error, false if no error
     */
    bool handleStatsReport(int sock, MsgBusInterface::obj_stats_report &stats);

    /**
     * handle the initiation message and udpate the router entry
     *
     * \param [in]     sock        Socket to read the init message from
     * \param [in/out] r_entry     Already defined router entry reference (will be updated)
     */
    void handleInitMsg(int sock, MsgBusInterface::obj_router &r_entry);

    /**
     * handle the termination message, router entry will be updated
     *
     * \param [in]     sock        Socket to read the term message from
     * \param [in/out] r_entry     Already defined router entry reference (will be updated)
     */
    void handleTermMsg(int sock, MsgBusInterface::obj_router &r_entry);
    /**
     * Buffer remaining BMP message
     *
     * \details This method will read the remaining amount of BMP data and store it in the instance variable bmp_data.
     *          Normally this is used to store the BGP message so that it can be parsed.
     *
     * \param [in]  sock       Socket to read the message from
     *
     * \returns true if successfully parsed the bmp peer down header, false otherwise
     */
    void bufferBMPMessage(int sock);

    /**
     * Parse the v3 peer down BMP header
     *
     *      This method will update the db peer_down_event struct with BMP header info.
     *
     * \param [in]  sock       Socket to read the message from
     * \param [out] down_event Reference to the peer down event storage (will be updated with bmp info)
     *
     * \returns true if successfully parsed the bmp peer down header, false otherwise
     */
    bool parsePeerDownEventHdr(int sock, MsgBusInterface::obj_peer_down_event &down_event);

    /**
     * Parse the v3 peer up BMP header
     *
     *      This method will update the db peer_up_event struct with BMP header info.
     *
     * \param [in]  sock     Socket to read the message from
     * \param [out] up_event Reference to the peer up event storage (will be updated with bmp info)
     *
     * \returns true if successfully parsed the bmp peer up header, false otherwise
     */
    bool parsePeerUpEventHdr(int sock, MsgBusInterface::obj_peer_up_event &up_event);

    /**
     * get current BMP message type
     */
    char getBMPType();

    /**
     * get current BMP message length
     *
     * The length returned does not include the version 3 common header length
     */
    uint32_t getBMPLength();

    /**
     * Parse the peer UP informational data
     *
     * \details Updates the peer struct info items
     *
     * \param [in] data         Data pointer to info, should start at start of info TLV
     * \param [in] len          Length of info TLV data to read
     */
    void parsePeerUpInfo(u_char *data, int len);

    // Debug methods
    void enableDebug();
    void disableDebug();

private:
    bool            debug;                      ///< debug flag to indicate debugging
    Logger          *logger;                    ///< Logging class pointer

    MsgBusInterface::obj_bgp_peer *p_entry;         ///< peer table entry - will be updated with BMP info
    char            bmp_type;                   ///< The BMP message type
    uint32_t        bmp_len;                    ///< Length of the BMP message - does not include the common header size

    // Storage for the byte converted strings - This must match the MsgBusInterface bgp_peer struct
    char peer_addr[40];                         ///< Printed format of the peer address (Ipv4 and Ipv6)
    char peer_as[32];                           ///< Printed format of the peer ASN
    char peer_rd[32];                           ///< Printed format of the peer RD
    char peer_bgp_id[16];                       ///< Printed format of the peer bgp ID

    /**
     * Parse v1 and v2 BMP header
     *
     * \details
     *      v2 uses the same common header, but adds the Peer Up message type.
     *
     * \param [in]  sock        Socket to read the message from
     */
    void parseBMPv2(int sock);

    /**
     * Parse v3 BMP header
     *
     * \details
     *      v3 has a different header structure and changes the peer
     *      header format.
     *
     * \param [in]  sock        Socket to read the message from
     */
    void parseBMPv3(int sock);


    /**
     * Parse the v3 peer header
     *
     * \param [in]  sock        Socket to read the message from
     */
    void parsePeerHdr(int sock);

    /**
     * Parse BMP peer header flags by peer type
     *
     *  This method will update the instance variable "p_entry"
     *
     * \param [in] peer_type         peer type from peer header
     * \param [in] peer_flags        flags from peer header
     *
     */
    void parsePeerFlags(u_char peer_type, u_char peer_flags);

};

#endif /* PARSEBMP_H_ */
