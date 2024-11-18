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

#include "AudioData.h"
#include "AudioPluginBuffer.h"
#include "Effect.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "PluginPinConnector.h"
#include "SampleFrame.h"

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


class NotePlayHandle;

// TODO: Make AudioPluginBuffer and `processImpl` usage by AudioPluginInterface identical
//       regardless of inplace status? Implementation of buffer would implement potential optimizations.
// TODO: Statically-known channel counts optimization?


namespace detail
{

//! Provides the correct `processImpl` interface for instruments or effects to implement
template<class ChildT, typename BufferT, typename ConstBufferT, bool inplace, bool customWorkingBuffer>
class AudioProcessingMethod;

//! Instrument specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, false, false>
{
protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* nph, ConstBufferT in, BufferT out) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(ConstBufferT in, BufferT out) {}
};

//! Instrument specialization (in-place)
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, true, false>
{
protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* nph, BufferT inOut) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(BufferT inOut) {}
};

//! Instrument specialization (custom working buffers)
template<typename BufferT, typename ConstBufferT, bool inplace>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, inplace, true>
{
protected:
	/**
	 * The main audio processing method for NotePlayHandle-based Instruments.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual void processImpl(NotePlayHandle* nph) {}

	/**
	 * The main audio processing method for MIDI-based Instruments.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual void processImpl() {}
};

//! Effect specialization
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, false, false>
{
protected:
	//! The main audio processing method for Effects. Runs when plugin is not asleep.
	virtual auto processImpl(ConstBufferT in, BufferT out) -> ProcessStatus = 0;
};

//! Effect specialization (in-place)
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, true, false>
{
protected:
	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(BufferT inOut) -> ProcessStatus = 0;
};

//! Effect specialization (custom working buffers)
template<typename BufferT, typename ConstBufferT, bool inplace>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, inplace, true>
{
protected:
	/**
	 * The main audio processing method for Effects. Runs when plugin is not asleep.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual auto processImpl() -> ProcessStatus = 0;
};

//! Connects the core audio buses to the instrument or effect using the pin connector
template<class ChildT, int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, bool inplace, bool customWorkingBuffer>
class AudioProcessorImpl
{
	static_assert(always_false_v<ChildT>, "ChildT must be either Instrument or Effect");
};

//! Instrument specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, bool inplace, bool customWorkingBuffer>
class AudioProcessorImpl<Instrument, numChannelsIn, numChannelsOut, layout, SampleT, inplace, customWorkingBuffer>
	: public Instrument
	, public AudioProcessingMethod<Instrument,
		typename AudioDataTypeSelector<layout, SampleT, numChannelsIn>::type,
		typename AudioDataTypeSelector<layout, const SampleT, numChannelsOut>::type,
		inplace, customWorkingBuffer>
	, public std::conditional_t<customWorkingBuffer,
		AudioPluginBufferInterfaceProvider<layout, SampleT, numChannelsIn, numChannelsOut>,
		AudioPluginBufferDefaultImpl<layout, SampleT, numChannelsIn, numChannelsOut, inplace>>
{
public:
	AudioProcessorImpl(const Plugin::Descriptor* desc, InstrumentTrack* parent = nullptr, const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr, Instrument::Flags flags = Instrument::Flag::NoFlags)
		: Instrument{desc, parent, key, flags}
		, m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			this->bufferInterface()->updateBuffers(
				m_pinConnector.in().channelCount(),
				m_pinConnector.out().channelCount()
			);
		});
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

protected:
	void playImpl(SampleFrame* inOut) final
	{
		const auto bus = CoreAudioBusMut{&inOut, 1, Engine::audioEngine()->framesPerPeriod()};
		auto bufferInterface = this->bufferInterface();

		if constexpr (inplace)
		{
			// Write core to plugin input buffer
			const auto pluginInOut = bufferInterface->inputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginInOut);

			// Process
			if constexpr (customWorkingBuffer) { this->processImpl(); }
			else { this->processImpl(pluginInOut); }

			// Write plugin output buffer to core
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginInOut, bus);
		}
		else
		{
			// Write core to plugin input buffer
			const auto pluginIn = bufferInterface->inputBuffer();
			const auto pluginOut = bufferInterface->outputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginIn);

			// Process
			if constexpr (customWorkingBuffer) { this->processImpl(); }
			else { this->processImpl(pluginIn, pluginOut); }

			// Write plugin output buffer to core
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginOut, bus);
		}
	}

	void playNoteImpl(NotePlayHandle* notesToPlay, SampleFrame* inOut) final
	{
		const auto bus = CoreAudioBusMut{&inOut, 1, Engine::audioEngine()->framesPerPeriod()};
		auto bufferInterface = this->bufferInterface();

		if constexpr (inplace)
		{
			// Write core to plugin input buffer
			const auto pluginInOut = bufferInterface->inputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginInOut);

			// Process
			if constexpr (customWorkingBuffer) { this->processImpl(notesToPlay); }
			else { this->processImpl(notesToPlay, pluginInOut); }

			// Write plugin output buffer to core
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginInOut, bus);
		}
		else
		{
			// Write core to plugin input buffer
			const auto pluginIn = bufferInterface->inputBuffer();
			const auto pluginOut = bufferInterface->outputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginIn);

			// Process
			if constexpr (customWorkingBuffer) { this->processImpl(notesToPlay); }
			else { this->processImpl(notesToPlay, pluginIn, pluginOut); }

			// Write plugin output buffer to core
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginOut, bus);
		}
	}

	auto pinConnector() -> PluginPinConnector* { return &m_pinConnector; }

private:
	PluginPinConnector m_pinConnector;
};

//! Effect specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, bool inplace, bool customWorkingBuffer>
class AudioProcessorImpl<Effect, numChannelsIn, numChannelsOut, layout, SampleT, inplace, customWorkingBuffer>
	: public Effect
	, public AudioProcessingMethod<Effect,
		typename AudioDataTypeSelector<layout, SampleT, numChannelsIn>::type,
		typename AudioDataTypeSelector<layout, const SampleT, numChannelsOut>::type,
		inplace, customWorkingBuffer>
	, public std::conditional_t<customWorkingBuffer,
		AudioPluginBufferInterfaceProvider<layout, SampleT, numChannelsIn, numChannelsOut>,
		AudioPluginBufferDefaultImpl<layout, SampleT, numChannelsIn, numChannelsOut, inplace>>
{
public:
	AudioProcessorImpl(const Plugin::Descriptor* desc, Model* parent = nullptr, const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr)
		: Effect{desc, parent, key}
		, m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			this->bufferInterface()->updateBuffers(
				m_pinConnector.in().channelCount(),
				m_pinConnector.out().channelCount()
			);
		});
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

protected:
	auto processAudioBufferImpl(CoreAudioDataMut inOut) -> bool final
	{
		if (isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		SampleFrame* temp = inOut.data();
		const auto bus = CoreAudioBusMut{&temp, 1, inOut.size()};
		auto bufferInterface = this->bufferInterface();
		ProcessStatus status;

		if constexpr (inplace)
		{
			// Write core to plugin input buffer
			const auto pluginInOut = bufferInterface->inputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginInOut);

			// Process
			if constexpr (customWorkingBuffer) { status = this->processImpl(); }
			else { status = this->processImpl(pluginInOut); }

			// Write plugin output buffer to core; TODO: Apply wet/dry here
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginInOut, bus);
		}
		else
		{
			// Write core to plugin input buffer
			const auto pluginIn = bufferInterface->inputBuffer();
			const auto pluginOut = bufferInterface->outputBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT, numChannelsIn>(bus, pluginIn);

			// Process
			if constexpr (customWorkingBuffer) { status = this->processImpl(); }
			else { status = this->processImpl(pluginIn, pluginOut); }

			// Write plugin output buffer to core; TODO: Apply wet/dry here
			m_pinConnector.routeFromPlugin<layout, SampleT, numChannelsOut>(pluginOut, bus);
		}

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

				checkGate(outSum / inOut.size());
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

	auto pinConnector() -> PluginPinConnector* { return &m_pinConnector; }

private:
	PluginPinConnector m_pinConnector;
};


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
 * TODO C++20: Use class type NTTP for better template parameter ergonomics
 *
 * @param ChildT Either `Instrument` or `Effect`
 * @param numChannelsIn The number of plugin input channels, or `DynamicChannelCount` if unknown at compile time
 * @param numChannelsOut The number of plugin output channels, or `DynamicChannelCount` if unknown at compile time
 * @param layout The audio data layout used by the plugin
 * @param SampleT The plugin's sample type - i.e. float, double, int32_t, `SampleFrame`, etc.
 * @param inplace In-place processing - true (always in-place) or false (dynamic, customizable in audio buffer impl)
 * @param customWorkingBuffer If true, plugin implementation will provide an `AudioPluginBufferInterface`
 */
template<class ChildT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, bool inplace, bool customWorkingBuffer>
class AudioPluginInterface
	: public detail::AudioProcessorImpl<ChildT, numChannelsIn, numChannelsOut,
		layout, SampleT, inplace, customWorkingBuffer>
{
	static_assert(!std::is_const_v<SampleT>);
	static_assert(!std::is_same_v<SampleT, SampleFrame>
		|| ((numChannelsIn == 0 || numChannelsIn == 2) && (numChannelsOut == 0 || numChannelsOut == 2)),
		"Don't use SampleFrame if more than 2 processor channels are needed");

	using Base = detail::AudioProcessorImpl<ChildT, numChannelsIn, numChannelsOut,
		layout, SampleT, inplace, customWorkingBuffer>;

public:
	using Base::AudioProcessorImpl;
};

using DefaultInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, true, false>;

using DefaultEffectPluginInterface = AudioPluginInterface<Effect, 2, 2,
	AudioDataLayout::Interleaved, SampleFrame, true, false>;

} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_INTERFACE_H
