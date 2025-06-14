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

#include <concepts>
#include <type_traits>

#include "Effect.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "PluginAudioPorts.h"

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
template<class ParentT, AudioPortsSettings settings,
	bool inplace = settings.inplace, bool buffered = settings.buffered>
class AudioProcessingMethod;

//! SingleStreamedInstrument specialization
template<AudioPortsSettings settings>
class AudioProcessingMethod<SingleStreamedInstrument, settings, false, false>
{
	using InBufferT = GetAudioBufferViewType<settings, false, true>;
	using OutBufferT = GetAudioBufferViewType<settings, true, false>;

protected:
	//! The main audio processing method for single-streamed instruments
	virtual void processImpl(InBufferT in, OutBufferT out) = 0;
};

//! SingleStreamedInstrument specialization (in-place)
template<AudioPortsSettings settings>
class AudioProcessingMethod<SingleStreamedInstrument, settings, true, false>
{
	using InOutBufferT = GetAudioBufferViewType<settings, false, false>;

protected:
	//! The main audio processing method for single-streamed instruments
	virtual void processImpl(InOutBufferT inOut) = 0;
};

//! SingleStreamedInstrument specialization (buffered)
template<AudioPortsSettings settings, bool inplace>
class AudioProcessingMethod<SingleStreamedInstrument, settings, inplace, true>
{
protected:
	/**
	 * The main audio processing method for single-streamed instruments.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual void processImpl() = 0;
};

//! SingleStreamedMidiInstrument specialization (same as SingleStreamedInstrument)
template<AudioPortsSettings settings, bool inplace, bool buffered>
class AudioProcessingMethod<SingleStreamedMidiInstrument, settings, inplace, buffered>
	: public AudioProcessingMethod<SingleStreamedInstrument, settings, inplace, buffered>
{
};

//! MultiStreamedInstrument specialization
template<AudioPortsSettings settings>
class AudioProcessingMethod<MultiStreamedInstrument, settings, false, false>
{
	using InBufferT = AudioDataViewType<settings, false, true>;
	using OutBufferT = AudioDataViewType<settings, true, false>;

protected:
	//! The main audio processing method for multi-streamed instruments
	virtual void processImpl(NotePlayHandle* nph, InBufferT in, OutBufferT out) = 0;
};

//! MultiStreamedInstrument specialization (in-place)
template<AudioPortsSettings settings>
class AudioProcessingMethod<MultiStreamedInstrument, settings, true, false>
{
	using InOutBufferT = AudioDataViewType<settings, false, false>;

protected:
	//! The main audio processing method for multi-streamed instruments
	virtual void processImpl(NotePlayHandle* nph, InOutBufferT inOut) = 0;
};

//! MultiStreamedInstrument specialization (buffered)
template<AudioPortsSettings settings, bool inplace>
class AudioProcessingMethod<MultiStreamedInstrument, settings, inplace, true>
{
protected:
	/**
	 * The main audio processing method for multi-streamed instruments.
	 * The implementation knows how to provide the working buffers.
	 */
	virtual void processImpl(NotePlayHandle* nph) = 0;
};

//! Effect specialization
template<AudioPortsSettings settings>
class AudioProcessingMethod<Effect, settings, false, false>
{
	using InBufferT = GetAudioBufferViewType<settings, false, true>;
	using OutBufferT = GetAudioBufferViewType<settings, true, false>;

protected:
	/**
	 * The main audio processing method for Effects. Runs when plugin is not asleep.
	 * The implementation is expected to perform wet/dry mixing for the first 2 channels.
	 */
	virtual auto processImpl(InBufferT in, OutBufferT out) -> ProcessStatus = 0;
};

//! Effect specialization (in-place)
template<AudioPortsSettings settings>
class AudioProcessingMethod<Effect, settings, true, false>
{
	using InOutBufferT = GetAudioBufferViewType<settings, false, false>;

protected:
	/**
	 * The main audio processing method for inplace Effects. Runs when plugin is not asleep.
	 * The implementation is expected to perform wet/dry mixing for the first 2 channels.
	 */
	virtual auto processImpl(InOutBufferT inOut) -> ProcessStatus = 0;
};

