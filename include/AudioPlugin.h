/*
 * AudioPlugin.h - Interface for audio plugins which provides
 *                 audio ports and compile-time customizations
 *
 * Copyright (c) 2025 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#ifndef LMMS_AUDIO_PLUGIN_H
#define LMMS_AUDIO_PLUGIN_H

#include <type_traits>

#include "AudioData.h"
#include "AudioPorts.h"
#include "AudioPortsConfig.h"
#include "Effect.h"
#include "Instrument.h"
#include "InstrumentTrack.h"

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

namespace detail
{

//! Provides the correct `processImpl` interface for instruments or effects to implement
template<class ParentT, AudioPortsConfig config, bool inplace = config.inplace, bool buffered = config.buffered>
class AudioProcessingMethod;

//! Instrument specialization
template<AudioPortsConfig config>
class AudioProcessingMethod<Instrument, config, false, false>
{
	using InBufferT = AudioDataViewType<config, false, true>;
	using OutBufferT = AudioDataViewType<config, true, false>;

protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	//! NOTE: NotePlayHandle-based instruments are currently unsupported
	//virtual void processImpl(NotePlayHandle* nph, InBufferT in, OutBufferT out) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(InBufferT in, OutBufferT out) = 0;
};

//! Instrument specialization (in-place)
template<AudioPortsConfig config>
class AudioProcessingMethod<Instrument, config, true, false>
{
	using InOutBufferT = AudioDataViewType<config, false, false>;

protected:
	//! The main audio processing method for NotePlayHandle-based Instruments
	//! NOTE: NotePlayHandle-based instruments are currently unsupported
	//virtual void processImpl(NotePlayHandle* nph, InOutBufferT inOut) {}

	//! The main audio processing method for MIDI-based Instruments
	virtual void processImpl(InOutBufferT inOut) = 0;
};

//! Instrument specialization (buffered)
template<AudioPortsConfig config, bool inplace>
class AudioProcessingMethod<Instrument, config, inplace, true>
{
protected:
	/**
	 * The main audio processing method for NotePlayHandle-based Instruments.
	 * The implementation knows how to provide the working buffers.
	 * NOTE: NotePlayHandle-based instruments are currently unsupported
	 */
	//virtual void processImpl(NotePlayHandle* nph) {}

	/**
	 * The main audio processing method for MIDI-based Instruments.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual void processImpl() = 0;
};

//! Effect specialization
template<AudioPortsConfig config>
class AudioProcessingMethod<Effect, config, false, false>
{
	using InBufferT = AudioDataViewType<config, false, true>;
	using OutBufferT = AudioDataViewType<config, true, false>;

protected:
	/**
	 * The main audio processing method for Effects. Runs when plugin is not asleep.
	 * The implementation is expected to perform wet/dry mixing for the first 2 channels.
	 */
	virtual auto processImpl(InBufferT in, OutBufferT out) -> ProcessStatus = 0;
};

//! Effect specialization (in-place)
template<AudioPortsConfig config>
class AudioProcessingMethod<Effect, config, true, false>
{
	using InOutBufferT = AudioDataViewType<config, false, false>;

protected:
	/**
	 * The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	 * The implementation is expected to perform wet/dry mixing for the first 2 channels.
	 */
	virtual auto processImpl(InOutBufferT inOut) -> ProcessStatus = 0;
};

//! Effect specialization (buffered)
template<AudioPortsConfig config, bool inplace>
class AudioProcessingMethod<Effect, config, inplace, true>
{
protected:
	/**
	 * The main audio processing method for Effects. Runs when plugin is not asleep.
	 * The implementation knows how to provide the working buffers.
	 * The implementation is expected to perform wet/dry mixing for the first 2 channels.
	 */
	virtual auto processImpl() -> ProcessStatus = 0;
};

//! Connects the core audio channels to the instrument or effect using the audio ports
template<class ParentT, AudioPortsConfig config, class AudioPortsT>
class AudioPlugin
{
	static_assert(always_false_v<ParentT>, "ParentT must be either Instrument or Effect");
};

//! Instrument specialization
template<AudioPortsConfig config, class AudioPortsT>
class AudioPlugin<Instrument, config, AudioPortsT>
	: public Instrument
	, public AudioProcessingMethod<Instrument, config>
{
public:
	template<typename... AudioPortsArgsT>
	AudioPlugin(const Plugin::Descriptor* desc, InstrumentTrack* parent = nullptr,
		const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr,
		Instrument::Flags flags = Instrument::Flag::NoFlags,
		AudioPortsArgsT&&... audioPortArgs)
		: Instrument{desc, parent, key, flags}
		, m_audioPorts{true, this, std::forward<AudioPortsArgsT>(audioPortArgs)...}
	{
		m_audioPorts.init();
	}

protected:
	auto audioPorts() -> AudioPortsT& { return m_audioPorts; }

	auto audioPortsModel() const -> const AudioPortsModel* final
	{
		return m_audioPorts.active()
			? &m_audioPorts.model()
			: nullptr;
	}

	void playImpl(std::span<SampleFrame> inOut) final
	{
		auto buffers = m_audioPorts.buffers();
		if (!buffers)
		{
			// Plugin is not running
			return;
		}

		SampleFrame* temp = inOut.data();
		const auto bus = CoreAudioBusMut{&temp, 1, inOut.size()};
		auto router = m_audioPorts.getRouter();

		router.process(bus, *buffers, [this](auto... buffers) {
			this->processImpl(buffers...);
		});
	}

	void playNoteImpl(NotePlayHandle* notesToPlay, std::span<SampleFrame> inOut) final
	{
		/**
		 * NOTE: Only MIDI-based instruments are currently supported by AudioPlugin.
		 * NotePlayHandle-based instruments use buffers from their play handles, and more work
		 * would be needed to integrate that system with the AudioBuffer system
		 * used by AudioPlugin. AudioBuffer is also not thread-safe.
		 *
		 * The `Instrument::playNote()` method is still called for MIDI-based instruments when
		 * notes are played, so this method is a no-op.
		 */
	}

private:
	AudioPortsT m_audioPorts;
};

