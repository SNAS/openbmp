#include "EVPN.h"

namespace bgp_msg {

    /**
     * Constructor for class
     *
     * \details Handles bgp Extended Communities
     *
     * \param [in]     logPtr       Pointer to existing Logger for app logging
     * \param [in]     peerAddr     Printed form of peer address used for logging
     * \param [in]     isUnreach    True if MP UNREACH, false if MP REACH
     * \param [out]    parsed_data  Reference to parsed_update_data; will be updated with all parsed data

     * \param [in]     enable_debug Debug true to enable, false to disable
     */
    EVPN::EVPN(Logger *logPtr, std::string peerAddr, bool isUnreach,
               UpdateMsg::parsed_update_data *parsed_data, bool enable_debug) {
        logger = logPtr;
        debug = enable_debug;
        peer_addr = peerAddr;
        this->parsed_data = parsed_data;
        this->isUnreach = isUnreach;
    }

    EVPN::~EVPN() {
    }

    /**
     * Parse Ethernet Segment Identifier
     *
     * \details
     *      Will parse the Segment Identifier. Based on https://tools.ietf.org/html/rfc7432#section-5
     *
     * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
     * \param [out]     rd_type                    Reference to RD type.
     * \param [out]     rd_assigned_number         Reference to Assigned Number subfield
     * \param [out]     rd_administrator_subfield  Reference to Administrator subfield
     */
    void EVPN::parseEthernetSegmentIdentifier(u_char *data_pointer, std::string *parsed_data) {
        std::stringstream result;
        uint8_t type = *data_pointer;

        data_pointer++;

        result << (int) type << " ";

        switch (type) {
            case 0: {
                for (int i = 0; i < 9; i++) {
                    result << std::hex << setfill('0') << setw(2) << (int) data_pointer[i];
                }
                break;
            }
            case 1: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint16_t CE_LACP_port_key;
                memcpy(&CE_LACP_port_key, data_pointer, 2);
                bgp::SWAP_BYTES(&CE_LACP_port_key, 2);

                result << std::dec << (int) CE_LACP_port_key;

                break;
            }
            case 2: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint16_t root_bridge_priority;
                memcpy(&root_bridge_priority, data_pointer, 2);
                bgp::SWAP_BYTES(&root_bridge_priority, 2);

                result << std::dec << (int) root_bridge_priority;

                break;
            }
            case 3: {
                for (int i = 0; i < 6; ++i) {
                    if (i != 0) result << ':';
                    result.width(2); //< Use two chars for each byte
                    result.fill('0'); //< Fill up with '0' if the number is only one hexadecimal digit
                    result << std::hex << (int) (data_pointer[i]);
                }
                data_pointer += 6;

                result << " ";

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 3);
                bgp::SWAP_BYTES(&local_discriminator_value, 4);
                local_discriminator_value = local_discriminator_value >> 8;
                result << std::dec << (int) local_discriminator_value;

