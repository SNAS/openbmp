/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include <arpa/inet.h>

#include "MPLinkState.h"
#include "md5.h"

namespace bgp_msg {
    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     peerAddr     Printed form of peer address used for logging
     * \param [out]    parsed_data  Reference to parsed_update_data; will be updated with all parsed data
     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    MPLinkState::MPLinkState(Logger *logPtr, std::string peerAddr,
                             UpdateMsg::parsed_update_data *parsed_data, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        peer_addr = peerAddr;
        this->parsed_data = parsed_data;
    }

    MPLinkState::~MPLinkState() {
    }

    /**
     * MP Reach Link State NLRI parse
     *
     * \details Will handle parsing the link state NLRI
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     */
    void MPLinkState::parseReachLinkState(MPReachAttr::mp_reach_nlri &nlri) {
        ls_data = &parsed_data->ls;

        // Process the next hop
        // Next-hop is an IPv6 address - Change/set the next-hop attribute in parsed data to use this next-hop
        u_char ip_raw[16];
        char ip_char[40];

        if (nlri.nh_len == 4) {
            memcpy(ip_raw, nlri.next_hop, nlri.nh_len);
            inet_ntop(AF_INET, ip_raw, ip_char, sizeof(ip_char));
        } else if (nlri.nh_len > 4) {
            memcpy(ip_raw, nlri.next_hop, nlri.nh_len);
            inet_ntop(AF_INET6, ip_raw, ip_char, sizeof(ip_char));
        }

        parsed_data->attrs[ATTR_TYPE_NEXT_HOP] = std::string(ip_char);

        /*
         * Decode based on SAFI
         */
        switch (nlri.safi) {
            case bgp::BGP_SAFI_BGPLS: // Unicast BGP-LS
                SELF_DEBUG("REACH: bgp-ls: len=%d", nlri.nlri_len);
                parseLinkStateNlriData(nlri.nlri_data, nlri.nlri_len);
                break;

            default :
                LOG_INFO("%s: MP_UNREACH AFI=bgp-ls SAFI=%d is not implemented yet, skipping for now",
                        peer_addr.c_str(), nlri.afi, nlri.safi);
                return;
        }
    }


    /**
     * MP UnReach Link State NLRI parse
     *
     * \details Will handle parsing the unreach link state NLRI
     *
     * \param [in]   nlri           Reference to parsed NLRI struct
     */
    void MPLinkState::parseUnReachLinkState(MPUnReachAttr::mp_unreach_nlri &nlri) {
        ls_data = &parsed_data->ls_withdrawn;

        /*
         * Decode based on SAFI
         */
        switch (nlri.safi) {
            case bgp::BGP_SAFI_BGPLS: // Unicast BGP-LS
                SELF_DEBUG("UNREACH: bgp-ls: len=%d", nlri.nlri_len);
                parseLinkStateNlriData(nlri.nlri_data, nlri.nlri_len);
                break;

            default :
                LOG_INFO("%s: MP_UNREACH AFI=bgp-ls SAFI=%d is not implemented yet, skipping for now",
                        peer_addr.c_str(), nlri.afi, nlri.safi);
                return;
        }
    }

