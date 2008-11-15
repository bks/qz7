/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>,
 * copyright (c) 2008 Benjamin K. Stuhl <bks24@cornell.edu>
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
#ifndef CRC_H
#define CRC_H
#include <QtCore/QtGlobal>
namespace qz7 {
quint32 CrcUpdate(quint32 crcInit, const void *buffer, size_t length);
quint32 CrcInitValue();
inline quint32 CrcValue(quint32 crc) { return crc ^ 0xffffffff; };
}
#endif
