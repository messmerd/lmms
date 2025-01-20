/*
 * RemotePluginAudioPort.h - PluginAudioPort implementation for RemotePlugin
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

#ifndef LMMS_REMOTE_PLUGIN_AUDIO_PORT_H
#define LMMS_REMOTE_PLUGIN_AUDIO_PORT_H

#include "PluginAudioPort.h"
#include "lmms_basics.h"

namespace lmms
{

class RemotePlugin;

class RemotePluginAudioPortController
{
public:
	RemotePluginAudioPortController(PluginPinConnector& pinConnector);

	//! Call after a RemotePlugin is created
	void activate(RemotePlugin* remotePlugin)
	{
		assert(remotePlugin != nullptr);
		m_remotePlugin = remotePlugin;
		remotePluginUpdateBuffers(
			m_pinConnector->in().channelCount(),
			m_pinConnector->out().channelCount(),
			m_frames);
	}

	//! Call before a RemotePlugin is destroyed
	void deactivate()
	{
		m_remotePlugin = nullptr;
	}

	auto pc() -> PluginPinConnector&
	{
		return *m_pinConnector;
	}

protected:
	void remotePluginUpdateBuffers(int channelsIn, int channelsOut, fpp_t frames);
	auto remotePluginInputBuffer() const -> float*;
	auto remotePluginOutputBuffer() const -> float*;

	RemotePlugin* m_remotePlugin = nullptr;
	PluginPinConnector* m_pinConnector = nullptr;

	fpp_t m_frames = 0;
};


//! `PluginAudioPort` implementation for `RemotePlugin`
template<AudioPluginConfig config>
class RemotePluginAudioPort
	: public CustomPluginAudioPort<config>
	, public RemotePluginAudioPortController
{
	using SampleT = GetAudioDataType<config.kind>;

public:
	RemotePluginAudioPort(bool isInstrument, Model* parent)
		: CustomPluginAudioPort<config>{isInstrument, parent}
		, RemotePluginAudioPortController{*static_cast<PluginPinConnector*>(this)}
	{
	}

	static_assert(config.kind == AudioDataKind::F32, "RemotePlugin only supports float");
	static_assert(config.layout == AudioDataLayout::Split, "RemotePlugin only supports non-interleaved");
	static_assert(!config.inplace, "RemotePlugin does not support inplace processing");

	auto controller() -> RemotePluginAudioPortController&
	{
		return *static_cast<RemotePluginAudioPortController*>(this);
	}

	/*
	 * `PluginAudioPort` implementation
	 */

	//! Only returns the buffer interface if audio port is active
	auto buffers() -> AudioPluginBufferInterface<config>* override
	{
		return active() ? this : nullptr;
	}

	/*
	 * `AudioPluginBufferInterface` implementation
	 */

	auto inputBuffer() -> SplitAudioData<SampleT, config.inputs> override
	{
		assert(m_remotePlugin != nullptr);
		return {m_audioBufferIn.data(), static_cast<pi_ch_t>(this->in().channelCount()), m_frames};
	}

	auto outputBuffer() -> SplitAudioData<SampleT, config.outputs> override
	{
		assert(m_remotePlugin != nullptr);
		return {m_audioBufferOut.data(), static_cast<pi_ch_t>(this->out().channelCount()), m_frames};
	}

	void updateBuffers(int channelsIn, int channelsOut, f_cnt_t frames) override
	{
		if (!m_remotePlugin) { return; }
		remotePluginUpdateBuffers(channelsIn, channelsOut, frames);
	}

	/*
	 * `PluginPinConnector` implementation
	 */

	//! Receives updates from the pin connector
	void bufferPropertiesChanged(int inChannels, int outChannels, f_cnt_t frames) override
	{
		if (!m_remotePlugin) { return; }

		m_frames = frames;

		// Connects the pin connector to the buffers
		updateBuffers(inChannels, outChannels, frames);

		// Update the views into the RemotePlugin buffer
		float* ptr = remotePluginInputBuffer();
		m_audioBufferIn.resize(inChannels);
		for (pi_ch_t idx = 0; idx < inChannels; ++idx)
		{
			m_audioBufferIn[idx] = ptr;
			ptr += frames;
		}

		ptr = remotePluginOutputBuffer();
		m_audioBufferOut.resize(outChannels);
		for (pi_ch_t idx = 0; idx < outChannels; ++idx)
		{
			m_audioBufferOut[idx] = ptr;
			ptr += frames;
		}
	}

	auto active() const -> bool override { return m_remotePlugin != nullptr; }

private:
	// Views into RemotePlugin's shared memory buffer
	std::vector<SplitSampleType<float>*> m_audioBufferIn;
	std::vector<SplitSampleType<float>*> m_audioBufferOut;
};


//! An audio port that can choose between RemotePlugin or a local buffer at runtime
template<AudioPluginConfig config, class LocalBufferT = DefaultAudioPluginBuffer<config>>
class ConfigurableAudioPort
	: public RemotePluginAudioPort<config>
{
	using SampleT = GetAudioDataType<config.kind>;

public:
	using RemotePluginAudioPort<config>::RemotePluginAudioPort;

	void useRemote(bool remote = true)
	{
		if (remote)
		{
			this->activate(this->m_remotePlugin);
			m_isRemote = remote;
			m_localBuffer.reset();
		}
		else
		{
			this->deactivate();
			m_isRemote = remote;
			m_localBuffer.emplace();
		}
	}

	auto isRemote() const { return m_isRemote; }

	auto buffers() -> AudioPluginBufferInterface<config>* override
	{
		return isRemote()
			? RemotePluginAudioPort<config>::buffers()
			: &m_localBuffer.value();
	}

	auto inputBuffer() -> SplitAudioData<SampleT, config.inputs> override
	{
		return isRemote()
			? RemotePluginAudioPort<config>::inputBuffer()
			: m_localBuffer->inputBuffer();
	}

	auto outputBuffer() -> SplitAudioData<SampleT, config.outputs> override
	{
		return isRemote()
			? RemotePluginAudioPort<config>::outputBuffer()
			: m_localBuffer->outputBuffer();
	}

	auto active() const -> bool override
	{
		return isRemote()
			? RemotePluginAudioPort<config>::active()
			: true;
	}

private:
	std::optional<LocalBufferT> m_localBuffer;
	bool m_isRemote = true;
};


} // namespace lmms

#endif // LMMS_REMOTE_PLUGIN_AUDIO_PORT_H
