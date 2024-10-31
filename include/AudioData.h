/*
 * AudioData.h - Audio data types
 *
 * Copyright (c) 2024 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#ifndef LMMS_AUDIO_DATA_H
#define LMMS_AUDIO_DATA_H

#include <type_traits>

#include "lmms_basics.h"
#include "Span.h"

namespace lmms
{

//! Conventions for passing audio data
enum class AudioDataLayout
{
	/**
	 * Given:
	 *   - N == Frame count
	 *   - C == Number of channels
	 *   - i == Sample index, where 0 <= i < N
	 *   - `samples` has the type sample_t*
	 *   - `samples` size == N * C
	 */

	/**
	 * Layout where the samples for each channel are interleaved.
	 * i.e. "LRLRLRLR"
	 *
	 * Samples for individual channels can be accessed like this:
	 * - Channel #0 samples: samples[C*i]
	 * - Channel #1 samples: samples[C*i + 1]
	 * - Channel #2 samples: samples[C*i + 2]
	 * - Channel #3 samples: samples[C*i + 3]
	 * - ...
	 */
	Interleaved,

	/**
	 * Layout where all samples for a particular channel are grouped together.
	 * i.e. "LLLLRRRR"
	 *
	 * Samples for individual channels can be accessed like this:
	 * - Channel #0 samples: samples[i]
	 * - Channel #1 samples: samples[1*N + i]
	 * - Channel #2 samples: samples[2*N + i]
	 * - Channel #3 samples: samples[3*N + i]
	 * - ...
	 */
	Split
};


/**
 * A simple type alias for floating point audio data types which documents the data layout.
 *
 * For example, `const InterleavedSampleType<sample_t>*` can be used as a replacement for `const sample_t*`
 * parameters in order to document that the data layout of the audio is interleaved.
 *
 * NOTE: Can add support for integer sample types later
 */
template<AudioDataLayout layout, typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
using SampleType = T;

template<typename T>
using SplitSampleType = SampleType<AudioDataLayout::Split, T>;

template<typename T>
using InterleavedSampleType = SampleType<AudioDataLayout::Interleaved, T>;


/**
 * A non-owning span for passing an audio buffer of a particular layout.
 * This is a simple replacement for pointer and size pairs.
 *
 * All data is contiguous in memory.
 * The size should be equal to the frame count * the channel count.
 *
 * TODO C++23: Use std::mdspan to make accessing samples by channel or frame
 *             easier, consistent regardless of layout, and less error prone.
 */
template<AudioDataLayout layout, typename T>
using AudioData = Span<SampleType<layout, T>>;

template<typename T>
using SplitAudioData = AudioData<AudioDataLayout::Split, T>;

template<typename T>
using InterleavedAudioData = AudioData<AudioDataLayout::Interleaved, T>;


//! Converts between sample types
template<typename Out, typename In>
inline auto convertSample(const In sample) -> Out
{
	if constexpr (std::is_floating_point_v<In> && std::is_floating_point_v<Out>)
	{
		return static_cast<Out>(sample);
	}
	else
	{
		static_assert(always_false_v<In, Out>, "only implemented for floating point samples");
	}
}


} // namespace lmms

#endif // LMMS_AUDIO_DATA_H
