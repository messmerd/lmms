/*
 * LadspaEffect.h - class for handling LADSPA effect plugins
 *
 * Copyright (c) 2006-2008 Danny McRae <khjklujn/at/users.sourceforge.net>
 * Copyright (c) 2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef _LADSPA_EFFECT_H
#define _LADSPA_EFFECT_H

#include <QMutex>

#include "AudioPlugin.h"
#include "ladspa.h"
#include "LadspaControls.h"
#include "LadspaManager.h"

namespace lmms
{

struct port_desc_t;
using multi_proc_t = QVector<port_desc_t*>;

inline constexpr auto LadspaConfig = AudioPortsConfig {
	.kind = AudioDataKind::F32,
	.interleaved = false,
	.inplace = false,
	.buffered = false
};


class LadspaAudioPortsBuffer
	: public AudioPorts<LadspaConfig>::Buffer
{
	static_assert(std::is_same_v<LADSPA_Data, GetAudioDataType<LadspaConfig.kind>>);

public:
	LadspaAudioPortsBuffer() = default;
	~LadspaAudioPortsBuffer() override = default;

	auto inputBuffer() -> SplitAudioData<LADSPA_Data> final
	{
		return {m_accessBuffer.data(), m_channelsIn, m_frames};
	}

	auto outputBuffer() -> SplitAudioData<LADSPA_Data> final
	{
		return {m_accessBuffer.data() + m_channelsIn, m_channelsOut, m_frames};
	}

	auto frames() const -> fpp_t final
	{
		return m_frames;
	}

	void updateBuffers(proc_ch_t channelsIn, proc_ch_t channelsOut, f_cnt_t frames) final
	{
		assert(channelsIn != DynamicChannelCount && channelsOut != DynamicChannelCount);
		const auto channels = static_cast<std::size_t>(channelsIn + channelsOut);

		m_sourceBuffer.resize(channels * frames);
		m_accessBuffer.resize(channels);

		m_frames = frames;

		LADSPA_Data* ptr = m_sourceBuffer.data();
		for (std::size_t channel = 0; channel < channels; ++channel)
		{
			m_accessBuffer[channel] = ptr;
			ptr += frames;
		}

		m_channelsIn = channelsIn;
		m_channelsOut = channelsOut;
	}

private:
	//! All input buffers followed by all output buffers
	std::vector<LADSPA_Data> m_sourceBuffer;

	//! Provides [channel][frame] view into `m_sourceBuffer`
	std::vector<LADSPA_Data*> m_accessBuffer;

	proc_ch_t m_channelsIn = config.inputs;
	proc_ch_t m_channelsOut = config.outputs;
	f_cnt_t m_frames = 0;
};


class LadspaAudioPorts
	: public PluginAudioPorts<LadspaConfig>
{
public:

};



class LadspaEffect : public AudioPlugin<Effect, LadspaAudioPorts>
{
	Q_OBJECT
public:
	LadspaEffect( Model * _parent,
			const Descriptor::SubPluginFeatures::Key * _key );
	~LadspaEffect() override;

	void setControl( int _control, LADSPA_Data _data );

	EffectControls * controls() override
	{
		return m_controls;
	}

	inline const multi_proc_t & getPortControls()
	{
		return m_portControls;
	}


private slots:
	void changeSampleRate();


private:
	ProcessStatus processImpl(SplitAudioData<const float> in, SplitAudioData<float> out) override;

	void pluginInstantiation();
	void pluginDestruction();

	static sample_rate_t maxSamplerate( const QString & _name );


	QMutex m_pluginMutex;
	LadspaControls * m_controls;

	sample_rate_t m_maxSampleRate;
	ladspa_key_t m_key;
	int m_portCount;
	bool m_inPlaceBroken;

	const LADSPA_Descriptor * m_descriptor;
	QVector<LADSPA_Handle> m_handles;

	QVector<multi_proc_t> m_ports;
	multi_proc_t m_portControls;

} ;


} // namespace lmms

#endif
