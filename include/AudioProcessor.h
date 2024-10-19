/*
 * AudioProcessor.h - Base class for audio plugins or other classes
 *                    which process audio
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

#ifndef LMMS_AUDIO_PROCESSOR_H
#define LMMS_AUDIO_PROCESSOR_H

#include "PluginPinConnector.h"

namespace lmms
{

enum class ProcessStatus
{
	//! Unconditionally continue processing
	Continue,

	//! Calculate the RMS out sum and call `checkGate` to determine whether to stop processing
	ContinueIfNotQuiet,

	//! Do not continue processing
	Sleep
};

//! Flags that affect the `processImpl` signature
enum class ProcessFlags
{
	None       = 0,
	Instrument = 1 << 0,
	Effect     = 1 << 1,
	MidiBased  = 1 << 2, // only applies to instruments
	Inplace    = 1 << 3, // only applies to effects
};

// NOTE: Not using lmms::Flags because classes are not allowed as NTTP until C++20
constexpr auto operator|(ProcessFlags lhs, ProcessFlags rhs) -> ProcessFlags
{
	return static_cast<ProcessFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

namespace detail
{

struct AudioProcessorTag {};

/**
 * This helper class uses SFINAE to declare a single `processImpl` method based on the template parameters.
 * All other possible `processImpl` signatures are disabled.
 *
 * The `processImpl` method is the main audio processing method that runs when plugin is not asleep.
 */
template<AudioDataLayout layout, typename SampleT, ProcessFlags flags>
class AudioProcessorBase : public AudioProcessorTag
{
	static_assert(!std::is_const_v<SampleT>);
	using F = ProcessFlags;

	static constexpr bool s_sampleFrame = std::is_same_v<SampleT, SampleFrame>;

public:
	using buffer_type = std::conditional_t<s_sampleFrame,
		CoreAudioBufferViewMut,
		AudioBufferView<layout, SampleT>>;

	using const_buffer_type = std::conditional_t<s_sampleFrame,
		CoreAudioBufferView,
		AudioBufferView<layout, const SampleT>>;

	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual auto processImpl(NotePlayHandle* noteToPlay, buffer_type out)
		-> std::enable_if_t<flags == F::Instrument, void> = 0;

	//! The main audio processing method for MIDI-based Instruments
	virtual auto processImpl(buffer_type out)
		-> std::enable_if_t<flags == (F::Instrument | F::MidiBased), void> = 0;

	//! Returns whether `processImpl` uses MIDI or uses NotePlayHandle [For Instruments only]
	constexpr auto isMidiBased() const -> std::enable_if_t<(flags & F::Instrument) != F::None, bool>
	{
		return (flags & F::MidiBased) != F::None;
	}

	//! The main audio processing method for non-inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(const_buffer_type in, buffer_type out)
		-> std::enable_if_t<flags == F::Effect, ProcessStatus> = 0;

	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(buffer_type inOut)
		-> std::enable_if_t<flags == (F::Effect | F::Inplace), ProcessStatus> = 0;

	/**
	 * Optional method that runs when an Effect is sleeping (not enabled,
	 * not running, not in the Okay state, or in the Don't Run state)
	 */
	virtual auto processBypassedImpl() -> std::enable_if_t<(flags & F::Effect) != F::None, void>
	{
	}

	//! Returns whether `processImpl` uses inplace processing [For Effects only]
	constexpr auto isInplace() const -> std::enable_if_t<(flags & F::Effect) != F::None, bool>
	{
		return (flags & F::Inplace) != F::None;
	}
};

} // namespace detail



template<class ParentT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, ProcessFlags flags>
class AudioProcessor
	: public detail::AudioProcessorBase<layout, SampleT, flags>
{
public:
	AudioProcessor(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
	}

	//! Returns true if audio was processed and should continue being processed [For Effects only]
	auto processAudioBuffer(SampleFrame* buf, const fpp_t frames)
		-> std::enable_if_t<(flags & F::Effect) != F::None, bool>
	{
		if (static_cast<const ParentT*>(this)->isBypassed())
		{
			this->processBypassedImpl();
			return false;
		}

		ProcessFlags status;

		// TODO: Use Pin Connector here

		status = this->processImpl(buf, frames);
		switch (status)
		{
			case ProcessStatus::Continue:
				break;
			case ProcessStatus::ContinueIfNotQuiet:
			{
				double outSum = 0.0;
				for (std::size_t idx = 0; idx < frames; ++idx)
				{
					outSum += buf[idx].sumOfSquaredAmplitudes();
				}

				static_cast<const ParentT*>(this)->checkGate(outSum / frames);
				break;
			}
			case ProcessStatus::Sleep:
				return false;
			default:
				break;
		}

		return static_cast<const ParentT*>(this)->isRunning();
	}

	auto pinConnector() const -> const PluginPinConnector& { return m_pinConnector; }

	auto channelsIn() const -> int { return m_pinConnector.in().channelCount(); }
	auto channelsOut() const -> int { return m_pinConnector.out().channelCount(); }


protected:



private:
	PluginPinConnector m_pinConnector;
};

template<class ParentT>
using DefaultInstrumentProcessor = AudioProcessor<ParentT, 0, 2, AudioDataLayout::Interleaved, SampleFrame,
	ProcessFlags::Instrument>;

template<class ParentT>
using MidiInstrumentProcessor = AudioProcessor<ParentT, 0, 2, AudioDataLayout::Interleaved, SampleFrame,
	ProcessFlags::Instrument | ProcessFlags::MidiBased>;

template<class ParentT>
using DefaultEffectProcessor = AudioProcessor<ParentT, 2, 2, AudioDataLayout::Interleaved, SampleFrame,
	ProcessFlags::Effect | ProcessFlags::Inplace>;

} // namespace lmms

#endif // LMMS_AUDIO_PROCESSOR_H
