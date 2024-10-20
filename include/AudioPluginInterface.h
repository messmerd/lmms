/*
 * AudioPluginInterface.h - Interface for all audio plugins
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

#ifndef LMMS_AUDIO_PLUGIN_INTERFACE_H
#define LMMS_AUDIO_PLUGIN_INTERFACE_H

#include "Effect.h"
#include "Instrument.h"
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

//! Compile-time customizations affecting the `processImpl` method
enum class ProcessFlags
{
	None       = 0,
	MidiBased  = 1 << 0, // only applies to instruments
	Inplace    = 1 << 1, // only applies to effects
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


//! Can dynamic cast a NPH-based `Instrument*` to this for NPH-specific methods
class NotePlayHandleInstrumentInterface
{
public:
	/**
	 * Needed for deleting plugin-specific-data of a note - plugin has to
	 * cast void-ptr so that the plugin-data is deleted properly
	 * (call of dtor if it's a class etc.)
	 */
	virtual void deleteNotePluginData(NotePlayHandle* noteToPlay) = 0;

	/**
	 * Get number of sample-frames that should be used when playing beat
	 * (note with unspecified length)
	 * Per default this function returns 0. In this case, channel is using
	 * the length of the longest envelope (if one active).
	 */
	virtual auto beatLen(NotePlayHandle* nph) const -> f_cnt_t { return 0; }
};

//! Can dynamic cast a MIDI-based `Instrument*` to this for MIDI-specific methods
class MidiInstrumentInterface
{
public:
	// Receives all incoming MIDI events; Return true if event was handled.
	virtual bool handleMidiEvent(const MidiEvent&, const TimePos& = TimePos(), f_cnt_t offset = 0) = 0;
};


namespace detail
{

//! Provides the correct `processImpl` interface for instruments or effects to implement
template<class ChildT, typename BufferT, typename ConstBufferT, ProcessFlags flags>
class AudioProcessingMethod;

//! NotePlayHandle-based Instrument specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, ProcessFlags::None>
	: public NotePlayHandleInstrumentInterface
{
protected:
	// TODO: Use BufferT parameter instead? (Any need for the frame count?)

	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* nph, typename BufferT::pointer workingBuffer) = 0;
};

//! MIDI-based Instrument specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, ProcessFlags::MidiBased>
	: public MidiInstrumentInterface
{
protected:
	// TODO: Use BufferT parameter instead? (Any need for the frame count?)

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(typename BufferT::pointer out) = 0;
};

//! Non-inplace Effect specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, ProcessFlags::None>
{
protected:
	//! The main audio processing method for non-inplace Effects. Runs when plugin is not bypassed.
	virtual auto processImpl(ConstBufferT in, BufferT out) -> ProcessStatus = 0;
};

//! Inplace Effect specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, ProcessFlags::Inplace>
{
protected:
	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(BufferT inOut) -> ProcessStatus = 0;
};


//! Connects the core audio buses to the instrument or effect using the pin connector
template<class ChildT, class ProcessorInterfaceT, int numChannelsIn, int numChannelsOut, ProcessFlags flags>
class AudioProcessorImpl
{
	static_assert(always_false_v<ChildT>, "ChildT must be either Instrument or Effect");
};

