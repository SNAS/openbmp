/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#ifndef OPENBMP_KAFKADELIVERYREPORTCALLBACK_H
#define OPENBMP_KAFKADELIVERYREPORTCALLBACK_H

#include <librdkafka/rdkafkacpp.h>
#include "Logger.h"

class KafkaDeliveryReportCallback : public RdKafka::DeliveryReportCb {
public:
    void dr_cb (RdKafka::Message &message);
};

#endif //OPENBMP_KAFKADELIVERYREPORTCALLBACK_H
