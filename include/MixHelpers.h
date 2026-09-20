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
//! @note If the @a dst subset has more channels or frames than the @a src subset,
//!       the additional channels or frames are unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.frames() >= src.frames()
void monoUpmix(PlanarBufferView<float, 2> dst, PlanarBufferView<const float, 1> src);

//! @brief Copies data from @a src to @a dst, upmixing from mono to stereo
//! @note If the @a dst subset has more channels or frames than the @a src subset,
//!       the additional channels or frames are zeroed.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.frames() >= src.frames()
void monoUpmixAndZero(PlanarBufferView<float, 2> dst, PlanarBufferView<const float, 1> src);

//! @brief Copies data from @a src to @a dst, starting from the given offsets
//! @note If the @a dst subset has more channels or frames than the @a src subset,
//!       the additional channels or frames are unmodified.
//! @param dst the output buffer
//! @param dstOffset the starting frame within @p dst
//! @param src the input buffer
//! @param srcOffset the starting frame within @p src
//! @pre dstOffset < dst.frames()
//! @pre srcOffset < src.frames()
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() - dstOffset >= src.frames() - srcOffset
void copy(PlanarBufferView<float> dst, f_cnt_t dstOffset, PlanarBufferView<const float> src, f_cnt_t srcOffset);

//! @brief Copies data from @a src to @a dst
//! @note If @a dst has more channels or frames than @a src, the additional channels or frames are unmodified.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copy(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

//! @brief Copies data from @a src to @a dst
//! @note If @a dst has more channels or frames than @a src, the additional channels or frames are zeroed.
//! @param dst the output buffer
//! @param src the input buffer
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copyAndZero(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

//! Same as @ref copy(PlanarBufferView<float>, PlanarBufferView<const float>) but with
//! one exception: When @a src has 1 channel and @a dst has 2, the data from @a src will be
//! upmixed from mono to stereo.
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copyWithMonoUpmix(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

//! Same as @ref copyAndZero(PlanarBufferView<float>, PlanarBufferView<const float>) but with
//! one exception: When @a src has 1 channel and @a dst has 2, the data from @a src will be
//! upmixed from mono to stereo.
//! @pre dst.channels() >= src.channels()
//! @pre dst.frames() >= src.frames()
void copyAndZeroWithMonoUpmix(PlanarBufferView<float> dst, PlanarBufferView<const float> src);

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