//! Effect specialization (buffered)
template<AudioPortsSettings settings, bool inplace>
class AudioProcessingMethod<Effect, settings, inplace, true>
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
template<class ParentT, AudioPortsSettings settings, class AudioPortsT>
class AudioPlugin
{
	static_assert(always_false_v<ParentT>, "ParentT must be either Instrument or Effect");
};

//! SingleStreamedInstrument / SingleStreamedMidiInstrument specialization
template<std::derived_from<SingleStreamedInstrument> InstrumentT, AudioPortsSettings settings, class AudioPortsT>
class AudioPlugin<InstrumentT, settings, AudioPortsT>
	: public InstrumentT
	, public AudioProcessingMethod<InstrumentT, settings>
{
public:
	template<typename... AudioPortsArgsT>
	AudioPlugin(const Plugin::Descriptor* desc, InstrumentTrack* parent = nullptr,
		const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr,
		Instrument::Flags flags = Instrument::Flag::NoFlags,
		AudioPortsArgsT&&... audioPortArgs)
		: InstrumentT{desc, parent, key, flags}
		, m_audioPorts{true, this, std::forward<AudioPortsArgsT>(audioPortArgs)...}
	{
		m_audioPorts.init();
	}

protected:
	auto audioPorts() -> AudioPortsT& { return m_audioPorts; }
	auto audioPorts() const -> const AudioPortsT& { return m_audioPorts; }

	auto audioPortsModel() const -> const AudioPortsModel* final
	{
		return m_audioPorts.active()
			? &m_audioPorts.model()
			: nullptr;
	}

	void processCoreImpl(std::span<SampleFrame> coreInOut) final
	{
		if (!m_audioPorts.active())
		{
			// Plugin is not running
			return;
		}

		auto buffers = m_audioPorts.buffers();
		assert(buffers != nullptr);

		SampleFrame* temp = coreInOut.data();
		const auto bus = AudioBus<SampleFrame>{&temp, 1, coreInOut.size()};
		auto router = m_audioPorts.getRouter();

		router.process(bus, *buffers, [this](auto... buffers) {
			this->processImpl(buffers...);
		});
	}

private:
	AudioPortsT m_audioPorts;
};

//! MultiStreamedInstrument specialization
template<AudioPortsSettings settings, class AudioPortsT>
class AudioPlugin<MultiStreamedInstrument, settings, AudioPortsT>
	: public MultiStreamedInstrument
	, public AudioProcessingMethod<MultiStreamedInstrument, settings>
{
public:
	template<typename... AudioPortsArgsT>
	AudioPlugin(const Plugin::Descriptor* desc, InstrumentTrack* parent = nullptr,
		const Plugin::Descriptor::SubPluginFeatures::Key* key = nullptr,
		Instrument::Flags flags = Instrument::Flag::NoFlags,
		AudioPortsArgsT&&... audioPortArgs)
		: MultiStreamedInstrument{desc, parent, key, flags}
		, m_audioPorts{true, this, std::forward<AudioPortsArgsT>(audioPortArgs)...}
	{
		m_audioPorts.init();
	}

protected:
	auto audioPorts() -> AudioPortsT& { return m_audioPorts; }
	auto audioPorts() const -> const AudioPortsT& { return m_audioPorts; }

	auto audioPortsModel() const -> const AudioPortsModel* final
	{
		// TODO: Enable pin connector GUI once multi-stream instrument buffers
		//       are supported. The default pin connector routing works with
		//       multi-stream instruments only because the "direct routing"
		//       optimization sidesteps the need for plugin buffers.
		return nullptr;
	}

	void processCoreImpl(NotePlayHandle* nph, std::span<SampleFrame> coreInOut) final
	{
		if (!m_audioPorts.active())
		{
			// Plugin is not running
			return;
		}

		// TODO: Need to use NPH buffer here

		auto buffers = m_audioPorts.buffers();
		assert(buffers != nullptr);

		SampleFrame* temp = coreInOut.data();
		const auto bus = AudioBus<SampleFrame>{&temp, 1, coreInOut.size()};
		auto router = m_audioPorts.getRouter();

		router.process(bus, *buffers, [=, this](auto... buffers) {
			this->processImpl(nph, buffers...);
		});
	}

private:
	AudioPortsT m_audioPorts;
};

