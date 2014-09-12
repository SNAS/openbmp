/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef NOTIFICATIONMSG_H_
#define NOTIFICATIONMSG_H_

#include "Logger.h"
#include "bgp_common.h"

namespace bgp_msg {

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
      * BGP notification header (BGP raw message)
      */
     struct notify_bgp_hdr {
         u_char       error_code;                ///< Indicates the type of error
                                                 ///<   NOTIFY_ERROR_CODES enum for errors
         u_char       error_subcode;             ///< specific info about the nature of the reported error
                                                 ///<   values depend on the error code
         /**
          * The length of the Data field can be determined from
          * the message Length field by the following:
          *
          *    Message Length = 19(common hdr) + 2(notify hdr) + Data Length
          *
          */
     } __attribute__((__packed__));

     /**
      * Decoded/parsed BGP notification message
      */
     struct parsed_notify_msg {
         u_char       error_code;                ///< Indicates the type of error
                                                 ///<   NOTIFY_ERROR_CODES enum for errors
         u_char       error_subcode;             ///< specific info about the nature of the reported error
                                                 ///<   values depend on the error code
         char         error_text[255];           ///< Decoded notification message
     };

/**
 * \class   NotificationMsg
 *
 * \brief   BGP notification message parser
 * \details This class parses a BGP notification message.  It can be extended to create messages.
 *          message.
 */
class NotificationMsg {
public:
    /**
     * Constructor for class
     *
     * \details Handles bgp notification messages
     *
     */
    NotificationMsg(Logger *logPtr,bool enable_debug=false);
    virtual ~NotificationMsg();

    /**
     * Parses a notification message stored in a byte buffer
     *
     * \details
     *      Reads the notification message from buffer.  The parsed data will be
     *      returned via parsed_msg.
     *
     * \param [in]      data        Pointer to raw bgp payload data, starting at the notification message
     * \param [in]      size        Size of the data buffer, to prevent overrun when reading
     * \param [out]     parsed_msg  Reference pointer to where to store the parsed notification message
     *
     * \return True if error, false if no error reading/parsing the notification message
     */
    bool parseNotify(u_char *data, size_t size, parsed_notify_msg &parsed_msg);

private:
    bool             debug;                           ///< debug flag to indicate debugging
    Logger           *logger;                         ///< Logging class pointer

};

} /* namespace bgp_msg */

#endif /* NOTIFICATIONMSG_H_ */
