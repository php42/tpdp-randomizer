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
#include <cstdint>

/* endian independent binary functions, also safe for unaligned memory access */

static inline void write_le32(void *dest, uint32_t val)
{
    uint8_t *d = (uint8_t*)dest;
    d[0] = val & 0xFF;
    d[1] = (val >> 8) & 0xFF;
    d[2] = (val >> 16) & 0xFF;
    d[3] = (val >> 24) & 0xFF;
}

static inline uint32_t read_le32(const void *src)
{
    const uint8_t *s = (const uint8_t*)src;
    uint32_t temp = 0;

    temp |= s[0];
    temp |= (uint32_t(s[1]) << 8);
    temp |= (uint32_t(s[2]) << 16);
    temp |= (uint32_t(s[3]) << 24);

    return temp;
}

static inline void write_le16(void *dest, uint16_t val)
{
	uint8_t *d = (uint8_t*)dest;
	d[0] = val & 0xFF;
	d[1] = (val >> 8) & 0xFF;
}

static inline uint16_t read_le16(const void *src)
{
	const uint8_t *s = (const uint8_t*)src;
	uint16_t temp = 0;

	temp |= s[0];
	temp |= (uint16_t(s[1]) << 8);

	return temp;
}

#endif // UTIL_H