//! Effect specialization
template<AudioPortsSettings settings, class AudioPortsT>
class AudioPlugin<Effect, settings, AudioPortsT>
	: public Effect
	, public AudioProcessingMethod<Effect, settings>
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
	auto audioPorts() const -> const AudioPortsT& { return m_audioPorts; }

	auto audioPortsModel() const -> const AudioPortsModel* final
	{
		return m_audioPorts.active()
			? &m_audioPorts.model()
			: nullptr;
	}

	auto processAudioBufferImpl(std::span<SampleFrame> inOut) -> bool final
	{
		if (isSleeping() || !m_audioPorts.active())
		{
			this->processBypassedImpl();
			return false;
		}

		auto buffers = m_audioPorts.buffers();
		assert(buffers != nullptr);

		SampleFrame* temp = inOut.data();
		const auto bus = AudioBus<SampleFrame>{&temp, 1, inOut.size()};
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
 * @tparam ParentT `SingleStreamedInstrument`, `SingleStreamedMidiInstrument`, `MultiStreamedInstrument`, or `Effect`
 * @tparam settings Compile-time settings to customize `AudioPlugin`
 * @tparam AudioPortsT The plugin's audio port - must fully implement `AudioPorts`
 */
template<class ParentT, AudioPortsSettings settings, class AudioPortsT>
class AudioPlugin
	: public detail::AudioPlugin<ParentT, settings, AudioPortsT>
{
	static_assert(settings == AudioPortsT::audioPortsSettings());

	static_assert(std::is_base_of_v<detail::AudioPortsTag, AudioPortsT>,
		"AudioPortT must implement `AudioPorts`");

	static_assert(validate<settings>());

	using Base = typename detail::AudioPlugin<ParentT, settings, AudioPortsT>;

public:
	//! The last parameter(s) are variadic template parameters passed to the audio port constructor
	using Base::Base;

	static constexpr auto audioPortsSettings() -> AudioPortsSettings { return settings; }

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


/**
 * Same as `AudioPlugin` but the audio port is passed as a template template parameter.
 *
 * @tparam ParentT `SingleStreamedInstrument`, `SingleStreamedMidiInstrument`, `MultiStreamedInstrument`, or `Effect`
 * @tparam settings Compile-time settings to customize `AudioPlugin`
 * @tparam AudioPortsT The plugin's audio port - must fully implement `AudioPorts`
 */
template<class ParentT, AudioPortsSettings settings,
	template<AudioPortsSettings> class AudioPortsT = PluginAudioPorts>
using AudioPluginExt = AudioPlugin<ParentT, settings, AudioPortsT<settings>>;


using DefaultSingleStreamedInstrument = AudioPluginExt<SingleStreamedInstrument, AudioPortsSettings {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 0,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;

using DefaultSingleStreamedMidiInstrument = AudioPluginExt<SingleStreamedMidiInstrument, AudioPortsSettings {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 0,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;

using DefaultMultiStreamedInstrument = AudioPluginExt<MultiStreamedInstrument, AudioPortsSettings {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 0,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;

using DefaultEffect = AudioPluginExt<Effect, AudioPortsSettings {
	.kind = AudioDataKind::SampleFrame,
	.interleaved = true,
	.inputs = 2,
	.outputs = 2,
	.inplace = true,
	.buffered = false }>;


} // namespace lmms

#endif // LMMS_AUDIO_PLUGIN_H