                break;
            }
            case 4: {
                uint32_t router_id;
                memcpy(&router_id, data_pointer, 4);
                bgp::SWAP_BYTES(&router_id, 4);
                result << std::dec << (int) router_id << " ";

                data_pointer += 4;

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 4);
                bgp::SWAP_BYTES(&local_discriminator_value, 4);
                result << std::dec << (int) local_discriminator_value;
                break;
            }
            case 5: {
                uint32_t as_number;
                memcpy(&as_number, data_pointer, 4);
                bgp::SWAP_BYTES(&as_number, 4);
                result << std::dec << (int) as_number << " ";

                data_pointer += 4;

                uint32_t local_discriminator_value;
                memcpy(&local_discriminator_value, data_pointer, 4);
                bgp::SWAP_BYTES(&local_discriminator_value, 4);
                result << std::dec << (int) local_discriminator_value;
                break;
            }
            default:
                LOG_WARN("%s: MP_REACH Cannot parse ethernet segment identifyer type: %d", type);
                break;
        }

        *parsed_data = result.str();
    }

    /**
     * Parse Route Distinguisher
     *
     * \details
     *      Will parse the Route Distinguisher. Based on https://tools.ietf.org/html/rfc4364#section-4.2
     *
     * \param [in/out]  data_pointer  Pointer to the beginning of Route Distinguisher
     * \param [out]     rd_type                    Reference to RD type.
     * \param [out]     rd_assigned_number         Reference to Assigned Number subfield
     * \param [out]     rd_administrator_subfield  Reference to Administrator subfield
     */
    void EVPN::parseRouteDistinguisher(u_char *data_pointer, uint8_t *rd_type, std::string *rd_assigned_number,
                                       std::string *rd_administrator_subfield) {

        data_pointer++;
        *rd_type = *data_pointer;
        data_pointer++;

        switch (*rd_type) {
            case 0: {
                uint16_t administration_subfield;
                bzero(&administration_subfield, 2);
                memcpy(&administration_subfield, data_pointer, 2);

                data_pointer += 2;

                uint32_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 4);
                memcpy(&assigned_number_subfield, data_pointer, 4);

                bgp::SWAP_BYTES(&administration_subfield);
                bgp::SWAP_BYTES(&assigned_number_subfield);

                *rd_assigned_number = std::to_string(assigned_number_subfield);
                *rd_administrator_subfield = std::to_string(administration_subfield);

                break;
            };

            case 1: {
                u_char administration_subfield[4];
                bzero(&administration_subfield, 4);
                memcpy(&administration_subfield, data_pointer, 4);

                data_pointer += 4;

                uint16_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 2);
                memcpy(&assigned_number_subfield, data_pointer, 2);

                bgp::SWAP_BYTES(&assigned_number_subfield);

                char administration_subfield_chars[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, administration_subfield, administration_subfield_chars, INET_ADDRSTRLEN);

                *rd_assigned_number = std::to_string(assigned_number_subfield);
                *rd_administrator_subfield = std::string(administration_subfield_chars);

                break;
            };

            case 2: {
                uint32_t administration_subfield;
                bzero(&administration_subfield, 4);
                memcpy(&administration_subfield, data_pointer, 4);

                data_pointer += 4;

                uint16_t assigned_number_subfield;
                bzero(&assigned_number_subfield, 2);
                memcpy(&assigned_number_subfield, data_pointer, 2);

                bgp::SWAP_BYTES(&administration_subfield);
                bgp::SWAP_BYTES(&assigned_number_subfield);

                *rd_assigned_number = std::to_string(assigned_number_subfield);
                *rd_administrator_subfield = std::to_string(administration_subfield);

                break;
            };
        }
    }

    // TODO: Refactor this method as it's overloaded - each case statement can be its own method
    /**
     * Parse all EVPN nlri's
     *
     *
     * \details
     *      Parsing based on https://tools.ietf.org/html/rfc7432.  Will process all NLRI's in data.
     *
     * \param [in]   data                   Pointer to the start of the prefixes to be parsed
     * \param [in]   data_len               Length of the data in bytes to be read
     *
     */
    void EVPN::parseNlriData(u_char *data, uint16_t data_len) {
        u_char      *data_pointer = data;
        u_char      ip_binary[16];
        int         addr_bytes;
        char        ip_char[40];
        int         data_read = 0;

        while ((data_read + 10 /* min read */) < data_len) {
            bgp::evpn_tuple tuple;

            //Cleanup variables in case of not modified
            tuple.mpls_label_1 = 0;
            tuple.mpls_label_2 = 0;
            tuple.mac_len = 0;
            tuple.ip_len = 0;


            // TODO: Keep an eye on this, as we might need to support add-paths for evpn
            tuple.path_id = 0;
            tuple.originating_router_ip_len = 0;

            uint8_t route_type = *data_pointer;
            data_pointer++;

            int len = *data_pointer;
            data_pointer++;

            parseRouteDistinguisher(
                    data_pointer,
                    &tuple.rd_type,
                    &tuple.rd_assigned_number,
                    &tuple.rd_administrator_subfield
            );
            data_pointer += 8;

            data_read += 10;
            len -= 8; // len doesn't include the route type and len octets

            switch (route_type) {
                case EVPN_ROUTE_TYPE_ETHERNET_AUTO_DISCOVERY: {

                    if ((data_read + 17 /* expected read size */) <= data_len) {

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, &tuple.ethernet_segment_identifier);
                        data_pointer += 10;

                        //Ethernet Tag Id (4 bytes), printing in hex.

                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream ethernet_tag_id_stream;

                        for (int i = 0; i < 4; i++) {
                            ethernet_tag_id_stream << std::hex << setfill('0') << setw(2) << (int) ethernet_id[i];
                        }

                        tuple.ethernet_tag_id_hex = ethernet_tag_id_stream.str();

                        //MPLS Label (3 bytes)
                        memcpy(&tuple.mpls_label_1, data_pointer, 3);
                        bgp::SWAP_BYTES(&tuple.mpls_label_1);
                        tuple.mpls_label_1 >>= 8;

                        data_pointer += 3;
                        data_read += 17;
                        len -= 17;
                    }
                    break;
                }
                case EVPN_ROUTE_TYPE_MAC_IP_ADVERTISMENT: {

                    if ((data_read + 25 /* expected read size */) <= data_len) {

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, &tuple.ethernet_segment_identifier);
                        data_pointer += 10;

                        // Ethernet Tag ID (4 bytes)

                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream ethernet_tag_id_stream;

                        for (int i = 0; i < 4; i++) {
                            ethernet_tag_id_stream << std::hex << setfill('0') << setw(2) << (int) ethernet_id[i];
                        }

                        tuple.ethernet_tag_id_hex = ethernet_tag_id_stream.str();

                        // MAC Address Length (1 byte)
                        uint8_t mac_address_length = *data_pointer;

                        tuple.mac_len = mac_address_length;
                        data_pointer++;

                        // MAC Address (6 byte)
                        tuple.mac.assign(bgp::parse_mac(data_pointer));
                        data_pointer += 6;

                        // IP Address Length (1 byte)
                        tuple.ip_len = *data_pointer;
                        data_pointer++;

                        data_read += 22;
                        len -= 22;

                        addr_bytes = tuple.ip_len > 0 ? (tuple.ip_len / 8) : 0;

                        if (tuple.ip_len > 0 and (addr_bytes + data_read) <= data_len) {
                            // IP Address (0, 4, or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, addr_bytes);

                            inet_ntop(tuple.ip_len > 32 ? AF_INET6 : AF_INET, ip_binary, ip_char, sizeof(ip_char));

                            tuple.ip = ip_char;

                            data_pointer += addr_bytes;
                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }

                        if ((data_read + 3) <= data_len) {

                            // MPLS Label1 (3 bytes)
                            memcpy(&tuple.mpls_label_1, data_pointer, 3);
                            bgp::SWAP_BYTES(&tuple.mpls_label_1);
                            tuple.mpls_label_1 >>= 8;

                            data_pointer += 3;
                            data_read += 3;
                            len -= 3;
                        }

                        // Parse second label if present
                        if (len == 3) {
                            SELF_DEBUG("%s: parsing second evpn label\n", peer_addr.c_str());

                            memcpy(&tuple.mpls_label_2, data_pointer, 3);
                            bgp::SWAP_BYTES(&tuple.mpls_label_2);
                            tuple.mpls_label_2 >>= 8;

                            data_pointer += 3;
                            data_read += 3;
                            len -= 3;
                        }
                    }
                    break;
                }
                case EVPN_ROUTE_TYPE_INCLUSIVE_MULTICAST_ETHERNET_TAG: {

                    if ((data_read + 5 /* expected read size */) <= data_len) {

                        // Ethernet Tag ID (4 bytes)
                        u_char ethernet_id[4];
                        bzero(&ethernet_id, 4);
                        memcpy(&ethernet_id, data_pointer, 4);
                        data_pointer += 4;

                        std::stringstream ethernet_tag_id_stream;

                        for (int i = 0; i < 4; i++) {
                            ethernet_tag_id_stream << std::hex << setfill('0') << setw(2) << (int) ethernet_id[i];
                        }

                        tuple.ethernet_tag_id_hex = ethernet_tag_id_stream.str();

                        // IP Address Length (1 byte)
                        tuple.originating_router_ip_len = *data_pointer;
                        data_pointer++;

                        data_read += 5;
                        len -= 5;

                        addr_bytes = tuple.originating_router_ip_len > 0 ? (tuple.originating_router_ip_len / 8) : 0;

                        if (tuple.originating_router_ip_len > 0 and (addr_bytes + data_read) <= data_len) {

                            // Originating Router's IP Address (4 or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, addr_bytes);

                            inet_ntop(tuple.originating_router_ip_len > 32 ? AF_INET6 : AF_INET,
                                      ip_binary, ip_char, sizeof(ip_char));

                            tuple.originating_router_ip = ip_char;

                            data_pointer += addr_bytes;
                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }
                    }

                    break;
                }
                case EVPN_ROUTE_TYPE_ETHERNET_SEGMENT_ROUTE: {

                    if ((data_read + 11 /* expected read size */) <= data_len) {

                        // Ethernet Segment Identifier (10 bytes)
                        parseEthernetSegmentIdentifier(data_pointer, &tuple.ethernet_segment_identifier);
                        data_pointer += 10;

                        // IP Address Length (1 bytes)
                        tuple.originating_router_ip_len = *data_pointer;
                        data_pointer++;

                        data_read += 11;
                        len -= 11;

                        addr_bytes = tuple.originating_router_ip_len > 0 ? (tuple.originating_router_ip_len / 8) : 0;

                        if (tuple.originating_router_ip_len > 0 and (addr_bytes + data_read) <= data_len) {

                            // Originating Router's IP Address (4 or 16 bytes)
                            bzero(ip_binary, 16);
                            memcpy(&ip_binary, data_pointer, (int) tuple.originating_router_ip_len / 8);

                            inet_ntop(tuple.originating_router_ip_len > 32 ? AF_INET6 : AF_INET,
                                      ip_binary, ip_char, sizeof(ip_char));

                            tuple.originating_router_ip = ip_char;

                            data_read += addr_bytes;
                            len -= addr_bytes;
                        }
                    }

                    break;
                }
                default: {
                    LOG_INFO("%s: EVPN ROUTE TYPE %d is not implemented yet, skipping",
                             peer_addr.c_str(), route_type);
                    break;
                }
            }

            if (isUnreach)
                parsed_data->evpn_withdrawn.push_back(tuple);
            else
                parsed_data->evpn.push_back(tuple);

            SELF_DEBUG("%s: Processed evpn NLRI read %d of %d, nlri len %d", peer_addr.c_str(),
                       data_read, data_len, len);
        }
    }
} /* namespace bgp_msg */