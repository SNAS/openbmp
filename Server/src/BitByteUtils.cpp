/*
 * Copyright (c) 2013-2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */

#include <cstring>

/**
 * Reverse bytes for int value from network to host or host to network
 *
 * \details
 *      Like hton and ntoh, but supports swaping bytes in either direction for
 *      variable sizes of data, such as 16bit, 32bit, 64bit.
 *
 * \param [in,out]  net_int     Network or host int value as char * (pointer)
 * \param [in]      sz          Size of the char buffer (2, 4, 8)
 */
void reverseBytes(unsigned char *net_int, int sz) {
    // Allocate a working buffer
    unsigned char *buf = new unsigned char[sz];

    // Make a copy
    memcpy(buf, (void *)net_int, sz);

    int i2 = 0;
    for (int i=sz-1; i >= 0; i--)
        net_int[i2++] = buf[i];

    // Free the buffer space
    delete [] buf;
}

