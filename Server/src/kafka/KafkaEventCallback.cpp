/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include "KafkaEventCallback.h"

KafkaEventCallback::KafkaEventCallback(bool *isConnectedRef, Logger *logPtr) : RdKafka::EventCb() {
    isConnected = isConnectedRef;
    logger = logPtr;
}

void KafkaEventCallback::event_cb (RdKafka::Event &event) {
    switch (event.type())
    {
        case RdKafka::Event::EVENT_ERROR:
            LOG_ERR("Kafka error: %s", RdKafka::err2str(event.err()).c_str());

            if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN) {
                LOG_ERR("Kafka all brokers down: %s", RdKafka::err2str(event.err()).c_str());
                *isConnected = false;
            }
            break;

        case RdKafka::Event::EVENT_STATS:
            LOG_INFO("Kafka stats: %s", event.str().c_str());
            break;

        case RdKafka::Event::EVENT_LOG: {
            switch (event.severity()) {
                case RdKafka::Event::EVENT_SEVERITY_EMERG:
                case RdKafka::Event::EVENT_SEVERITY_ALERT:
                case RdKafka::Event::EVENT_SEVERITY_CRITICAL:
                case RdKafka::Event::EVENT_SEVERITY_ERROR:
                    *isConnected = false;
                    LOG_ERR("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                            event.str().c_str());
                    break;
                case RdKafka::Event::EVENT_SEVERITY_WARNING:
                    LOG_WARN("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                             event.str().c_str());
                    break;
                case RdKafka::Event::EVENT_SEVERITY_NOTICE:
                    LOG_NOTICE("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                               event.str().c_str());
                    break;
                default:
                    LOG_INFO("Kafka LOG-%i-%s: %s", event.severity(), event.fac().c_str(),
                             event.str().c_str());
                    break;
            }
            break;
        }
        default:
            LOG_INFO("Kafka event type = %d (%s) %s", event.type(), RdKafka::err2str(event.err()).c_str(),
                     event.str().c_str());
            break;
    }
}
