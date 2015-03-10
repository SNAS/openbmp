/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "NotificationMsg.h"
#include <cstring>

namespace bgp_msg {

/**
 * Constructor for class
 *
 * \details Handles bgp notification messages
 *
 * \param [in]     logPtr       Pointer to existing Logger for app logging
 * \param [in]     enable_debug Debug true to enable, false to disable
 */
NotificationMsg::NotificationMsg(Logger *logPtr, bool enable_debug) {
    logger = logPtr;
    debug = enable_debug;
}

NotificationMsg::~NotificationMsg() {

}

/**
 * Parses a notification message stored in a byte parsed_msg.error_textfer
 *
 * \details
 *      Reads the notification message from buffer.  The parsed data will be
 *      returned via parsed_msg.
 *
 * \param [in]      data        Pointer to raw bgp payload data, starting at the notification message
 * \param [in]      size        Size of the data available to read; prevent overrun when reading
 * \param [out]     parsed_msg  Reference pointer to where to store the parsed notification message
 *
 * \return True if error, false if no error reading/parsing the notification message
 */
bool NotificationMsg::parseNotify(u_char *data, size_t size, parsed_notify_msg &parsed_msg) {
    u_char *dataPtr = data;
    size_t read_size = 0;

    // Reset the storage buffer for parsed data
    bzero(&parsed_msg, sizeof(parsed_msg));

    if (read_size < size)
        parsed_msg.error_code = *dataPtr++, size++;
    else {
        LOG_ERR("Could not read the BGP error code from notify message");
        return true;
    }

    if (read_size < size)
        parsed_msg.error_subcode = *dataPtr++,size++;
    else {
        LOG_ERR("Could not read the BGP sub code from notify message");
        return true;
    }

    // Update the error text to be meaningful
    switch (parsed_msg.error_code) {
        case NOTIFY_MSG_HDR_ERR : {
            if (parsed_msg.error_subcode == MSG_HDR_BAD_MSG_LEN)
                snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Bad message header length");
            else
                snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Bad message header type");
            break;
        }

        case NOTIFY_OPEN_MSG_ERR : {
            switch (parsed_msg.error_subcode) {
                case OPEN_BAD_BGP_ID :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Bad BGP ID");
                    break;
                case OPEN_BAD_PEER_AS :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Incorrect peer AS");
                    break;
                case OPEN_UNACCEPTABLE_HOLD_TIME :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Unacceptable hold time");
                    break;
                case OPEN_UNSUPPORTED_OPT_PARAM :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Unsupported optional parameter");
                    break;
                case OPEN_UNSUPPORTED_VER :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Unsupported BGP version");
                    break;
                default :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Open message error - unknown subcode [%d]",
                            parsed_msg.error_subcode);
                    break;
            }
            break;
        }

        case NOTIFY_UPDATE_MSG_ERR : {
            switch (parsed_msg.error_subcode) {
                case UPDATE_ATTR_FLAGS_ERROR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Update attribute flags error");
                    break;
                case UPDATE_ATTR_LEN_ERROR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Update attribute lenght error");
                    break;
                case UPDATE_INVALID_NET_FIELD :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Invalid network field");
                    break;
                case UPDATE_INVALID_NEXT_HOP_ATTR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Invalid next hop address/attribute");
                    break;
                case UPDATE_MALFORMED_AS_PATH :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Malformed AS_PATH");
                    break;
                case UPDATE_MALFORMED_ATTR_LIST :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Malformed attribute list");
                    break;
                case UPDATE_MISSING_WELL_KNOWN_ATTR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Missing well known attribute");
                    break;
                case UPDATE_OPT_ATTR_ERROR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Update optional attribute error");
                    break;
                case UPDATE_UNRECOGNIZED_WELL_KNOWN_ATTR :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Unrecognized well known attribute");
                    break;
                default :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Update message error - unknown subcode [%d]",
                             parsed_msg.error_subcode);
                    break;
            }
            break;
        }

        case NOTIFY_HOLD_TIMER_EXPIRED : {
            snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Hold timer expired");
            break;
        }

        case NOTIFY_FSM_ERR : {
            snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "FSM error");
            break;
        }

        case NOTIFY_CEASE : {
            switch (parsed_msg.error_subcode) {
                case CEASE_MAX_PREFIXES :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Maximum number of prefixes reached");
                    break;
                case CEASE_ADMIN_SHUT :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Administrative shutdown");
                    break;
                case CEASE_PEER_DECONFIG :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Peer de-configured");
                    break;
                case CEASE_ADMIN_RESET :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Administratively reset");
                    break;
                case CEASE_CONN_REJECT :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Connection rejected");
                    break;
                case CEASE_OTHER_CONFIG_CHG :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Other configuration change");
                    break;
                case CEASE_CONN_COLLISION :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Connection collision resolution");
                    break;
                case CEASE_OUT_OF_RESOURCES :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Maximum number of prefixes reached");
                    break;
                default :
                    snprintf(parsed_msg.error_text, sizeof(parsed_msg.error_text), "Unknown cease code, subcode [%d]",
                                        parsed_msg.error_subcode);
                    break;
            }
            break;
        }

        default : {
            sprintf(parsed_msg.error_text, "Unknown notification type [%d]", parsed_msg.error_code);
            break;
        }
    }

    return false;
}


} /* namespace bgp_msg */
