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

// NOTE: Not using lmms::Flags because classes are not allowed as NTTP until C++20
constexpr auto operator&(ProcessFlags lhs, ProcessFlags rhs) -> ProcessFlags
{
	return static_cast<ProcessFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

class NotePlayHandle;

namespace detail
{

struct AudioProcessorTag {};

// Primary template
template<typename BufferT, typename ConstBufferT, ProcessFlags flags>
class AudioProcessorInterface;

template<bool midiBased>
class InstrumentProcessorInterface
{
public:
	//! Returns whether the instrument is MIDI-based or NotePlayHandle-based
	constexpr auto isMidiBased() const -> bool { return midiBased; }
};

// NotePlayHandle-based Instruments
template<typename BufferT, typename ConstBufferT>
class AudioProcessorInterface<BufferT, ConstBufferT, ProcessFlags::Instrument>
	: public InstrumentProcessorInterface<false>
{
protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* noteToPlay, BufferT out) = 0;
};

// MIDI-based Instruments
template<typename BufferT, typename ConstBufferT>
struct AudioProcessorInterface<BufferT, ConstBufferT, ProcessFlags::Instrument | ProcessFlags::MidiBased>
	: public InstrumentProcessorInterface<true>
{
protected:
	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(BufferT out) = 0;
};

template<bool inplace>
class EffectProcessorInterface
{
public:
	//! Returns whether the effect uses inplace processing
	constexpr auto isInplace() const -> bool { return inplace; }

protected:
	/**
	 * Optional method that runs when an effect is asleep (not enabled,
	 * not running, not in the Okay state, or in the Don't Run state)
	 */
	virtual void processBypassedImpl()
	{
	}
};

// Non-inplace Effects
template<typename BufferT, typename ConstBufferT>
class AudioProcessorInterface<BufferT, ConstBufferT, ProcessFlags::Effect>
	: public EffectProcessorInterface<false>
{
protected:
	//! The main audio processing method for non-inplace Effects. Runs when plugin is not bypassed.
	virtual auto processImpl(ConstBufferT in, BufferT out) -> ProcessStatus = 0;
};

// Inplace Effects
template<typename BufferT, typename ConstBufferT>
class AudioProcessorInterface<BufferT, ConstBufferT, ProcessFlags::Effect | ProcessFlags::Inplace>
	: public EffectProcessorInterface<true>
{
protected:
	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(BufferT inOut) -> ProcessStatus = 0;
};

} // namespace detail


template<class ParentT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, ProcessFlags flags>
class AudioProcessor
	: public detail::AudioProcessorInterface<
		AudioBufferView<layout, SampleT>,
		AudioBufferView<layout, const SampleT>,
		flags>
	, public detail::AudioProcessorTag
{
	static_assert(!std::is_const_v<SampleT>);

public:
	using buffer_type = AudioBufferView<layout, SampleT>;
	using const_buffer_type = AudioBufferView<layout, const SampleT>;

	AudioProcessor(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
	}

	/**
	 * Returns true if audio was processed and should continue being processed [For Effects only]
	 *
	 * The parent class must define `isSleeping`, `checkGate`, and `isRunning`.
	 */
	template<class T = ParentT>
	auto processAudioBuffer(SampleFrame* buf, const fpp_t frames)
		-> std::enable_if_t<(flags & ProcessFlags::Effect) != ProcessFlags::None, bool>
	{
		if (static_cast<const ParentT*>(this)->isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		ProcessStatus status;

		// TODO: Use Pin Connector here

		status = this->processImpl(CoreAudioBufferViewMut{buf, frames});
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

				static_cast<ParentT*>(this)->checkGate(outSum / frames);
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
