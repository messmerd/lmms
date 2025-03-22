/*
 * BitOperations.h - Bit operations and endian helpers
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *                    2025 Dalton Messmer <messmer.dalton/at/gmail.com>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_BIT_OPERATIONS_H
#define LMMS_BIT_OPERATIONS_H

#include <bit>
#include <concepts>

#include "lmms_basics.h"

namespace lmms
{


constexpr auto isLittleEndian() -> bool
{
	return std::endian::native == std::endian::little;
}

// TODO C++23: Use std::byteswap
template<std::integral T>
constexpr auto byteswap(T value) -> T
{
	if constexpr (sizeof(T) <= 1)
	{
		return value;
	}
	else if constexpr (sizeof(T) == 2)
	{
		return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
	}
	else if constexpr (sizeof(T) == 4)
	{
		return ((value & 0xff000000) >> 24)
			| ((value & 0x00ff0000) >> 8)
			| ((value & 0x0000ff00) << 8)
			| ((value & 0x000000ff) << 24);
	}
	else if constexpr (sizeof(T) == 8)
	{
		// Taken from https://stackoverflow.com/a/21507710/8704745
		value = (value & 0x00000000FFFFFFFF) << 32 | (value & 0xFFFFFFFF00000000) >> 32;
		value = (value & 0x0000FFFF0000FFFF) << 16 | (value & 0xFFFF0000FFFF0000) >> 16;
		value = (value & 0x00FF00FF00FF00FF) << 8  | (value & 0xFF00FF00FF00FF00) >> 8;
		return value;
	}
	else
	{
		static_assert(always_false_v<T>);
	}
}

template<std::integral T>
constexpr auto byteswapIfBE(T value) -> T
{
	if constexpr (isLittleEndian())
	{
		return value;
	}
	else
	{
		return byteswap(value);
	}
}

static_assert(byteswap<std::int8_t>(0x12) == 0x12);
static_assert(byteswap<std::uint8_t>(0x12) == 0x12);

static_assert(byteswap<std::int16_t>(0x1234) == 0x3412);
static_assert(byteswap<std::int16_t>(0x9876) == 0x7698);
static_assert(byteswap<std::uint16_t>(0x1234) == 0x3412);
static_assert(byteswap<std::uint16_t>(0x9876) == 0x7698);

static_assert(byteswap<std::int32_t>(0x12345678) == 0x78563412);
static_assert(byteswap<std::int32_t>(0x98765432) == 0x32547698);
static_assert(byteswap<std::uint32_t>(0x12345678) == 0x78563412);
static_assert(byteswap<std::uint32_t>(0x98765432) == 0x32547698);

static_assert(byteswap<std::int64_t>(0x1234567898765432) == 0x3254769878563412);
static_assert(byteswap<std::int64_t>(0x9876543210123456) == 0x5634121032547698);
static_assert(byteswap<std::uint64_t>(0x1234567898765432) == 0x3254769878563412);
static_assert(byteswap<std::uint64_t>(0x9876543210123456) == 0x5634121032547698);

} // namespace lmms

#endif // LMMS_BIT_OPERATIONS_H
