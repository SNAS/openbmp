/*
 * Copyright (c) 2013-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "AddPathDataContainer.h"
#include "OpenMsg.h"

std::map<size_t, AddPathDataContainer::addPathMapType> AddPathDataContainer::peerInfoToAddPathMap;

/**
 * Constructor for class
 *
 * \details Constructs peer related object with Add Path data.
 *
 * \param [in] peer_info   Persistent peer information
 */
AddPathDataContainer::AddPathDataContainer(BMPReader::peer_info *peer_info){
    if (AddPathDataContainer::peerInfoToAddPathMap.count((size_t)&peer_info) == 0){
        AddPathDataContainer::addPathMapType addPathMap;
        peerInfoToAddPathMap.insert(std::pair<size_t, AddPathDataContainer::addPathMapType>((size_t)&peer_info, addPathMap));
    }

    this->addPathMap = &AddPathDataContainer::peerInfoToAddPathMap[(size_t)&peer_info];
}

/**
 * Add Add Path data to persistent storage
 *
 * \param [in] afi              Afi code from RFC
 * \param [in] safi             Safi code form RFC
 * \param [in] send_receive     Send Recieve code from RFC
 * \param [in] sent_open        Is obtained from sent open message. False if from recieved
 */
void AddPathDataContainer::addAddPath(int afi, int safi, int send_receive, bool sent_open) {
    addPathMapType::iterator iterator = this->addPathMap->find(this->getAFiSafiKeyString(afi, safi));
    if(iterator == this->addPathMap->end()) {
        sendReceiveCodesForSentAndReceivedOpenMessageStructure newStructure;

        if (sent_open) {
            newStructure.sendReceiveCodeForSentOpenMessage = send_receive;
        } else {
            newStructure.sendReceiveCodeForReceivedOpenMessage = send_receive;
        }

        this->addPathMap->insert(std::pair<std::string, sendReceiveCodesForSentAndReceivedOpenMessageStructure>(
                this->getAFiSafiKeyString(afi, safi),
                newStructure
        ));
    } else {
        if (sent_open) {
            iterator->second.sendReceiveCodeForSentOpenMessage = send_receive;
        } else {
            iterator->second.sendReceiveCodeForReceivedOpenMessage = send_receive;
        }
    }
}

/**
 * Constructor for class
 *
 * \details Constructs peer related object with Add Path data.
 *
 * \param [in] peer_info   Persistent peer information
 */
std::string AddPathDataContainer::getAFiSafiKeyString(int afi, int safi) {
    std::string result = std::to_string(afi);
    result.append("_");
    result.append(std::to_string(safi));
    return result;
}

/**
 * Is add path capability enabled for such AFI and SAFI
 *
 * \param [in] afi              Afi code from RFC
 * \param [in] safi             Safi code form RFC
 *
 * \return is enabled
 */
bool AddPathDataContainer::isAddPathEnabled(int afi, int safi) {
    addPathMapType::iterator iterator = this->addPathMap->find(this->getAFiSafiKeyString(afi, safi));

    if(iterator == this->addPathMap->end()) {
        return false;
    } else {
        return (
            iterator->second.sendReceiveCodeForSentOpenMessage == bgp_msg::OpenMsg::BGP_CAP_ADD_PATH_SEND or
                    iterator->second.sendReceiveCodeForSentOpenMessage == bgp_msg::OpenMsg::BGP_CAP_ADD_PATH_SEND_RECEIVE
            ) and (
            iterator->second.sendReceiveCodeForReceivedOpenMessage == bgp_msg::OpenMsg::BGP_CAP_ADD_PATH_RECEIVE or
                    iterator->second.sendReceiveCodeForReceivedOpenMessage == bgp_msg::OpenMsg::BGP_CAP_ADD_PATH_SEND_RECEIVE
            );
    }
}