    /**********************************************************************************//*
     * Parses Link State NLRI data
     *
     * \details Will parse the link state NLRI's from MP_REACH or MP_UNREACH.
     *
     * \param [in]   data           Pointer to the NLRI data
     * \param [in]   len            Length of the NLRI data
     */
    void MPLinkState::parseLinkStateNlriData(u_char *data, uint16_t len) {
        uint16_t        nlri_type;
        uint16_t        nlri_len;
        uint16_t        nlri_len_read = 0;

        // Process the NLRI data
        while (nlri_len_read < len) {

            SELF_DEBUG("NLRI read=%d total = %d", nlri_len_read, len);

            /*
             * Parse the NLRI TLV
             */
            memcpy(&nlri_type, data, 2);
            data += 2;
            bgp::SWAP_BYTES(&nlri_type);

            memcpy(&nlri_len, data, 2);
            data += 2;
            bgp::SWAP_BYTES(&nlri_len);

            nlri_len_read += 4;

            if (nlri_len > len) {
                LOG_NOTICE("%s: bgp-ls: failed to parse link state NLRI; length is larger than available data",
                        peer_addr.c_str());
                return;
            }

            /*
             * Parse out the protocol and ID (present in each NLRI ypte
             */
            uint8_t          proto_id;
            uint64_t         id;

            proto_id = *data;
            memcpy(&id, data + 1, sizeof(id));
            bgp::SWAP_BYTES(&id);

            // Update read NLRI attribute, current TLV length and data pointer
            nlri_len_read += 9; nlri_len -= 9; data += 9;

            /*
             * Decode based on bgp-ls NLRI type
             */
            switch (nlri_type) {
                case NLRI_TYPE_NODE:
                    SELF_DEBUG("%s: bgp-ls: parsing NODE NLRI len=%d", peer_addr.c_str(), nlri_len);
                    parseNlriNode(data, nlri_len, id, proto_id);
                    break;

                case NLRI_TYPE_LINK:
                    SELF_DEBUG("%s: bgp-ls: parsing LINK NLRI", peer_addr.c_str());
                    parseNlriLink(data, nlri_len, id, proto_id);
                    break;

                case NLRI_TYPE_IPV4_PREFIX:
                    SELF_DEBUG("%s: bgp-ls: parsing IPv4 PREFIX NLRI", peer_addr.c_str());
                    parseNlriPrefix(data, nlri_len, id, proto_id, true);
                    break;

                case NLRI_TYPE_IPV6_PREFIX:
                    SELF_DEBUG("%s: bgp-ls: parsing IPv6 PREFIX NLRI", peer_addr.c_str());
                    parseNlriPrefix(data, nlri_len, id, proto_id, false);
                    break;

                default :
                    LOG_INFO("%s: bgp-ls NLRI Type %d is not implemented yet, skipping for now",
                            peer_addr.c_str(), nlri_type);
                    return;
            }

            // Move to next link state type
            data += nlri_len;
            nlri_len_read += nlri_len;
        }
    }

    /**********************************************************************************//*
     * Decode Protocol ID
     *
     * \details will decode and return string representation of protocol (matches DB enum)
     *
     * \param [in]   proto_id       NLRI protocol type id
     *
     * \return string representation for the protocol that matches the DB enum string value
     *          empty will be returned if invalid/unknown.
     */
    std::string MPLinkState::decodeNlriProtocolId(uint8_t proto_id) {
        std::string value = "";

        switch (proto_id) {
            case NLRI_PROTO_DIRECT:
                value = "Direct";
                break;

            case NLRI_PROTO_STATIC:
                value = "Static";
                break;

            case NLRI_PROTO_ISIS_L1:
                value = "IS-IS_L1";
                break;

            case NLRI_PROTO_ISIS_L2:
                value = "IS-IS_L2";
                break;

            case NLRI_PROTO_OSPFV2:
                value = "OSPFv2";
                break;

            case NLRI_PROTO_OSPFV3:
                value = "OSPFv3";
                break;

            default:
                break;
        }

        return value;
    }

    /**********************************************************************************//*
     * Parse NODE NLRI
     *
     * \details will parse the node NLRI type. Data starts at local node descriptor.
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [in]   id             NLRI/type identifier
     * \param [in]   proto_id       NLRI protocol type id
     */
    void MPLinkState::parseNlriNode(u_char *data, int data_len, uint64_t id, uint8_t proto_id) {
        MsgBusInterface::obj_ls_node node_tbl;
        bzero(&node_tbl, sizeof(node_tbl));

        if (data_len < 4) {
            LOG_WARN("%s: bgp-ls: Unable to parse node NLRI since it's too short (invalid)", peer_addr.c_str());
            return;
        }

        node_tbl.id       = id;
        snprintf(node_tbl.protocol, sizeof(node_tbl.protocol), "%s", decodeNlriProtocolId(proto_id).c_str());

        SELF_DEBUG("%s: bgp-ls: ID = %x Protocol = %s", peer_addr.c_str(), id, node_tbl.protocol);

        /*
         * Parse the local node descriptor sub-tlv
         */
        node_descriptor info;
        bzero(&info, sizeof(info));

        uint16_t type;
        uint16_t len;

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);
        memcpy(&len, data + 2, 2);
        bgp::SWAP_BYTES(&len);
        data_len -= 4;
        data += 4;

