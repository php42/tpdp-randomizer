/*
	Copyright (C) 2016 php42

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTIL_H
#define UTIL_H
#pragma once

#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#endif

/* endian independent binary functions. obeys alignment and aliasing rules.
 * this is intended for serialization and deserialization of integral types
 * to/from buffers such as file and network buffers. */

/* define UTIL_X86 on x86 based cpu architectures for performance optimizations
 *
 * strict aliasing rules permit memcpy to be used to convert types, and most
 * compilers will optimize away the temporary variable and the result is a single
 * wide load/store equivalent to a type-pun such as *(uint32_t*)buf */

#ifndef UTIL_X86
#if (defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)) && !defined(UTIL_NO_OPTIMIZATION)
#define UTIL_X86
#endif
#endif // UTIL_X86

inline void write_le64(void *dest, uint64_t val)
{
#ifndef UTIL_X86
    unsigned char *d = (unsigned char*)dest;
    for(unsigned int i = 0; i < sizeof(val); ++i)
    {
        d[i] = (unsigned char)val;
        val >>= 8;
    }
#else
    memcpy(dest, &val, sizeof(val));
#endif // UTIL_X86
}

inline uint64_t read_le64(const void *src)
{
#ifndef UTIL_X86
    const unsigned char *s = (const unsigned char*)src;
    return (((uint64_t)s[0]) | (((uint64_t)s[1]) << 8) | (((uint64_t)s[2]) << 16) | (((uint64_t)s[3]) << 24)
            | (((uint64_t)s[4]) << 32) | (((uint64_t)s[5]) << 40) | (((uint64_t)s[6]) << 48) | (((uint64_t)s[7]) << 56));
#else
    uint64_t temp;
    memcpy(&temp, src, sizeof(temp));
    return temp;
#endif // UTIL_X86
}

inline void write_le32(void *dest, uint32_t val)
{
#ifndef UTIL_X86
    unsigned char *d = (unsigned char*)dest;
    for(unsigned int i = 0; i < sizeof(val); ++i)
    {
        d[i] = (unsigned char)val;
        val >>= 8;
    }
#else
    memcpy(dest, &val, sizeof(val));
#endif // UTIL_X86
}

inline uint32_t read_le32(const void *src)
{
#ifndef UTIL_X86
    const unsigned char *s = (const unsigned char*)src;
    return (((uint32_t)s[0]) | (((uint32_t)s[1]) << 8) | (((uint32_t)s[2]) << 16) | (((uint32_t)s[3]) << 24));
#else
    uint32_t temp;
    memcpy(&temp, src, sizeof(temp));
    return temp;
#endif // UTIL_X86
}

inline void write_le16(void *dest, uint16_t val)
{
#ifndef UTIL_X86
    unsigned char *d = (unsigned char*)dest;
    for(unsigned int i = 0; i < sizeof(val); ++i)
    {
        d[i] = (unsigned char)val;
        val >>= 8;
    }
#else
    memcpy(dest, &val, sizeof(val));
#endif // UTIL_X86
}

inline uint16_t read_le16(const void *src)
{
#ifndef UTIL_X86
    const unsigned char *s = (const unsigned char*)src;
    return (((uint16_t)s[0]) | (((uint16_t)s[1]) << 8));
#else
    uint16_t temp;
    memcpy(&temp, src, sizeof(temp));
    return temp;
#endif // UTIL_X86
}

#endif // UTIL_H
