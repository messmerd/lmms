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
#include "InstrumentTrack.h"
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
	Never,
	Dynamic, // TODO: CLAP allows an in/out channel pair to be marked in-place
	Always
};

// ProcessFlags:
//   Midi vs NPH (leave for future PR?)
//   custom working buffer?


class NotePlayHandle;

namespace detail
{

// TODO: Make AudioPluginBuffer and `processImpl` usage by AudioPluginInterface identical
//       regardless of inplace status? Implementation of buffer would implement potential optimizations.
template<AudioDataLayout layout, typename SampleT>
class AudioPluginBufferInterface
{
protected:
	virtual auto getWorkingBufferIn() -> AudioData<layout, SampleT> = 0;
	virtual auto getWorkingBufferOut() -> AudioData<layout, SampleT> = 0;
	virtual void resizeWorkingBuffers(int channelsIn, int channelsOut) = 0;
};

template<>
class AudioPluginBufferInterface<AudioDataLayout::Interleaved, SampleFrame>
{
protected:
	virtual auto getWorkingBufferIn() -> CoreAudioDataMut = 0;
	virtual auto getWorkingBufferOut() -> CoreAudioDataMut = 0;
	virtual void resizeWorkingBuffers(int channelsIn, int channelsOut) = 0;
};


template<AudioDataLayout layout, typename SampleT, bool inplace>
class AudioPluginBufferDefaultImpl;

template<AudioDataLayout layout, typename SampleT>
class AudioPluginBufferDefaultImpl<layout, SampleT, false>
	: public AudioPluginBufferInterface<layout, SampleT>
{
public:
	auto getWorkingBufferIn() -> AudioData<layout, SampleT> final
	{
		return AudioData<layout, SampleT>{m_bufferIn.data(), m_bufferIn.size()};
	}

	auto getWorkingBufferOut() -> AudioData<layout, SampleT> final
	{
		return AudioData<layout, SampleT>{m_bufferOut.data(), m_bufferOut.size()};
	}

	void resizeWorkingBuffers(int channelsIn, int channelsOut) final
	{
		const auto frames = Engine::audioEngine()->framesPerPeriod();
		if (channelsIn >= 0) { m_bufferIn.resize(frames * channelsIn); }
		if (channelsOut >= 0) { m_bufferOut.resize(frames * channelsOut); }
	}

private:
	std::vector<SampleType<layout, SampleT>> m_bufferIn;
	std::vector<SampleType<layout, SampleT>> m_bufferOut;
};

template<AudioDataLayout layout, typename SampleT>
class AudioPluginBufferDefaultImpl<layout, SampleT, true>
	: public AudioPluginBufferInterface<layout, SampleT>
{
public:
	auto getWorkingBufferIn() -> AudioData<layout, SampleT> final
	{
		return AudioData<layout, SampleT>{m_buffer.data(), m_buffer.size()};
	}

	auto getWorkingBufferOut() -> AudioData<layout, SampleT> final
	{
		return getWorkingBufferIn();
	}

	void resizeWorkingBuffers(int channelsIn, int channelsOut) final
	{
		const auto channels = std::max(channelsIn, channelsOut);
		if (channels < 0) { return; }
		m_buffer.resize(Engine::audioEngine()->framesPerPeriod() * channels);
	}

private:
	std::vector<SampleType<layout, SampleT>> m_buffer; // inOut
};

template<>
class AudioPluginBufferDefaultImpl<AudioDataLayout::Interleaved, SampleFrame, true>
	: public AudioPluginBufferInterface<AudioDataLayout::Interleaved, SampleFrame>
{
public:
	auto getWorkingBufferIn() -> CoreAudioDataMut final
	{
		return CoreAudioDataMut{m_buffer.data(), m_buffer.size()};
	}

	auto getWorkingBufferOut() -> CoreAudioDataMut final
	{
		return getWorkingBufferIn();
	}

	void resizeWorkingBuffers(int channelsIn, int channelsOut) final
	{
		assert(channelsIn == 2);
		assert(channelsOut == 2);
		(void)channelsIn;
		(void)channelsOut;
		m_buffer.resize(Engine::audioEngine()->framesPerPeriod());
	}

private:
	std::vector<SampleFrame> m_buffer; // inOut
};


// FIXME: My std::span implementation fails to convert Span<T> to Span<const T>, so this is a workaround
template<class T, std::size_t extent>
constexpr inline auto constSpan(const Span<T, extent>& s)
{
	return Span<std::add_const_t<T>, extent>{s.data(), s.size()};
}


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
class AudioProcessingMethod<Instrument, BufferT, ConstBufferT, InplaceOption::Always>
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
class AudioProcessingMethod<Effect, BufferT, ConstBufferT, InplaceOption::Always>
{
protected:
	//! The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	virtual auto processImpl(BufferT inOut) -> ProcessStatus = 0;
};


//! Connects the core audio buses to the instrument or effect using the pin connector
template<class ChildT, int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace, bool customWorkingBuffer>
class AudioProcessorImpl
{
	static_assert(always_false_v<ChildT>, "ChildT must be either Instrument or Effect");
};

//! Instrument specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace, bool customWorkingBuffer>
class AudioProcessorImpl<Instrument, numChannelsIn, numChannelsOut, layout, SampleT, inplace, customWorkingBuffer>
	: public Instrument
	, public AudioProcessingMethod<Instrument,
		typename GetAudioDataType<layout, SampleT>::type,
		typename GetAudioDataType<layout, const SampleT>::type,
		inplace>
	, public std::conditional_t<customWorkingBuffer,
		AudioPluginBufferInterface<layout, SampleT>,
		AudioPluginBufferDefaultImpl<layout, SampleT, inplace == InplaceOption::Always>>
{
public:
	AudioProcessorImpl(const Plugin::Descriptor* desc, InstrumentTrack* parent = nullptr, const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr, Instrument::Flags flags = Instrument::Flag::NoFlags)
		: Instrument{desc, parent, key, flags}
		, m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			this->resizeWorkingBuffers(m_pinConnector.in().channelCount(), m_pinConnector.out().channelCount());
		});
		this->resizeWorkingBuffers(m_pinConnector.in().channelCount(), m_pinConnector.out().channelCount());
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

