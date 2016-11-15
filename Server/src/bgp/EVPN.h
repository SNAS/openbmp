#ifndef _OPENBMP_EVPN_H_
#define _OPENBMP_EVPN_H_

#include <cstdint>
#include <cinttypes>
#include <sys/types.h>

#include "MPReachAttr.h"
#include "MPUnReachAttr.h"
#include "Logger.h"
#include "MsgBusInterface.hpp"

namespace bgp_msg {

    class EVPN {

    public:

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
        EVPN(Logger *logPtr, std::string peerAddr,
                   UpdateMsg::parsed_update_data *parsed_data, bool enable_debug);
        virtual ~EVPN();

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
        void parseEthernetSegmentIdentifier(u_char *data_pointer, std::string *parsed_data);

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
        static void parseRouteDistinguisher(u_char *data_pointer, uint8_t *rd_type, std::string *rd_assigned_number,
                                       std::string *rd_administrator_subfield);

        void parse(MPReachAttr::mp_reach_nlri &nlri);

        /**
         * Parse EVPN nlri
         * \details
         *      Parsing based on https://tools.ietf.org/html/rfc7432
         *
         * @param [in] nlri           Reference to parsed NLRI struct
         */
        void parse_nlri(MPReachAttr::mp_reach_nlri &nlri);


    private:
        bool             debug;                           ///< debug flag to indicate debugging
        Logger           *logger;                         ///< Logging class pointer
        std::string      peer_addr;                       ///< Printed form of the peer address for logging
        UpdateMsg::parsed_update_data *parsed_data;       ///< Parsed data structure

    };

}


#endif //_OPENBMP_EVPN_H_
