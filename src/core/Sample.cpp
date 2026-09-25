/*
 * Sample.cpp
 *
 * Copyright (c) 2025 saker <sakertooth@gmail.com>
 * Copyright (c) 2026 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#include "Sample.h"

#include "MixHelpers.h"

namespace lmms {

Sample::Sample(const SampleFrame* data, f_cnt_t numFrames, int sampleRate)
	: m_buffer(std::make_shared<SampleBuffer>(
		std::span{data, numFrames}, SampleImportModification::Unmodified, sampleRate))
	, m_startFrame(0)
	, m_endFrame(m_buffer->frames())
	, m_loopStartFrame(0)
	, m_loopEndFrame(m_buffer->frames())
{
}

Sample::Sample(std::shared_ptr<const SampleBuffer> buffer)
	: m_buffer(buffer)
	, m_startFrame(0)
	, m_endFrame(m_buffer->frames())
	, m_loopStartFrame(0)
	, m_loopEndFrame(m_buffer->frames())
{
}

Sample::Sample(const Sample& other)
	: m_buffer(other.m_buffer)
	, m_startFrame(other.startFrame())
	, m_endFrame(other.endFrame())
	, m_loopStartFrame(other.loopStartFrame())
	, m_loopEndFrame(other.loopEndFrame())
	, m_amplification(other.amplification())
	, m_frequency(other.frequency())
	, m_reversed(other.reversed())
{
}

Sample::Sample(Sample&& other) noexcept
	: m_buffer(std::move(other.m_buffer))
	, m_startFrame(other.startFrame())
	, m_endFrame(other.endFrame())
	, m_loopStartFrame(other.loopStartFrame())
	, m_loopEndFrame(other.loopEndFrame())
	, m_amplification(other.amplification())
	, m_frequency(other.frequency())
	, m_reversed(other.reversed())
{
}

auto Sample::operator=(const Sample& other) -> Sample&
{
	m_buffer = other.m_buffer;
	m_startFrame = other.startFrame();
	m_endFrame = other.endFrame();
	m_loopStartFrame = other.loopStartFrame();
	m_loopEndFrame = other.loopEndFrame();
	m_amplification = other.amplification();
	m_frequency = other.frequency();
	m_reversed = other.reversed();

	return *this;
}

auto Sample::operator=(Sample&& other) noexcept -> Sample&
{
	m_buffer = std::move(other.m_buffer);
	m_startFrame = other.startFrame();
	m_endFrame = other.endFrame();
	m_loopStartFrame = other.loopStartFrame();
	m_loopEndFrame = other.loopEndFrame();
	m_amplification = other.amplification();
	m_frequency = other.frequency();
	m_reversed = other.reversed();

	return *this;
}

auto Sample::play(PlanarBufferSpan<float> dst, PlaybackState* state,
	Loop loop, double ratio) const -> bool
{
	if (!m_buffer || m_buffer->empty()) { return false; }

	state->m_frameIndex = std::max<int>(m_startFrame, state->m_frameIndex);

	const auto sampleRateRatio = static_cast<double>(Engine::audioEngine()->outputSampleRate()) / m_buffer->sampleRate();
	const auto freqRatio = frequency() / DefaultBaseFreq;
	state->m_resampler.setRatio(sampleRateRatio * freqRatio * ratio);

	// TODO: These kind of playback pipelines/graphs are repeated within other parts of the codebase that work with
	// audio samples. We should find a way to unify this but the right abstraction is not so clear yet.
	f_cnt_t numFrames = dst.frames();
	while (numFrames > 0)
	{
		if (state->m_bufferSpan.empty())
		{
			const auto rendered = render(state, loop);
			state->m_bufferSpan = PlanarBufferSpan{state->m_buffer.allBuffers().first(rendered)};
		}

		const auto [inputFramesUsed, outputFramesGenerated] = state->m_resampler.process(
			state->m_bufferSpan,
			dst
		);

		if (inputFramesUsed == 0 && outputFramesGenerated == 0)
		{
			MixHelpers::zero(dst);
			break;
		}

		state->m_bufferSpan = state->m_bufferSpan.subspan(inputFramesUsed);
		dst = dst.subspan(outputFramesGenerated);
		numFrames -= outputFramesGenerated;
	}

	return numFrames < Engine::audioEngine()->framesPerPeriod(); // TODO: Is this right?
}

f_cnt_t Sample::render(PlaybackState* state, Loop loop) const
{
	const auto dst = state->m_buffer.allBuffers();
	const auto src = m_buffer->data();

	assert(!src.empty());
	assert(src.channels() == dst.channels());
	assert(m_endFrame <= src.frames()); // ???
	assert(m_loopEndFrame <= src.frames()); // ???

	using CopyFunction = auto(*)(
		float* const*       dst, f_cnt_t dstBegin, f_cnt_t dstEnd,
		const float* const* src, f_cnt_t srcBegin, f_cnt_t srcEnd, f_cnt_t& srcReadPos,
		ch_cnt_t channels, float amp) -> f_cnt_t;

	constexpr CopyFunction copyForwardRead = +[](
		float* const*       dst, f_cnt_t dstBegin, f_cnt_t dstEnd,
		const float* const* src, f_cnt_t srcBegin, f_cnt_t srcEnd, f_cnt_t& srcReadPos,
		ch_cnt_t channels, float amp) -> f_cnt_t
	{
		assert(srcBegin <= srcReadPos);
		assert(srcReadPos < srcEnd);
		(void)srcBegin;

		const auto maxWriteAmount = dstEnd - dstBegin;
		const auto maxReadAmount = srcEnd - srcReadPos;
		const auto framesWritten = std::min(maxWriteAmount, maxReadAmount);

		for (ch_cnt_t ch = 0; ch < channels; ++ch)
		{
			float* const       dstPtr = dst[ch] + dstBegin;
			const float* const srcPtr = src[ch] + srcReadPos;
			for (f_cnt_t frame = 0; frame < framesWritten; ++frame)
			{
				dstPtr[frame] = srcPtr[frame] * amp;
			}
		}

		srcReadPos += framesWritten;

		return framesWritten;
	};

	constexpr CopyFunction copyBackwardRead = +[](
		float* const*       dst, f_cnt_t dstBegin, f_cnt_t dstEnd,
		const float* const* src, f_cnt_t srcBegin, f_cnt_t srcEnd, f_cnt_t& srcReadPos,
		ch_cnt_t channels, float amp) -> f_cnt_t
	{
		assert(srcBegin <= srcReadPos);
		assert(srcReadPos < srcEnd);
		(void)srcEnd;

		const auto maxWriteAmount = dstEnd - dstBegin;
		const auto maxReadAmount = srcReadPos - srcBegin + 1;
		const auto framesWritten = std::min(maxWriteAmount, maxReadAmount);

		for (ch_cnt_t ch = 0; ch < channels; ++ch)
		{
			float* const       dstPtr = dst[ch] + dstBegin;
			const float* const srcPtr = src[ch] + srcReadPos;
			for (f_cnt_t frame = 0; frame < framesWritten; ++frame)
			{
				dstPtr[frame] = *(srcPtr - frame) * amp;
			}
		}

		// NOTE: Since we're using unsigned frame counts, this may underflow to
		//       static_cast<f_cnt_t>(-1) for the reverse-past-the-end sentinel.
		srcReadPos -= framesWritten;

		return framesWritten;
	};

	// Call like this: copy[state->m_backwards](...)
	std::array<CopyFunction, 2> copy = m_reversed.load()
		? std::array{copyBackwardRead, copyForwardRead/* <-- might need further restrictions */}
		: std::array{copyForwardRead, copyBackwardRead};

	switch (loop)
	{
		case Loop::Off:
		{
			// Snap to a valid position
			state->m_frameIndex = std::clamp(state->m_frameIndex, static_cast<f_cnt_t>(0), src.frames() - 1);

			return copy[state->m_backwards](
				dst.data(), 0, dst.frames(),
				src.data(), 0, src.frames(), state->m_frameIndex,
				dst.channels(), m_amplification
			);
		}
		case Loop::On:
		{
			const auto dstFrames = dst.frames();
			const auto loopStartFrame = m_loopStartFrame.load();
			const auto loopEndFrame = m_loopEndFrame.load();
			const auto backwards = state->m_backwards;

			f_cnt_t framesWritten = 0;
			f_cnt_t framesLeft = dstFrames;
			while (framesLeft > 0)
			{
				// Loop wraparound
				if (state->m_frameIndex < loopStartFrame || state->m_frameIndex >= loopEndFrame)
				{
					state->m_frameIndex = backwards ? loopEndFrame - 1 : loopStartFrame;
				}

				// Copy contiguous section
				const auto written = copy[backwards](
					dst.data(), framesWritten, dstFrames,
					src.data(), loopStartFrame, loopEndFrame, state->m_frameIndex,
					dst.channels(), m_amplification
				);

				framesWritten += written;
				framesLeft -= written;
			}
			break;
		}
		case Loop::PingPong:
		{
			const auto dstFrames = dst.frames();
			const auto loopStartFrame = m_loopStartFrame.load();
			const auto loopEndFrame = m_loopEndFrame.load();

			f_cnt_t framesWritten = 0;
			f_cnt_t framesLeft = dstFrames;
			while (framesLeft > 0)
			{
				// Loop ping-pong
				if (state->m_backwards)
				{
					if (state->m_frameIndex < loopStartFrame || state->m_frameIndex == static_cast<f_cnt_t>(-1))
					{
						state->m_frameIndex = loopStartFrame;
						state->m_backwards = false;
					}
					else if (state->m_frameIndex >= loopEndFrame)
					{
						state->m_frameIndex = loopEndFrame - 1;
					}
				}
				else
				{
					if (state->m_frameIndex < loopStartFrame || state->m_frameIndex == static_cast<f_cnt_t>(-1))
					{
						state->m_frameIndex = loopStartFrame;
					}
					else if (state->m_frameIndex >= loopEndFrame)
					{
						state->m_frameIndex = loopEndFrame - 1;
						state->m_backwards = true;
					}
				}

				// Copy contiguous section
				const auto written = copy[state->m_backwards](
					dst.data(), framesWritten, dstFrames,
					src.data(), loopStartFrame, loopEndFrame, state->m_frameIndex,
					dst.channels(), m_amplification
				);

				framesWritten += written;
				framesLeft -= written;
			}
			break;
		}
		default:
			assert(false);
			return 0;
	}

	return dst.frames();
}

auto Sample::sampleDuration() const -> std::chrono::milliseconds
{
	const auto numFrames = endFrame() - startFrame();
	const auto duration = numFrames / static_cast<float>(m_buffer->sampleRate()) * 1000;
	return std::chrono::milliseconds{static_cast<int>(duration)};
}

void Sample::setAllPointFrames(f_cnt_t startFrame, f_cnt_t endFrame, f_cnt_t loopStartFrame, f_cnt_t loopEndFrame)
{
	setStartFrame(startFrame);
	setEndFrame(endFrame);
	setLoopStartFrame(loopStartFrame);
	setLoopEndFrame(loopEndFrame);
}

} // namespace lmms
