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

#include <vector>

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


//! Whether inplace processing is available - may only be known at runtime
enum class InplaceOption
{
	Dynamic = -1,
	False   =  0,
	True    =  1
};


class NotePlayHandle;

namespace detail
{

template<AudioDataLayout layout, typename SampleT>
struct GetAudioDataType { using type = AudioData<layout, SampleT>; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, SampleFrame> { using type = CoreAudioDataMut; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, const SampleFrame> { using type = CoreAudioData; };


template<AudioDataLayout layout, typename SampleT, InplaceOption inplace>
struct WorkingBuffer
{
	void resize(PluginPinConnector& pinConnector)
	{
		const auto frames = Engine::audioEngine()->framesPerPeriod();
		dataIn.resize(frames * pinConnector.in().channelCount());
		dataOut.resize(frames * pinConnector.out().channelCount());
	}

	std::vector<SampleType<layout, SampleT>> dataIn;
	std::vector<SampleType<layout, SampleT>> dataOut;
};

template<AudioDataLayout layout, typename SampleT>
struct WorkingBuffer<layout, SampleT, InplaceOption::True>
{
	void resize(PluginPinConnector& pinConnector)
	{
		const auto channels = std::max(pinConnector.in().channelCount(), pinConnector.out().channelCount());
		dataInOut.resize(Engine::audioEngine()->framesPerPeriod() * channels);
	}

	std::vector<SampleType<layout, SampleT>> dataInOut;
};

template<InplaceOption inplace>
struct WorkingBuffer<AudioDataLayout::Interleaved, SampleFrame, inplace>
{
	static_assert(inplace == InplaceOption::True);

	void resize(PluginPinConnector&)
	{
		dataInOut.resize(Engine::audioEngine()->framesPerPeriod());
	}

	std::vector<SampleFrame> dataInOut;
};


//! Provides the correct `processImpl` interface for instruments or effects to implement
template<class ChildT, typename BufferT, typename ConstBufferT, InplaceOption inplace>
class AudioProcessingMethod;

//! Instrument specialization
template<typename BufferT, typename ConstBufferT, InplaceOption inplace>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, inplace>
{
protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* nph, ConstBufferT in, BufferT out) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(ConstBufferT in, BufferT out) {}
};

//! Instrument specialization (inplace)
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, InplaceOption::True>
{
protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	virtual void processImpl(NotePlayHandle* nph, BufferT inOut) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(BufferT inOut) {}
};

//! Effect specialization
template<typename BufferT, typename ConstBufferT, InplaceOption inplace>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, inplace>
{
protected:
	/**
	 * The main audio processing method for Effects. Runs when plugin is not bypassed.
	 * If `isInplace() == true`, `in` and `out` are the same buffer.
	 */
	virtual auto processImpl(ConstBufferT in, BufferT out) -> ProcessStatus = 0;
};

//! Effect specialization (inplace)
template<typename BufferT, typename ConstBufferT>
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, InplaceOption::True>
{
protected:
	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(BufferT inOut) -> ProcessStatus = 0;
};


//! Connects the core audio buses to the instrument or effect using the pin connector
template<class ChildT, int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace>
class AudioProcessorImpl
{
	static_assert(always_false_v<ChildT>, "ChildT must be either Instrument or Effect");
};

//! Instrument specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace>
class AudioProcessorImpl<Instrument, numChannelsIn, numChannelsOut, layout, SampleT, inplace>
	: public Instrument
	, public AudioProcessingMethod<Instrument,
		typename detail::GetAudioDataType<layout, SampleT>::type,
		typename detail::GetAudioDataType<layout, const SampleT>::type,
		inplace
	>
{
public:
	AudioProcessorImpl(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			m_buffer.resize(m_pinConnector);
		});
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

protected:
	void playImpl(SampleFrame* inOut) final
	{
		SampleFrame* temp = inOut.data();
		auto busInOut = CoreAudioBusMut{{&temp, 1}, Engine::audioEngine()->framesPerPeriod()};

		if constexpr (inplace == InplaceOption::True)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
			processImpl(m_buffer.dataInOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
		}
		else if constexpr (inplace == InplaceOption::False)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
			processImpl(m_buffer.dataIn, m_buffer.dataOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
		}
		else
		{
			if (m_inplace)
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
				processImpl(m_buffer.dataInOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
			}
			else
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
				processImpl(m_buffer.dataIn, m_buffer.dataOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
			}
		}
	}

