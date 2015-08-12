/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_KAFKAEVENTCALLBACK_H
#define OPENBMP_KAFKAEVENTCALLBACK_H

#include <librdkafka/rdkafkacpp.h>
#include "Logger.h"

class KafkaEventCallback : public RdKafka::EventCb {

public:
    /**
     * Constructor for callback
     *
     * \param isConnected[in,out]   Pointer to isConnected bool to indicate if connected or not
     * \param logPtr[in]            Pointer to the Logger class to use for logging
     */
    KafkaEventCallback(bool *isConnectedRef, Logger *logPtr);

    void event_cb (RdKafka::Event &event);

    void setLogger(Logger *logPtr);

private:
    Logger *logger;
    bool   *isConnected;           // Indicates if connected to the broker or not.
};


#endif //OPENBMP_KAFKAEVENTCALLBACK_H