        if (len > data_len) {
            LOG_WARN("%s: bgp-ls: failed to parse node descriptor; type length is larger than available data %d>=%d",
                    peer_addr.c_str(), len, data_len);
            return;
        }

        if (type != NODE_DESCR_LOCAL_DESCR) {
            LOG_WARN("%s: bgp-ls: failed to parse node descriptor; Type (%d) is not local descriptor",
                    peer_addr.c_str(), type);
            return;
        }

        // Parse the local descriptor sub-tlv's
        int data_read;
        while (len > 0) {
            data_read = parseDescrLocalRemoteNode(data, len, info);
            len -= data_read;

            // Update the nlri data pointer and remaining length after processing the local descriptor sub-tlv
            data += data_read;
            data_len -= data_read;
        }

        genNodeHashId(info);


        // Update node table entry and add to parsed data list
        node_tbl.isIPv4 = true;
        memcpy(node_tbl.hash_id, info.hash_bin, sizeof(node_tbl.hash_id));
        node_tbl.asn = info.asn;
        memcpy(node_tbl.ospf_area_Id, info.ospf_area_Id, sizeof(node_tbl.ospf_area_Id));
        node_tbl.bgp_ls_id = info.bgp_ls_id;
        memcpy(node_tbl.igp_router_id, info.igp_router_id, sizeof(node_tbl.igp_router_id));

