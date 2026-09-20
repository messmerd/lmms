/*
 * MixHelpers.h - helper functions for mixing buffers
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

#ifndef LMMS_MIX_HELPERS_H
#define LMMS_MIX_HELPERS_H

#include "AudioBufferView.h"

namespace lmms
{

class ValueBuffer;
class SampleFrame;

namespace MixHelpers
{

bool isSilent(const SampleFrame* src, int frames);

bool isSilent(std::span<const sample_t> buffer);

bool isSilent(PlanarBufferView<const float> buffer);

//! @brief Copies data from @a src to @a dst, upmixing from mono to stereo
//!        starting from the given offsets
//! @note If the @a dst subset has more frames than the @a src subset, the additional
//!       frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @param dstOffset the starting frame within @p dst
//! @param srcOffset the starting frame within @p src
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.frames() - dstOffset >= src.frames() - srcOffset
void monoUpmix(PlanarBufferView<float, 2> dst, PlanarBufferView<const float, 1> src,
	f_cnt_t dstOffset = 0, f_cnt_t srcOffset = 0);

//! @brief Copies data from @a src to @a dst, downmixing from stereo to mono
//!        starting from the given offsets
//! @note If the @a dst subset has more frames than the @a src subset, the additional
//!       frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @param dstOffset the starting frame within @p dst
//! @param srcOffset the starting frame within @p src
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.frames() - dstOffset >= src.frames() - srcOffset
void stereoDownmix(PlanarBufferView<float, 1> dst, PlanarBufferView<const float, 2> src,
	f_cnt_t dstOffset = 0, f_cnt_t srcOffset = 0);

//! @brief Copies data from @a src to @a dst, starting from the given offsets
//! @note If the @a dst subset has more channels or frames than the @a src subset,
//!       the additional channels or frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @param dstOffset the starting frame within @p dst
//! @param srcOffset the starting frame within @p src
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() - dstOffset >= src.frames() - srcOffset
void copy(PlanarBufferView<float> dst, PlanarBufferView<const float> src, f_cnt_t dstOffset, f_cnt_t srcOffset = 0);

//! @brief Copies data from @a src to @a dst
//! @note If @a dst has more channels or frames than @a src, the additional channels or frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copy(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

//! @brief Copies data from @a src to @a dst, starting from the given offsets
//! @note If the @a dst subset has more channels than the @a src subset, the additional channels are zeroed, but
//!       only within the same span of @a dst frames as the other channels that were copied.
//! @note If the @a dst subset has more frames than the @a src subset,
//!       the additional frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @param dstOffset the starting frame within @p dst
//! @param srcOffset the starting frame within @p src
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() - dstOffset >= src.frames() - srcOffset
void copyAndZero(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	f_cnt_t dstOffset = 0, f_cnt_t srcOffset = 0);

//! @brief Copies data from @a src to @a dst
//! @note If @a dst has more channels than @a src, the additional channels are zeroed,
//!       but only the first `src.frames()` frames.
//! @note If @a dst has more frames than @a src, the additional frames are left unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copyAndZero(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

//! Same as @ref copy(PlanarBufferView<float>, PlanarBufferView<const float>, f_cnt_t, f_cnt_t) but
//! applies @ref monoUpmix or @ref stereoDownmix if possible.
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.channels() >= src.channels() || (dst.channels() == 1 && src.channels() == 2)
//! @pre dst.frames() >= src.frames()
void copyWithMonoStereoConversion(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	f_cnt_t dstOffset = 0, f_cnt_t srcOffset = 0);

//! Same as @ref copyAndZero(PlanarBufferView<float>, PlanarBufferView<const float>, f_cnt_t, f_cnt_t) but
//! applies @ref monoUpmix or @ref stereoDownmix if possible.
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.channels() >= src.channels() || (dst.channels() == 1 && src.channels() == 2)
//! @pre dst.frames() >= src.frames()
void copyAndZeroWithMonoStereoConversion(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	f_cnt_t dstOffset = 0, f_cnt_t srcOffset = 0);

/*! \brief Add samples from src to dst */
void add( SampleFrame* dst, const SampleFrame* src, int frames );

/*! \brief Add samples from src to dst */
void add(PlanarBufferView<sample_t> dst, PlanarBufferView<const sample_t> src);

//! @brief Multiply samples from `dst` by `coeff`
void multiply(PlanarBufferView<float> dst, float coeff);

/*! \brief Multiply samples from `dst` by `coeff` */
void multiply(SampleFrame* dst, float coeff, int frames);

//! @brief Add samples from src multiplied by coeffSrc to dst
void addMultiplied(PlanarBufferView<float> dst, PlanarBufferView<const float> src, float coeffSrc);

/*! \brief Add samples from src multiplied by coeffSrc to dst */
void addMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames );

/*! \brief Add samples from src multiplied by coeffSrc to dst, swap inputs */
void addSwappedMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames );

//! @brief Add samples from src multiplied by coeffSrc and coeffSrcBuf to dst
void addMultipliedByBuffer(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	float coeffSrc, const ValueBuffer* coeffSrcBuf);

//! @brief Add samples from src multiplied by coeffSrcBuf1 and coeffSrcBuf2 to dst
void addMultipliedByBuffers(PlanarBufferView<float> dst, PlanarBufferView<const float> src,
	const ValueBuffer* coeffSrcBuf1, const ValueBuffer* coeffSrcBuf2);

/*! \brief Add samples from src multiplied by coeffSrcLeft/coeffSrcRight to dst */
void addMultipliedStereo( SampleFrame* dst, const SampleFrame* src, float coeffSrcLeft, float coeffSrcRight, int frames );

/*! \brief Multiply dst by coeffDst and add samples from src multiplied by coeffSrc */
void multiplyAndAddMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffDst, float coeffSrc, int frames );

/*! \brief Multiply dst by coeffDst and add samples from srcLeft/srcRight multiplied by coeffSrc */
void multiplyAndAddMultipliedJoined( SampleFrame* dst, const sample_t* srcLeft, const sample_t* srcRight, float coeffDst, float coeffSrc, int frames );

} // namespace MixHelpers


} // namespace lmms

#endif // LMMS_MIX_HELPERS_H
