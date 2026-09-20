/*
 * MixHelpers.cpp - helper functions for mixing buffers
 *
 * Copyright (c) 2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "MixHelpers.h"

#include <algorithm>
#include <cmath>

#include "ValueBuffer.h"
#include "SampleFrame.h"

namespace lmms::MixHelpers
{

namespace {

constexpr auto SilenceThreshold = 0.000001f; // -120 dBFS

/*! \brief Function for applying MIXOP on all sample frames */
template<typename MIXOP>
inline void run(SampleFrame* dst, const SampleFrame* src, int frames, const MIXOP& OP)
{
	for( int i = 0; i < frames; ++i )
	{
		OP( dst[i], src[i] );
	}
}

/*! \brief Function for applying MIXOP on all sample frames - split source */
template<typename MIXOP>
inline void run(SampleFrame* dst, const sample_t* srcLeft, const sample_t* srcRight, int frames, const MIXOP& OP)
{
	for( int i = 0; i < frames; ++i )
	{
		const SampleFrame src = { srcLeft[i], srcRight[i] };
		OP( dst[i], src );
	}
}

} // namespace

bool isSilent(const SampleFrame* src, int frames)
{
	return isSilent({&src[0][0], static_cast<std::size_t>(frames * 2)});
}

bool isSilent(std::span<const sample_t> buffer)
{
	return std::ranges::all_of(buffer, [&](const sample_t s) { return std::abs(s) < SilenceThreshold; });
}

bool isSilent(PlanarBufferView<const float> buffer)
{
	for (ch_cnt_t ch = 0; ch < buffer.channels(); ++ch)
	{
		if (!isSilent(buffer.buffer(ch))) { return false; }
	}
	return true;
}

void copy(PlanarBufferView<float> dst, f_cnt_t dstOffset, PlanarBufferView<const float> src, f_cnt_t srcOffset)
{
	assert(dst.channels() == src.channels());
	assert(dstOffset < dst.frames());
	assert(srcOffset < src.frames());

	const auto framesToCopy = src.frames() - srcOffset;
	assert(framesToCopy <= dst.frames() - dstOffset);

	const auto channels = dst.channels();
	for (ch_cnt_t ch = 0; ch < channels; ++ch)
	{
		float* dstPtr = dst.bufferPtr(ch);
		const float* srcPtr = src.bufferPtr(ch);
		for (f_cnt_t idx = 0; idx < framesToCopy; ++idx)
		{
			dstPtr[dstOffset + idx] = srcPtr[srcOffset + idx];
		}
	}
}

struct AddOp
{
	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst += src;
	}
} ;

void add( SampleFrame* dst, const SampleFrame* src, int frames )
{
	run<>( dst, src, frames, AddOp() );
}


void add(PlanarBufferView<sample_t> dst, PlanarBufferView<const sample_t> src)
{
	assert(dst.channels() == src.channels());
	assert(dst.frames() == src.frames());

	const auto channels = dst.channels();
	const auto frames = dst.frames();
	for (ch_cnt_t channel = 0; channel < channels; ++channel)
	{
		auto* dstPtr = dst.bufferPtr(channel);
		const auto* srcPtr = src.bufferPtr(channel);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] += srcPtr[frame];
		}
	}
}


struct AddMultipliedOp
{
	AddMultipliedOp( float coeff ) : m_coeff( coeff ) { }

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst += src * m_coeff;
	}

	const float m_coeff;
} ;


void addMultiplied(PlanarBufferView<float> dst, PlanarBufferView<const float> src, float coeffSrc)
{
	assert(dst.channels() == src.channels());
	assert(dst.frames() == src.frames());

	const ch_cnt_t channels = dst.channels();
	const f_cnt_t frames = dst.frames();
	for (ch_cnt_t ch = 0; ch < channels; ++ch)
	{
		float* dstPtr = dst.bufferPtr(ch);
		const float* srcPtr = src.bufferPtr(ch);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] += srcPtr[frame] * coeffSrc;
		}
	}
}

void addMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames )
{
	run<>( dst, src, frames, AddMultipliedOp(coeffSrc) );
}


struct AddSwappedMultipliedOp
{
	AddSwappedMultipliedOp( float coeff ) : m_coeff( coeff ) { }

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] += src[1] * m_coeff;
		dst[1] += src[0] * m_coeff;
	}

	const float m_coeff;
};

void multiply(PlanarBufferView<float> dst, float coeff)
{
	const ch_cnt_t channels = dst.channels();
	const f_cnt_t frames = dst.frames();
	for (ch_cnt_t ch = 0; ch < channels; ++ch)
	{
		float* dstPtr = dst.bufferPtr(ch);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] *= coeff;
		}
	}
}

void multiply(SampleFrame* dst, float coeff, int frames)
{
	for (int i = 0; i < frames; ++i)
	{
		dst[i] *= coeff;
	}
}

void addSwappedMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames )
{
	run<>( dst, src, frames, AddSwappedMultipliedOp(coeffSrc) );
}

void addMultipliedByBuffer(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	float coeffSrc, const ValueBuffer* coeffSrcBuf)
{
	assert(dst.channels() == src.channels());
	assert(dst.frames() == src.frames());

	const ch_cnt_t channels = dst.channels();
	const f_cnt_t frames = dst.frames();
	for (ch_cnt_t ch = 0; ch < channels; ++ch)
	{
		float* dstPtr = dst.bufferPtr(ch);
		const float* srcPtr = src.bufferPtr(ch);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] += srcPtr[frame] * coeffSrc * coeffSrcBuf->values()[frame];
		}
	}
}

void addMultipliedByBuffers(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	const ValueBuffer* coeffSrcBuf1, const ValueBuffer* coeffSrcBuf2)
{
	assert(dst.channels() == src.channels());
	assert(dst.frames() == src.frames());

	const ch_cnt_t channels = dst.channels();
	const f_cnt_t frames = dst.frames();
	for (ch_cnt_t ch = 0; ch < channels; ++ch)
	{
		float* dstPtr = dst.bufferPtr(ch);
		const float* srcPtr = src.bufferPtr(ch);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] += srcPtr[frame] * coeffSrcBuf1->values()[frame] * coeffSrcBuf2->values()[frame];
		}
	}
}


struct AddMultipliedStereoOp
{
	AddMultipliedStereoOp( float coeffLeft, float coeffRight )
	{
		m_coeffs[0] = coeffLeft;
		m_coeffs[1] = coeffRight;
	}

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] += src[0] * m_coeffs[0];
		dst[1] += src[1] * m_coeffs[1];
	}

	std::array<float, 2> m_coeffs;
} ;


void addMultipliedStereo( SampleFrame* dst, const SampleFrame* src, float coeffSrcLeft, float coeffSrcRight, int frames )
{

	run<>( dst, src, frames, AddMultipliedStereoOp(coeffSrcLeft, coeffSrcRight) );
}





struct MultiplyAndAddMultipliedOp
{
	MultiplyAndAddMultipliedOp( float coeffDst, float coeffSrc )
	{
		m_coeffs[0] = coeffDst;
		m_coeffs[1] = coeffSrc;
	}

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] = dst[0]*m_coeffs[0] + src[0]*m_coeffs[1];
		dst[1] = dst[1]*m_coeffs[0] + src[1]*m_coeffs[1];
	}

	std::array<float, 2> m_coeffs;
} ;


void multiplyAndAddMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffDst, float coeffSrc, int frames )
{
	run<>( dst, src, frames, MultiplyAndAddMultipliedOp(coeffDst, coeffSrc) );
}



void multiplyAndAddMultipliedJoined( SampleFrame* dst,
										const sample_t* srcLeft,
										const sample_t* srcRight,
										float coeffDst, float coeffSrc, int frames )
{
	run<>( dst, srcLeft, srcRight, frames, MultiplyAndAddMultipliedOp(coeffDst, coeffSrc) );
}

} // namespace lmms::MixHelpers