        // Save the parsed data
        ls_data->nodes.push_back(node_tbl);
    }

    /**********************************************************************************//*
     * Parse LINK NLRI
     *
     * \details will parse the LINK NLRI type.  Data starts at local node descriptor.
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [in]   id             NLRI/type identifier
     * \param [in]   proto_id       NLRI protocol type id
     */
    void MPLinkState::parseNlriLink(u_char *data, int data_len, uint64_t id, uint8_t proto_id) {
        MsgBusInterface::obj_ls_link link_tbl;
        bzero(&link_tbl, sizeof(link_tbl));

        if (data_len < 4) {
            LOG_WARN("%s: bgp-ls: Unable to parse link NLRI since it's too short (invalid)", peer_addr.c_str());
            return;
        }

        link_tbl.id       = id;
        snprintf(link_tbl.protocol, sizeof(link_tbl.protocol), "%s", decodeNlriProtocolId(proto_id).c_str());

        SELF_DEBUG("%s: bgp-ls: ID = %x Protocol = %s", peer_addr.c_str(), id, link_tbl.protocol);

        /*
         * Parse local and remote node descriptors (expect both)
         */
        uint16_t type;
        uint16_t len;

        for (char i = 0; i < 2; i++) {
            node_descriptor info;
            bzero(&info, sizeof(info));

            memcpy(&type, data, 2);
            bgp::SWAP_BYTES(&type);
            memcpy(&len, data + 2, 2);
            bgp::SWAP_BYTES(&len);
            data_len -= 4;
            data += 4;

            if (len > data_len) {
                LOG_WARN("%s: bgp-ls: failed to parse node descriptor; type length is larger than available data %d>=%d",
                        peer_addr.c_str(), len, data_len);
                return;
            }

            // Parse the local descriptor sub-tlv's
            int data_read;
            while (len > 0) {
                data_read = parseDescrLocalRemoteNode(data, len, info);
                len -= data_read;

                // Update the nlri data pointer and remaining length after processing the local descriptor sub-tlv
                data += data_read;
                data_len -= data_read;
            }

            genNodeHashId(info);

            switch (type) {
                case NODE_DESCR_LOCAL_DESCR:
                    memcpy(link_tbl.local_node_hash_id, info.hash_bin, sizeof(link_tbl.local_node_hash_id));
                    memcpy(link_tbl.ospf_area_Id, info.ospf_area_Id, sizeof(link_tbl.ospf_area_Id));
                    link_tbl.bgp_ls_id = info.bgp_ls_id;
                    memcpy(link_tbl.igp_router_id, info.igp_router_id, sizeof(link_tbl.igp_router_id));
                    break;

                case NODE_DESCR_REMOTE_DESCR:
                    memcpy(link_tbl.remote_node_hash_id, info.hash_bin, sizeof(link_tbl.remote_node_hash_id));
                    break;

                default:
                    LOG_WARN("%s: bgp-ls: failed to parse node descriptor; Type (%d) is not local descriptor",
                            peer_addr.c_str(), type);
                     break;
            }
        }

        /*
         * Remaining data is the link descriptor sub-tlv's
         */
        int data_read;
        link_descriptor info;
        bzero(&info, sizeof(info));
        while (data_len > 0) {
            data_read = parseDescrLink(data, data_len, info);

            // Update the nlri data pointer and remaining length after processing the local descriptor sub-tlv
            data += data_read;
            data_len -= data_read;
        }

        // Save link to parsed data
        SELF_DEBUG("MT-ID = %u/%u", link_tbl.mt_id, info.mt_id);
        link_tbl.isIPv4             = info.isIPv4;
        link_tbl.mt_id              = info.mt_id;
        link_tbl.local_link_id      = info.local_id;
        link_tbl.remote_link_id     = info.remote_id;
        memcpy(link_tbl.intf_addr, info.intf_addr, sizeof(link_tbl.intf_addr));
        memcpy(link_tbl.nei_addr, info.nei_addr, sizeof(link_tbl.nei_addr));

        ls_data->links.push_back(link_tbl);
    }

    /**********************************************************************************//*
     * Parse PREFIX NLRI
     *
     * \details will parse the PREFIX NLRI type.  Data starts at local node descriptor.
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [in]   id             NLRI/type identifier
     * \param [in]   proto_id       NLRI protocol type id
     * \param [in]   isIPv4         Bool value to indicate IPv4(true) or IPv6(false)
     */
    void MPLinkState::parseNlriPrefix(u_char *data, int data_len, uint64_t id, uint8_t proto_id, bool isIPv4) {
        MsgBusInterface::obj_ls_prefix prefix_tbl;
        bzero(&prefix_tbl, sizeof(prefix_tbl));

        if (data_len < 4) {
            LOG_WARN("%s: bgp-ls: Unable to parse prefix NLRI since it's too short (invalid)", peer_addr.c_str());
            return;
        }

        prefix_tbl.id       = id;
        snprintf(prefix_tbl.protocol, sizeof(prefix_tbl.protocol), "%s", decodeNlriProtocolId(proto_id).c_str());

        SELF_DEBUG("%s: bgp-ls: ID = %x Protocol = %s", peer_addr.c_str(), id, prefix_tbl.protocol);

        /*
         * Parse the local node descriptor sub-tlv
         */
        node_descriptor local_node;
        bzero(&local_node, sizeof(local_node));

        uint16_t type;
        uint16_t len;

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);
        memcpy(&len, data + 2, 2);
        bgp::SWAP_BYTES(&len);
        data_len -= 4;
        data += 4;

        if (len > data_len) {
            LOG_WARN("%s: bgp-ls: failed to parse node descriptor; type length is larger than available data %d>=%d",
                    peer_addr.c_str(), len, data_len);
            return;
        }

        if (type != NODE_DESCR_LOCAL_DESCR) {
            LOG_WARN("%s: bgp-ls: failed to parse node descriptor; Type (%d) is not local descriptor",
                    peer_addr.c_str(), type);
            return;
        }

        // Parse the local descriptor sub-tlv's
        int data_read;
        while (len > 0) {
            data_read = parseDescrLocalRemoteNode(data, len, local_node);
            len -= data_read;

            // Update the nlri data pointer and remaining length after processing the local descriptor sub-tlv
            data += data_read;
            data_len -= data_read;
        }

        // Update the node hash
        genNodeHashId(local_node);

        /*
         * Remaining data is the link descriptor sub-tlv's
         */
        prefix_descriptor info;
        bzero(&info, sizeof(info));
        while (data_len > 0) {
            data_read = parseDescrPrefix(data, data_len, info, isIPv4);

            // Update the nlri data pointer and remaining length after processing the local descriptor sub-tlv
            data += data_read;
            data_len -= data_read;
        }

        // Save prefix to parsed data
        prefix_tbl.isIPv4       = isIPv4;
        prefix_tbl.prefix_len   = info.prefix_len;
        prefix_tbl.mt_id        = info.mt_id;

        memcpy(prefix_tbl.ospf_area_Id, local_node.ospf_area_Id, sizeof(prefix_tbl.ospf_area_Id));
        prefix_tbl.bgp_ls_id = local_node.bgp_ls_id;
        memcpy(prefix_tbl.igp_router_id, local_node.igp_router_id, sizeof(prefix_tbl.igp_router_id));
        memcpy(prefix_tbl.local_node_hash_id, local_node.hash_bin, sizeof(prefix_tbl.local_node_hash_id));
        memcpy(prefix_tbl.prefix_bin, info.prefix, sizeof(prefix_tbl.prefix_bin));
        memcpy(prefix_tbl.prefix_bcast_bin, info.prefix_bcast, sizeof(prefix_tbl.prefix_bcast_bin));
        memcpy(prefix_tbl.ospf_route_type, info.ospf_route_type, sizeof(prefix_tbl.ospf_route_type));

        ls_data->prefixes.push_back(prefix_tbl);
    }

    /**********************************************************************************//*
     * Parse a Local or Remote Descriptor sub-tlv's
     *
     * \details will parse a local/remote descriptor
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [out]  info           Node descriptor information returned/updated
     *
     * \returns number of bytes read
     */
    int MPLinkState::parseDescrLocalRemoteNode(u_char *data, int data_len, node_descriptor &info) {
        uint16_t        type;
        uint16_t        len;
        int             data_read = 0;

        if (data_len < 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor; too short",
                        peer_addr.c_str());
            return data_len;
        }

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);

        memcpy(&len, data+2, 2);
        bgp::SWAP_BYTES(&len);

        //SELF_DEBUG("%s: bgp-ls: Parsing node descriptor type %d len %d", peer_addr.c_str(), type, len);

        if (len > data_len - 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor; type length is larger than available data %d>=%d",
                        peer_addr.c_str(), len, data_len);
            return data_len;
        }

        data += 4; data_read += 4;

        switch (type) {
            case NODE_DESCR_AS:
            {
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor AS sub-tlv; too short",
                                peer_addr.c_str());
                    data_read += len;
                    break;
                }

                memcpy(&info.asn, data, 4);
                bgp::SWAP_BYTES(&info.asn);
                data_read += 4;

                SELF_DEBUG("%s: bgp-ls: Node descriptior AS = %u", peer_addr.c_str(), info.asn);

                break;
            }

            case NODE_DESCR_BGP_LS_ID:
            {
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor BGP-LS ID sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len;
                    break;
                }


                memcpy(&info.bgp_ls_id, data, 4);
                bgp::SWAP_BYTES(&info.bgp_ls_id);
                data_read += 4;

                SELF_DEBUG("%s: bgp-ls: Node descriptior BGP-LS ID = %08X", peer_addr.c_str(), info.bgp_ls_id);
                break;
            }

            case NODE_DESCR_OSPF_AREA_ID:
            {
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor OSPF Area ID sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len <= data_len ? len : data_len;
                    break;
                }


                char    ipv4_char[16];
                memcpy(info.ospf_area_Id, data, 4);
                inet_ntop(AF_INET, info.ospf_area_Id, ipv4_char, sizeof(ipv4_char));
                data_read += 4;

                SELF_DEBUG("%s: bgp-ls: Node descriptior OSPF Area ID = %s", peer_addr.c_str(), ipv4_char);
                break;
            }

            case NODE_DESCR_IGP_ROUTER_ID:
            {
                if (len > data_len or len > 8) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse node descriptor IGP Router ID sub-tlv; len (%d) is invalid",
                            peer_addr.c_str(), len);
                    data_read += len;
                    break;
                }

                bzero(info.igp_router_id, sizeof(info.igp_router_id));
                memcpy(info.igp_router_id, data, len);
                data_read += len;

                SELF_DEBUG("%s: bgp-ls: Node descriptior IGP Router ID %d = %d.%d.%d.%d (%02x%02x.%02x%02x.%02x%02x.%02x %02x)", peer_addr.c_str(), data_read,
                            info.igp_router_id[0], info.igp_router_id[1], info.igp_router_id[2], info.igp_router_id[3],
                        info.igp_router_id[0], info.igp_router_id[1], info.igp_router_id[2], info.igp_router_id[3],
                        info.igp_router_id[4], info.igp_router_id[5], info.igp_router_id[6], info.igp_router_id[7]);
                break;
            }

            default:
                LOG_NOTICE("%s: bgp-ls: Failed to parse node descriptor; invalid sub-tlv type of %d",
                            peer_addr.c_str(), type);
                data_read += len;
                break;
        }

        return data_read;
    }

    /**********************************************************************************//*
     * Parse Link Descriptor sub-tlvs
     *
     * \details will parse a link descriptor (series of sub-tlv's)
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [out]  info           link descriptor information returned/updated
     *
     * \returns number of bytes read
     */
    int MPLinkState::parseDescrLink(u_char *data, int data_len, link_descriptor &info) {
        uint16_t        type;
        uint16_t        len;
        int             data_read = 0;

        if (data_len < 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor; too short",
                    peer_addr.c_str());
            return data_len;
        }

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);

        memcpy(&len, data+2, 2);
        bgp::SWAP_BYTES(&len);

        if (len > data_len - 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor; type length is larger than available data %d>=%d",
                    peer_addr.c_str(), len, data_len);
            return data_len;
        }

        data += 4; data_read += 4;

        switch (type) {
            case LINK_DESCR_ID:
            {
                if (len != 8) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link ID descriptor sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len;
                    break;
                }

                memcpy(&info.local_id, data, 4); bgp::SWAP_BYTES(&info.local_id);
                memcpy(&info.remote_id, data, 4); bgp::SWAP_BYTES(&info.remote_id);
                data_read += 8;

                SELF_DEBUG("%s: bgp-ls: Link descriptior ID local = %08x remote = %08x", peer_addr.c_str(), info.local_id, info.remote_id);

                break;
            }

            case LINK_DESCR_MT_ID:
            {
                if (len < 2) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link MT-ID descriptor sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len;
                    break;
                }

                if (len > 4) {
                    SELF_DEBUG("%s: bgp-ls: failed to parse link MT-ID descriptor sub-tlv; too long %d",
                               peer_addr.c_str(), len);
                    info.mt_id = 0;
                    data_read += len;
                    break;
                }

                memcpy(&info.mt_id, data, len); bgp::SWAP_BYTES(&info.mt_id);
                data_read += len;

                SELF_DEBUG("%s: bgp-ls: Link descriptior MT-ID = %08x ", peer_addr.c_str(), info.mt_id);

                break;
            }


            case LINK_DESCR_IPV4_INTF_ADDR:
            {
                info.isIPv4 = true;
                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor interface IPv4 sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len <= data_len ? len : data_len;
                    break;
                }

                char ip_char[46];
                memcpy(info.intf_addr, data, 4);
                inet_ntop(AF_INET, info.intf_addr, ip_char, sizeof(ip_char));
                data_read += 4;

                SELF_DEBUG("%s: bgp-ls: Link descriptior Interface Address = %s", peer_addr.c_str(), ip_char);
                break;
            }

            case LINK_DESCR_IPV6_INTF_ADDR:
            {
                info.isIPv4 = false;
                if (len != 16) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor interface IPv6 sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len <= data_len ? len : data_len;
                    break;
                }

                char ip_char[46];
                memcpy(info.intf_addr, data, 16);
                inet_ntop(AF_INET6, info.intf_addr, ip_char, sizeof(ip_char));
                data_read += 16;

                SELF_DEBUG("%s: bgp-ls: Link descriptior interface address = %s", peer_addr.c_str(), ip_char);
                break;
            }

            case LINK_DESCR_IPV4_NEI_ADDR:
            {
                info.isIPv4 = true;

                if (len != 4) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor neighbor IPv4 sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len <= data_len ? len : data_len;
                    break;
                }

                char ip_char[46];
                memcpy(info.nei_addr, data, 4);
                inet_ntop(AF_INET, info.nei_addr, ip_char, sizeof(ip_char));
                data_read += 4;

                SELF_DEBUG("%s: bgp-ls: Link descriptior neighbor address = %s", peer_addr.c_str(), ip_char);
                break;
            }

            case LINK_DESCR_IPV6_NEI_ADDR:
            {
                info.isIPv4 = false;
                if (len != 16) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor neighbor IPv6 sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len <= data_len ? len : data_len;
                    break;
                }

                char ip_char[46];
                memcpy(info.nei_addr, data, 16);
                inet_ntop(AF_INET6, info.nei_addr, ip_char, sizeof(ip_char));
                data_read += 16;

                SELF_DEBUG("%s: bgp-ls: Link descriptior neighbor address = %s", peer_addr.c_str(), ip_char);
                break;
            }


            default:
                LOG_NOTICE("%s: bgp-ls: Failed to parse node descriptor; invalid sub-tlv type of %d",
                        peer_addr.c_str(), type);
                data_read += len;
                break;
        }

        return data_read;
    }

    /**********************************************************************************//*
     * Parse Prefix Descriptor sub-tlvs
     *
     * \details will parse a prefix descriptor (series of sub-tlv's)
     *
     * \param [in]   data           Pointer to the start of the node NLRI data
     * \param [in]   data_len       Length of the data
     * \param [out]  info           prefix descriptor information returned/updated
     * \param [in]   isIPv4         Bool value to indicate IPv4(true) or IPv6(false)
     * \returns number of bytes read
     */
    int MPLinkState::parseDescrPrefix(u_char *data, int data_len, prefix_descriptor &info, bool isIPv4) {
        uint16_t type;
        uint16_t len;
        int data_read = 0;

        if (data_len < 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse link descriptor; too short",
                    peer_addr.c_str());
            return data_len;
        }

        memcpy(&type, data, 2);
        bgp::SWAP_BYTES(&type);

        memcpy(&len, data + 2, 2);
        bgp::SWAP_BYTES(&len);

        if (len > data_len - 4) {
            LOG_NOTICE("%s: bgp-ls: failed to parse prefix descriptor; type length is larger than available data %d>=%d",
                    peer_addr.c_str(), len, data_len);
            return data_len;
        }

        data += 4; data_read += 4;

        switch (type) {
            case PREFIX_DESCR_IP_REACH_INFO: {
                uint64_t    value_64bit;
                uint32_t    value_32bit;

                if (len < 2) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse prefix ip_reach_info sub-tlv; too short",
                               peer_addr.c_str());
                    data_read += len;
                    break;
                }

                info.prefix_len = *data;
                data_read++; data++;

                char ip_char[46];
                bzero(info.prefix, sizeof(info.prefix));
                memcpy(info.prefix, data, len - 1);

                if (isIPv4) {
                    inet_ntop(AF_INET, info.prefix, ip_char, sizeof(ip_char));

                    // Get the broadcast/ending IP address
                    if (info.prefix_len < 32) {
                        memcpy(&value_32bit, info.prefix, 4);
                        bgp::SWAP_BYTES(&value_32bit);

                        value_32bit |= 0xFFFFFFFF >> info.prefix_len;
                        bgp::SWAP_BYTES(&value_32bit);
                        memcpy(info.prefix_bcast, &value_32bit, 4);

                    } else
                        memcpy(info.prefix_bcast, info.prefix, sizeof(info.prefix_bcast));


                } else {
                    inet_ntop(AF_INET6, info.prefix, ip_char, sizeof(ip_char));

                    // Get the broadcast/ending IP address
                    if (info.prefix_len < 128) {
                        if (info.prefix_len >= 64) {
                            // High order bytes are left alone
                            memcpy(info.prefix_bcast, info.prefix, 8);

                            // Low order bytes are updated
                            memcpy(&value_64bit, &info.prefix[8], 8);
                            bgp::SWAP_BYTES(&value_64bit);

                            value_64bit |= 0xFFFFFFFFFFFFFFFF >> (info.prefix_len - 64);
                            bgp::SWAP_BYTES(&value_64bit);
                            memcpy(&info.prefix_bcast[8], &value_64bit, 8);

                        } else {
                            // Low order types are all ones
                            value_64bit = 0xFFFFFFFFFFFFFFFF;
                            memcpy(&info.prefix_bcast[8], &value_64bit, 8);

                            // High order bypes are updated
                            memcpy(&value_64bit, info.prefix, 8);
                            bgp::SWAP_BYTES(&value_64bit);

                            value_64bit |= 0xFFFFFFFFFFFFFFFF >> info.prefix_len;
                            bgp::SWAP_BYTES(&value_64bit);
                            memcpy(info.prefix_bcast, &value_64bit, 8);
                        }
                    } else
                            memcpy(info.prefix_bcast, info.prefix, sizeof(info.prefix_bcast));
                }

                data_read += len - 1;

                SELF_DEBUG("%s: bgp-ls: prefix ip_reach_info: prefix = %s/%d", peer_addr.c_str(),
                            ip_char, info.prefix_len);
                break;
            }
            case PREFIX_DESCR_MT_ID:
                if (len < 2) {
                    LOG_NOTICE("%s: bgp-ls: failed to parse prefix MT-ID descriptor sub-tlv; too short",
                            peer_addr.c_str());
                    data_read += len;
                    break;
                }

                if (len > 4) {
                    SELF_DEBUG("%s: bgp-ls: failed to parse link MT-ID descriptor sub-tlv; too long %d",
                               peer_addr.c_str(), len);
                    info.mt_id = 0;
                    data_read += len;
                    break;
                }

                memcpy(&info.mt_id, data, len); bgp::SWAP_BYTES(&info.mt_id);
                data_read += len;

                SELF_DEBUG("%s: bgp-ls: Link descriptior MT-ID = %08x ", peer_addr.c_str(), info.mt_id);

                break;

            case PREFIX_DESCR_OSPF_ROUTE_TYPE: {
                data_read++;
                switch (*data) {
                    case OSPF_RT_EXTERNAL_1:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type), "Ext-1");
                        break;

                    case OSPF_RT_EXTERNAL_2:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"Ext-2");
                        break;

                    case OSPF_RT_INTER_AREA:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"Inter");
                        break;

                    case OSPF_RT_INTRA_AREA:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"Intra");
                        break;

                    case OSPF_RT_NSSA_1:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"NSSA-1");
                        break;

                    case OSPF_RT_NSSA_2:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"NSSA-2");
                        break;

                    default:
                        snprintf(info.ospf_route_type, sizeof(info.ospf_route_type),"Intra");
                }
                SELF_DEBUG("%s: bgp-ls: prefix ospf route type is %s", peer_addr.c_str(), info.ospf_route_type);
                break;
            }

            default:
                LOG_NOTICE("%s: bgp-ls: Failed to parse prefix descriptor; invalid sub-tlv type of %d",
                        peer_addr.c_str(), type);
                data_read += len;
                break;
        }


        return data_read;
    }

    /**********************************************************************************//*
     * Hash node descriptor info
     *
     * \details will produce a hash for the node descriptor.  Info hash_bin will be updated.
     *
     * \param [in/out]  info           Node descriptor information returned/updated
     * \param [out]  hash_bin       Node descriptor information returned/updated
     */
    void MPLinkState::genNodeHashId(node_descriptor &info) {
        MD5 hash;

        hash.update(info.igp_router_id, sizeof(info.igp_router_id));
        hash.update((unsigned char *)&info.bgp_ls_id, sizeof(info.bgp_ls_id));
        hash.update((unsigned char *)&info.asn, sizeof(info.asn));
        hash.update(info.ospf_area_Id, sizeof(info.ospf_area_Id));
        hash.finalize();

        // Save the hash
        unsigned char *hash_bin = hash.raw_digest();
        memcpy(info.hash_bin, hash_bin, 16);
        delete[] hash_bin;
    }

} /* namespace bgp_msg */