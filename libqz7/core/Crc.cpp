/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "qz7/Crc.h"

#include <byteswap.h>

namespace qz7 {

static quint32 Crc32IeeeLeData[1024];
static bool inited;

// create the CRC table if needed
quint32 CrcInitValue()
{
    if (inited)
        return 0xffffffff;

    quint32 *ctx = Crc32IeeeLeData;
    const quint32 poly = 0xEDB88320;
    int i, j;
    quint32 c;

    for (i = 0; i < 256; i++) {
        for (c = i, j = 0; j < 8; j++)
            c = (c>>1)^(poly & (-(c&1)));
        ctx[i] = c;
    }   
    for (i = 0; i < 256; i++)
        for(j=0; j<3; j++)
            ctx[256*(j+1) + i]= (ctx[256*j + i]>>8) ^ ctx[ ctx[256*j + i]&0xFF ];

    inited = true;
    return 0xffffffff;
}

/**
 * Calculate the CRC of a block
 * @param crcType the type of crc to calculate
 * @param crc CRC of previous blocks if any or initial value for CRC.
 * @return CRC updated with the data from the given block
 */
quint32 CrcUpdate(quint32 crc, const void *bufIn, size_t length)
{
    Q_ASSERT(inited);
    const quint32 *ctx = Crc32IeeeLeData;
    const quint8 *buffer = reinterpret_cast<const quint8 *>(bufIn);
    const quint8 *end = buffer+length;

    // fix up the initial alignment of the buffer
    while (buffer < end && (reinterpret_cast<unsigned long>(buffer) & 3)) {
        crc = ctx[((quint8)crc) ^ *buffer++] ^ (crc >> 8);
    }

    // try to do a fast word-by-word CRC
    const quint32 *buffer32 = reinterpret_cast<const quint32 *>(buffer);
    const quint32 *end32 = reinterpret_cast<const quint32 *>(reinterpret_cast<unsigned long>(end) & ~3UL);

    while (buffer32 < end32) {
        quint32 word = *buffer32++;

        if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
            word = bswap_32(word);

        crc ^= word;
        crc =  ctx[3*256 + ( crc     &0xFF)]
                ^ctx[2*256 + ((crc>>8 )&0xFF)]
                ^ctx[1*256 + ((crc>>16)&0xFF)]
                ^ctx[0*256 + ((crc>>24)     )];
    }
    buffer = reinterpret_cast<const quint8 *>(end32);

    // and handle any tail of the buffer
    while (buffer < end) {
        crc = ctx[((quint8)crc) ^ *buffer++] ^ (crc >> 8);
    }

    return crc;
}

}
