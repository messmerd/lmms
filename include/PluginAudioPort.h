/*
 * PluginAudioPort.h
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

#ifndef LMMS_PLUGIN_AUDIO_PORT_H
#define LMMS_PLUGIN_AUDIO_PORT_H

#include "AudioPluginBuffer.h"
#include "AudioPluginConfig.h"
#include "PluginPinConnector.h"

namespace lmms
{


namespace detail {
struct PluginAudioPortTag {};
} // namespace detail


/**
 * Interface for an audio port implementation.
 * Contains a pin connector and provides access to the audio buffers.
 */
template<AudioPluginConfig config>
class PluginAudioPort
	: public PluginPinConnector // TODO: private inheritance?
	, public detail::PluginAudioPortTag
{
public:
	auto pinConnector() const -> const PluginPinConnector&
	{
		return *static_cast<const PluginPinConnector*>(this);
	}

	/**
	 * Returns false if the plugin is not loaded.
	 * Custom audio ports with a "plugin not loaded" state should override this.
	 */
	virtual auto active() const -> bool { return true; }

	/**
	 * `AudioPlugin` calls this to decide whether to pass the audio buffers to
	 * the `processImpl` methods.
	 *
	 * Sending the audio buffers to `processImpl` in the plugin implementation may be
	 * pointless for custom audio port implementations that manage their own buffers,
	 * so in that case reimplementing this method in a child class to return `false`
	 * results in a cleaner interface.
	 */
	constexpr static auto provideProcessBuffers() -> bool { return true; }

protected:
	PluginAudioPort(bool isInstrument, Model* parent)
		: PluginPinConnector{isInstrument, parent}
	{
	}

	auto pinConnector() -> PluginPinConnector& { return *static_cast<PluginPinConnector*>(this); }

	//! Returns the pin connector's router
	auto getRouter() const -> PluginPinConnector::Router<config>
	{
		return pinConnector().template getRouter<config>();
	}

	//! Returns nullptr if the port is unavailable (i.e. Vestige with no plugin loaded)
	virtual auto buffers() -> AudioPluginBufferInterface<config>* = 0;
};


/**
 * The default audio port for plugins that do not provide their own.
 * Contains a pin connector and audio buffers.
 *
 * This audio port still has *some* ability for customization by using a custom `BufferT`,
 * but for full control, you'll need to provide your own audio port implementation.
 */
template<AudioPluginConfig config, template<AudioPluginConfig> class BufferT>
class PluginAudioPortDefaultImpl
	: public PluginAudioPort<config>
	, public BufferT<config>
{
	static_assert(std::is_base_of_v<AudioPluginBufferInterface<config>, BufferT<config>>,
		"BufferT must derive from AudioPluginBufferInterface");

protected:
	using PluginAudioPort<config>::PluginAudioPort;

	auto buffers() -> BufferT<config>* final { return static_cast<BufferT<config>*>(this); }

	static constexpr auto pluginConfig() -> AudioPluginConfig { return config; }

private:
	void bufferPropertiesChanged(int inChannels, int outChannels, f_cnt_t frames) final
	{
		// Connects the pin connector to the buffers
		this->updateBuffers(inChannels, outChannels, frames);
	}
};


//! Default audio port
template<AudioPluginConfig config>
using DefaultPluginAudioPort = PluginAudioPortDefaultImpl<config, AudioPluginBufferDefaultImpl>;


//! Custom audio port - audio buffer interface to be implementated in child class
template<AudioPluginConfig config>
class PluginAudioPortCustom
	: public PluginAudioPort<config>
	, public AudioPluginBufferInterface<config>
{
public:
	constexpr static auto provideProcessBuffers() -> bool { return false; }

protected:
	using PluginAudioPort<config>::PluginAudioPort;

	void bufferPropertiesChanged(int inChannels, int outChannels, f_cnt_t frames) override
	{
		// Connects the pin connector to the buffers
		this->updateBuffers(inChannels, outChannels, frames);
	}

	static constexpr auto pluginConfig() -> AudioPluginConfig { return config; }
};


} // namespace lmms

#endif // LMMS_PLUGIN_AUDIO_PORT_H
