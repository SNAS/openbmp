/*
 * Copyright (c) 2013-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#include "KafkaPeerPartitionerCallback.h"
#include <string>
#include <ctime>

KafkaPeerPartitionerCallback::KafkaPeerPartitionerCallback()
            : RdKafka::PartitionerCb() {
}

int32_t KafkaPeerPartitionerCallback::partitioner_cb (const RdKafka::Topic *topic,
                                                  const std::string *key,
                                                  int32_t partition_cnt,
                                                  void *msg_opaque) {

    return (key->front() + key->back()) % partition_cnt;
}