protected:
	void playImpl(SampleFrame* inOut) final
	{
		auto busConst = CoreAudioBus{{&inOut, 1}, Engine::audioEngine()->framesPerPeriod()};
		auto busMut = CoreAudioBusMut{{&inOut, 1}, Engine::audioEngine()->framesPerPeriod()};

		if constexpr (inplace == InplaceOption::Always)
		{
			auto workingBuffer = this->getWorkingBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBuffer);
			this->processImpl(workingBuffer);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBuffer), busMut);
		}
		else if constexpr (inplace == InplaceOption::Never)
		{
			auto workingBufferIn = this->getWorkingBufferIn();
			auto workingBufferOut = this->getWorkingBufferOut();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBufferIn);
			this->processImpl(constSpan(workingBufferIn), workingBufferOut);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBufferOut), busMut);
		}
		else
		{
			if (m_inplace)
			{
				auto workingBuffer = this->getWorkingBufferIn();
				m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBuffer);
				this->processImpl(workingBuffer, workingBuffer);
				m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBuffer), busMut);
			}
			else
			{
				auto workingBufferIn = this->getWorkingBufferIn();
				auto workingBufferOut = this->getWorkingBufferOut();
				m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBufferIn);
				this->processImpl(constSpan(workingBufferIn), workingBufferOut);
				m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBufferOut), busMut);
			}
		}
	}

	void playNoteImpl(NotePlayHandle* notesToPlay, SampleFrame* inOut) final
	{
		auto busConst = CoreAudioBus{{&inOut, 1}, Engine::audioEngine()->framesPerPeriod()};
		auto busMut = CoreAudioBusMut{{&inOut, 1}, Engine::audioEngine()->framesPerPeriod()};

		if constexpr (inplace == InplaceOption::Always)
		{
			auto workingBuffer = this->getWorkingBuffer();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBuffer);
			this->processImpl(notesToPlay, workingBuffer);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBuffer), busMut);
		}
		else if constexpr (inplace == InplaceOption::Never)
		{
			auto workingBufferIn = this->getWorkingBufferIn();
			auto workingBufferOut = this->getWorkingBufferOut();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBufferIn);
			this->processImpl(notesToPlay, constSpan(workingBufferIn), workingBufferOut);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBufferOut), busMut);
		}
		else
		{
			if (m_inplace)
			{
				auto workingBuffer = this->getWorkingBufferIn();
				m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBuffer);
				this->processImpl(notesToPlay, workingBuffer, workingBuffer);
				m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBuffer), busMut);
			}
			else
			{
				auto workingBufferIn = this->getWorkingBufferIn();
				auto workingBufferOut = this->getWorkingBufferOut();
				m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBufferIn);
				this->processImpl(notesToPlay, constSpan(workingBufferIn), workingBufferOut);
				m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBufferOut), busMut);
			}
		}
	}

	auto pinConnector() -> PluginPinConnector* { return &m_pinConnector; }

private:
	PluginPinConnector m_pinConnector;

	/**
	 * TODO: In future, can enable inplace processing at runtime for individual plugins
	 * which advertise support for it
	 */
	bool m_inplace = false;
};