//! Instrument specialization
template<class ProcessorInterfaceT, int numChannelsIn, int numChannelsOut, ProcessFlags flags>
class AudioProcessorImpl<Instrument, ProcessorInterfaceT, numChannelsIn, numChannelsOut, flags>
	: public Instrument
	, public ProcessorInterfaceT
{
public:
	AudioProcessorImpl(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

	auto isMidiBased() const -> bool final
	{
		return (flags & ProcessFlags::MidiBased) != ProcessFlags::None;
	}

protected:
	void playImpl(SampleFrame* workingBuffer, NotePlayHandle* notesToPlay) override
	{
		// TODO:
		if constexpr ((flags & ProcessFlags::MidiBased) != ProcessFlags::None)
		{

		}
		else
		{

		}
	}

private:
	PluginPinConnector m_pinConnector;
};

//! Effect specialization
template<class ProcessorInterfaceT, int numChannelsIn, int numChannelsOut, ProcessFlags flags>
class AudioProcessorImpl<Effect, ProcessorInterfaceT, numChannelsIn, numChannelsOut, flags>
	: public Effect
	, public ProcessorInterfaceT
{
public:
	AudioProcessorImpl(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

	//! Returns whether the effect uses inplace processing
	auto isInplace() const -> bool final
	{
		return (flags & ProcessFlags::Inplace) != ProcessFlags::None;
	}

protected:
	auto processAudioBufferImpl(CoreAudioDataMut inOut) -> bool override
	{
		if (isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		ProcessStatus status;

		// TODO: Use Pin Connector here

		status = this->processImpl(inOut);
		switch (status)
		{
			case ProcessStatus::Continue:
				break;
			case ProcessStatus::ContinueIfNotQuiet:
			{
				double outSum = 0.0;
				for (const SampleFrame& frame : inOut)
				{
					outSum += frame.sumOfSquaredAmplitudes();
				}

				checkGate(outSum / frames);
				break;
			}
			case ProcessStatus::Sleep:
				return false;
			default:
				break;
		}

		return isRunning();
	}

	/**
	 * Optional method that runs when an effect is asleep (not enabled,
	 * not running, not in the Okay state, or in the Don't Run state)
	 */
	virtual void processBypassedImpl()
	{
	}

private:
	PluginPinConnector m_pinConnector;
};


template<AudioDataLayout layout, typename SampleT>
struct GetAudioDataType { using type = AudioData<layout, SampleT>; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, SampleFrame> { using type = CoreAudioDataMut; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, const SampleFrame> { using type = CoreAudioData; };

} // namespace detail


/**
 * AudioPluginInterface is the bridge connecting an Instrument/Effect base class used by the Core
 * with its derived class used by a plugin implementation.
 *
 * Pin connector routing and other common tasks are handled here to allow plugin implementations
 * to focus solely on audio processing or generation without needing to worry about how their plugin
 * interfaces with LMMS Core.
 *
 * This design allows for some compile-time customization over aspects of the plugin implementation
 * such as the number of in/out channels and the audio data layout, so plugin developers can
 * implement their plugin in whatever way works best for them. All the mapping from their plugin to/from
 * LMMS Core is handled here, at compile-time where possible for best performance.
 *
 * A `processImpl` interface method is provided which must be implemented by the plugin implementation.
 *
 * @param ChildT Either `Instrument` or `Effect`
 * @param numChannelsIn The number of plugin input channels, or `DynamicChannelCount` if not known at compile time
 * @param numChannelsOut The number of plugin output channels, or `DynamicChannelCount` if not known at compile time
 * @param layout The audio data layout used by the plugin
 * @param SampleT The plugin's sample type - i.e. float, double, int32_t, `SampleFrame`, etc.
 * @param flags Other compile-time customizations for the plugin
 */
template<class ChildT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, ProcessFlags flags>
class AudioPluginInterface
	: public detail::AudioProcessorImpl<
		ChildT,
		detail::AudioProcessingMethod<
			ChildT,
			typename detail::GetAudioDataType<layout, SampleT>::type,
			typename detail::GetAudioDataType<layout, const SampleT>::type,
			flags>,
		numChannelsIn,
		numChannelsOut,
		flags
	>
{
	static_assert(!std::is_const_v<SampleT>);
	static_assert(!std::is_same_v<SampleT, SampleFrame>
		|| ((numChannelsIn == 0 || numChannelsIn == 2) && (numChannelsOut == 0 || numChannelsOut == 2)),
		"Don't use SampleFrame if more than 2 processor channels are needed");

public:
	AudioProcessor(Model* parent = nullptr)
		: AudioProcessorImpl{parent}
	{
	}

	auto channelsIn() const -> int { return pinConnector()->in().channelCount(); }
	auto channelsOut() const -> int { return pinConnector()->out().channelCount(); }

	// ...
};

using DefaultInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, ProcessFlags::None>;

using MidiInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, ProcessFlags::MidiBased>;

using DefaultEffectPluginInterface = AudioPluginInterface<Effect, 2, 2,
	AudioDataLayout::Interleaved, SampleFrame, ProcessFlags::Inplace>;

} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_INTERFACE_H