//! Effect specialization
template<AudioPortsConfig config, class AudioPortsT>
class AudioPlugin<Effect, config, AudioPortsT>
	: public Effect
	, public AudioProcessingMethod<Effect, config>
{
public:
	template<typename... AudioPortsArgsT>
	AudioPlugin(const Plugin::Descriptor* desc, Model* parent = nullptr,
		const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr,
		AudioPortsArgsT&&... audioPortArgs)
		: Effect{desc, parent, key}
		, m_audioPorts{false, this, std::forward<AudioPortsArgsT>(audioPortArgs)...}
	{
		m_audioPorts.init();
	}

protected:
	auto audioPorts() -> AudioPortsT& { return m_audioPorts; }

	auto audioPortsModel() const -> const AudioPortsModel* final
	{
		return m_audioPorts.active()
			? &m_audioPorts.model()
			: nullptr;
	}

	auto processAudioBufferImpl(std::span<SampleFrame> inOut) -> bool final
	{
		if (isSleeping())
		{
			this->processBypassedImpl();
			return false;
		}

		auto buffers = m_audioPorts.buffers();
		assert(buffers != nullptr);

		SampleFrame* temp = inOut.data();
		const auto bus = CoreAudioBusMut{&temp, 1, inOut.size()};
		auto router = m_audioPorts.getRouter();

		ProcessStatus status;
		router.process(bus, *buffers, [&status, this](auto... buffers) {
			status = this->processImpl(buffers...);
		});

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

private:
	AudioPortsT m_audioPorts;
};


} // namespace detail


/**
 * AudioPlugin is the bridge connecting an Instrument/Effect base class used by the Core
 * with its derived class used by a plugin implementation.
 *
 * Pin connector routing and other common tasks are handled here to allow plugin implementations
 * to focus solely on audio processing or generation without needing to worry about how their plugin
 * interfaces with LMMS Core.
 *
 * This design allows for some compile-time customization over aspects of the plugin implementation
 * such as the number of in/out channels and whether samples are interleaved, so plugin developers can
 * implement their plugin in whatever way works best for them. All the mapping of their plugin to/from
 * LMMS Core is handled here, at compile-time where possible for best performance.
 *
 * A `processImpl` interface method is provided which must be implemented by the plugin implementation.
 *
 * @tparam ParentT Either `Instrument` or `Effect`
 * @tparam config Compile time configuration to customize `AudioPlugin`
 * @tparam AudioPortsT The plugin's audio port - must fully implement `AudioPorts`
 */
template<class ParentT, AudioPortsConfig config,
	template<AudioPortsConfig> class AudioPortsT = DefaultAudioPorts>
class AudioPlugin
	: public detail::AudioPlugin<ParentT, config, AudioPortsT<config>>
{
	static_assert(config.kind != AudioDataKind::SampleFrame
		|| ((config.inputs == 0 || config.inputs == 2) && (config.outputs == 0 || config.outputs == 2)),
		"Don't use SampleFrame if more than 2 processor channels are needed");

	static_assert(std::is_base_of_v<detail::AudioPortsTag, AudioPortsT<config>>,
		"AudioPortT must be `AudioPorts` or inherit from it");

	using Base = typename detail::AudioPlugin<ParentT, config, AudioPortsT<config>>;

public:
	//! The last parameter(s) are variadic template parameters passed to the audio port constructor
	using Base::Base;

	static constexpr auto pluginConfig() -> AudioPortsConfig { return config; }

private:
	/**
	 * Hooks into the plugin's SerializingObject in order to save and load the audio ports.
	 * Plugin implementations do not have to do anything and audio ports will be
	 * saved and loaded when they need to be.
	 */
	class AudioPortSerializer final : public SerializingObjectHook
	{
	public:
		AudioPortSerializer(AudioPlugin* ap)
			: m_ap{ap}
		{
			ap->setHook(this);
		}

		void saveSettings(QDomDocument& doc, QDomElement& element) final
		{
			m_ap->audioPorts().saveSettings(doc, element);
		}

		void loadSettings(const QDomElement& element) final
		{
			m_ap->audioPorts().loadSettings(element);
		}

	private:
		AudioPlugin* m_ap;
	};

	AudioPortSerializer m_serializer{this};
};


// NOTE: NotePlayHandle-based instruments are not supported yet
using DefaultMidiInstrument = AudioPlugin<Instrument, AudioPortsConfig {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 0,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;

using DefaultEffect = AudioPlugin<Effect, AudioPortsConfig {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 2,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;


} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_H