//! Effect specialization
template<int numChannelsIn, int numChannelsOut, AudioDataLayout layout, typename SampleT, InplaceOption inplace, bool customWorkingBuffer>
class AudioProcessorImpl<Effect, numChannelsIn, numChannelsOut, layout, SampleT, inplace, customWorkingBuffer>
	: public Effect
	, public AudioProcessingMethod<Effect,
		typename GetAudioDataType<layout, SampleT>::type,
		typename GetAudioDataType<layout, const SampleT>::type,
		inplace>
	, public std::conditional_t<customWorkingBuffer,
		AudioPluginBufferInterface<layout, SampleT>,
		AudioPluginBufferDefaultImpl<layout, SampleT, inplace == InplaceOption::Always>>
{
public:
	AudioProcessorImpl(const Plugin::Descriptor* desc, Model* parent = nullptr, const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr)
		: Effect{desc, parent, key}
		, m_pinConnector{numChannelsIn, numChannelsOut, parent}
	{
		connect(Engine::audioEngine(), &AudioEngine::sampleRateChanged, [this]() {
			this->resizeWorkingBuffers(m_pinConnector.in().channelCount(), m_pinConnector.out().channelCount());
		});
		this->resizeWorkingBuffers(m_pinConnector.in().channelCount(), m_pinConnector.out().channelCount());
	}

	auto pinConnector() const -> const PluginPinConnector* final { return &m_pinConnector; }

	//! Returns whether the effect uses inplace processing
	auto isInplace(int channel) const -> bool
	{
		if constexpr (inplace == InplaceOption::Dynamic)
		{
			return m_inplace.at(channel);
		}
		else
		{
			(void)channel;
			return static_cast<bool>(inplace);
		}
	}

protected:
	auto processAudioBufferImpl(CoreAudioDataMut inOut) -> bool final
	{
		if (isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		SampleFrame* temp = inOut.data();
		auto busConst = CoreAudioBus{{&temp, 1}, inOut.size()};
		auto busMut = CoreAudioBusMut{{&temp, 1}, inOut.size()};
		ProcessStatus status;

		if constexpr (inplace == InplaceOption::Always)
		{
			auto workingBuffer = this->getWorkingBufferIn();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBuffer);
			status = this->processImpl(workingBuffer);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBuffer), busMut);
		}
		else if constexpr (inplace == InplaceOption::Never)
		{
			auto workingBufferIn = this->getWorkingBufferIn();
			auto workingBufferOut = this->getWorkingBufferOut();
			m_pinConnector.routeToPlugin<layout, SampleT>(busConst, workingBufferIn);
			status = this->processImpl(constSpan(workingBufferIn), workingBufferOut);
			m_pinConnector.routeFromPlugin<layout, SampleT>(constSpan(workingBufferOut), busMut);
		}
		else
		{
			// TODO: Inplace processing
			assert(false);
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

	/**
	 * TODO: In future, can enable inplace processing at runtime for individual plugins
	 * which advertise support for it.
	 * The index represents the in/out channel index
	 */
	std::vector<bool> m_inplace;
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
 * @param customWorkingBuffer If true, plugin implementation will implement `AudioPluginBufferInterface`
 */
template<class ChildT, int numChannelsIn, int numChannelsOut,
	AudioDataLayout layout, typename SampleT, InplaceOption inplace, bool customWorkingBuffer>
class AudioPluginInterface
	: public detail::AudioProcessorImpl<
		ChildT,
		numChannelsIn,
		numChannelsOut,
		layout,
		SampleT,
		inplace,
		customWorkingBuffer
	>
{
	static_assert(!std::is_const_v<SampleT>);
	static_assert(!std::is_same_v<SampleT, SampleFrame>
		|| ((numChannelsIn == 0 || numChannelsIn == 2) && (numChannelsOut == 0 || numChannelsOut == 2)),
		"Don't use SampleFrame if more than 2 processor channels are needed");

public:
	/*
	AudioProcessor(const Plugin::Descriptor* desc, Model* parent = nullptr, const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr)
		: AudioProcessorImpl{desc, parent, key}
	{
	}

	AudioProcessor(const Plugin::Descriptor* desc, Model* parent, const Plugin::Descriptor::SubPluginFeatures::Key* key, Instrument::Flags flags)
		: AudioProcessorImpl{desc, parent, key, flags}
	{
	}*/

	using Base = detail::AudioProcessorImpl<ChildT,
		numChannelsIn,
		numChannelsOut,
		layout,
		SampleT,
		inplace,
		customWorkingBuffer>;

	using Base::AudioProcessorImpl;

	auto channelsIn() const -> int { return Base::pinConnector()->in().channelCount(); }
	auto channelsOut() const -> int { return Base::pinConnector()->out().channelCount(); }

	// ...
};

using DefaultInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::Always, false>;

using MidiInstrumentPluginInterface = AudioPluginInterface<Instrument, 0, 2,
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::Always, false>;

using DefaultEffectPluginInterface = AudioPluginInterface<Effect, 2, 2,
	AudioDataLayout::Interleaved, SampleFrame, InplaceOption::Always, false>;

} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_INTERFACE_H