	void playNoteImpl(NotePlayHandle* notesToPlay, SampleFrame* inOut) final
	{
		SampleFrame* temp = inOut.data();
		auto busInOut = CoreAudioBusMut{{&temp, 1}, Engine::audioEngine()->framesPerPeriod()};

		if constexpr (inplace == InplaceOption::True)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
			processImpl(notesToPlay, m_buffer.dataInOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
		}
		else if constexpr (inplace == InplaceOption::False)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
			processImpl(notesToPlay, m_buffer.dataIn, m_buffer.dataOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
		}
		else
		{
			if (m_inplace)
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
				processImpl(notesToPlay, m_buffer.dataInOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
			}
			else
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
				processImpl(notesToPlay, m_buffer.dataIn, m_buffer.dataOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
			}
		}
	}

private:
	PluginPinConnector m_pinConnector;
	WorkingBuffer<layout, SampleT, inplace> m_buffer;
};

//! Effect specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace>
class AudioProcessorImpl<Effect, numChannelsIn, numChannelsOut, layout, SampleT, inplace>
	: public Effect
	, public AudioProcessingMethod<Effect,
		typename detail::GetAudioDataType<layout, SampleT>::type,
		typename detail::GetAudioDataType<layout, const SampleT>::type,
		inplace
	>
{
public:
	AudioProcessorImpl(Model* parent = nullptr)
		: m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			m_buffer.resize(m_pinConnector);
		});
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

	//! Returns whether the effect uses inplace processing
	auto isInplace() const -> bool
	{
		if constexpr (inplace == InplaceOption::Dynamic)
		{
			return m_inplace;
		}
		else
		{
			return static_cast<bool>(inplace);
		}
	}

protected:
	auto processAudioBufferImpl(CoreAudioDataMut inOut) -> bool override
	{
		if (isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		SampleFrame* temp = inOut.data();
		auto busInOut = CoreAudioBusMut{{&temp, 1}, inOut.size()};
		ProcessStatus status;

		if constexpr (inplace == InplaceOption::True)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
			status = this->processImpl(m_buffer.dataInOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
		}
		else if constexpr (inplace == InplaceOption::False)
		{
			m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
			status = this->processImpl(m_buffer.dataIn, m_buffer.dataOut);
			m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
		}
		else
		{
			if (m_inplace)
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataInOut);
				status = this->processImpl(m_buffer.dataInOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataInOut, busInOut);
			}
			else
			{
				m_pinConnector.routeToPlugin(busInOut, m_buffer.dataIn);
				status = this->processImpl(m_buffer.dataIn, m_buffer.dataOut);
				m_pinConnector.routeFromPlugin(m_buffer.dataOut, busInOut);
			}
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

	WorkingBuffer<layout, SampleT, inplace> m_buffer;

	/**
	 * TODO: In future, can enable inplace processing at runtime for individual plugins
	 * which advertise support for it
	 */
	bool m_inplace = false;
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
 * @param numChannelsIn The number of plugin input channels, or `DynamicChannelCount` if not known at compile time
 * @param numChannelsOut The number of plugin output channels, or `DynamicChannelCount` if not known at compile time
 * @param layout The audio data layout used by the plugin
 * @param SampleT The plugin's sample type - i.e. float, double, int32_t, `SampleFrame`, etc.
 * @param inplace Inplace processing options - dynamic (runtime), false, or true
 */
template<class ChildT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, InplaceOption inplace>
class AudioPluginInterface
	: public detail::AudioProcessorImpl<
		ChildT,
		numChannelsIn,
		numChannelsOut,
		layout,
		SampleT,
		inplace
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
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::True>;

using MidiInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::True>;

using DefaultEffectPluginInterface = AudioPluginInterface<Effect, 2, 2,
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::True>;

} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_INTERFACE_H
