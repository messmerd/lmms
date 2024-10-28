/*
 * PluginPinConnector.h - Specifies how to route audio channels
 *                        in and out of a plugin.
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

#ifndef LMMS_PLUGIN_PIN_CONNECTOR_H
#define LMMS_PLUGIN_PIN_CONNECTOR_H

#include <cassert>
#include <vector>

#include "AutomatableModel.h"
#include "lmms_export.h"
#include "SampleFrame.h"
#include "SerializingObject.h"

class QWidget;

namespace lmms
{

namespace gui
{

class PluginPinConnectorView;

} // namespace gui

inline constexpr int DynamicChannelCount = -1;

//! Configuration for audio channel routing in/out of plugin
class LMMS_EXPORT PluginPinConnector
	: public Model
	, public SerializingObject
{
	Q_OBJECT

public:
	//! [track channel][plugin channel]
	using PinMap = std::vector<std::vector<BoolModel*>>; // TODO: Experiment with different options to see which has the best performance

	//! A plugin's input or output connections and other info
	class Matrix
	{
	public:
		auto pins() const -> const PinMap& { return m_pins; }

		auto channelCount() const -> int { return m_channelCount; }

		auto channelName(int channel) const -> QString;

		auto enabled(std::uint8_t trackChannel, unsigned pluginChannel) const -> bool
		{
			return m_pins[trackChannel][pluginChannel]->value();
		}

		friend class PluginPinConnector;

	private:
		void setTrackChannelCount(PluginPinConnector* parent, int count, const QString& nameFormat);
		void setPluginChannelCount(PluginPinConnector* parent, int count, const QString& nameFormat);

		void setDefaultConnections();

		void saveSettings(QDomDocument& doc, QDomElement& elem) const;
		void loadSettings(const QDomElement& elem);

		PinMap m_pins;
		int m_channelCount = 0;
		std::vector<QString> m_channelNames; //!< optional
	};

	PluginPinConnector(Model* parent = nullptr);
	PluginPinConnector(int pluginChannelCountIn, int pluginChannelCountOut, Model* parent = nullptr);

	/**
	 * Getters
	 */
	auto in() const -> const Matrix& { return m_in; }
	auto out() const -> const Matrix& { return m_out; }
	auto trackChannelCount() const -> std::size_t { return s_totalTrackChannels; }

	/**
	 * Setters
	 */
	void setPluginChannelCounts(int inCount, int outCount);
	void setPluginChannelCountIn(int inCount);
	void setPluginChannelCountOut(int outCount);

	void setDefaultConnections();

	/*
	 * Routes audio from LMMS track channels to plugin inputs according to the plugin pin connector configuration.
	 *
	 * Iterates through each output channel, mixing together all input audio routed to the output channel.
	 * If no audio is routed to an output channel, the output channel's buffer is zeroed.
	 *
	 * `in`     : track channels from LMMS core (currently just the main track channel pair)
	 *            `in.frames` provides the number of frames in each `in`/`out` audio buffer
	 * `out`    : plugin input channels in Split form
	 */
	template<AudioDataLayout layout, typename SampleT>
	void routeToPlugin(CoreAudioBus in, AudioData<layout, SampleT> out) const
	{
		// Ignore all unused track channels for better performance
		const auto inSizeConstrained = m_trackChannelsUpperBound / 2;
		assert(inSizeConstrained <= in.bus.size());

		// Zero the output buffer
		std::fill(out.begin(), out.end(), {});

		std::uint8_t outChannel = 0;
		for (f_cnt_t outSampleIdx = 0; outSampleIdx < out.size(); outSampleIdx += in.frames, ++outChannel)
		{
			mix_ch_t numRouted = 0; // counter for # of in channels routed to the current out channel
			SampleType<layout, SampleT>* outPtr = &out[outSampleIdx];

			for (std::uint8_t inChannelPairIdx = 0; inChannelPairIdx < inSizeConstrained; ++inChannelPairIdx)
			{
				const SampleFrame* inPtr = in.bus[inChannelPairIdx]; // L/R track channel pair

				const std::uint8_t inChannel = inChannelPairIdx * 2;
				const std::uint8_t enabledPins =
					(static_cast<std::uint8_t>(m_in.enabled(inChannel, outChannel)) << 1u)
					| static_cast<std::uint8_t>(m_in.enabled(inChannel + 1, outChannel));

				switch (enabledPins)
				{
					case 0b00: break;
					case 0b01: // R channel only
					{
						for (f_cnt_t frame = 0; frame < in.frames; ++frame)
						{
							outPtr[frame] += convertSample<SampleT>(inPtr[frame].right());
						}
						++numRouted;
						break;
					}
					case 0b10: // L channel only
					{
						for (f_cnt_t frame = 0; frame < in.frames; ++frame)
						{
							outPtr[frame] += convertSample<SampleT>(inPtr[frame].left());
						}
						++numRouted;
						break;
					}
					case 0b11: // Both channels
					{
						for (f_cnt_t frame = 0; frame < in.frames; ++frame)
						{
							outPtr[frame] += convertSample<SampleT>(inPtr[frame].left() + inPtr[frame].right());
						}
						numRouted += 2;
						break;
					}
					default:
						unreachable();
						break;
				}
			}

			// Either no input channels were routed to this output and output stays zeroed,
			// or only one channel was routed and normalization is not needed
			if (numRouted <= 1) { continue; }

			// Normalize output
			for (f_cnt_t frame = 0; frame < in.frames; ++frame)
			{
				outPtr[frame] /= numRouted;
			}
		}
	}

	/*
	 * Routes audio from plugin outputs to LMMS track channels according to the plugin pin connector configuration.
	 *
	 * Iterates through each output channel, mixing together all input audio routed to the output channel.
	 * If no audio is routed to an output channel, `inOut` remains unchanged for audio bypass.
	 *
	 * `in`      : plugin output channels in Split form
	 * `inOut`   : track channels from/to LMMS core (inplace processing)
	 *            `inOut.frames` provides the number of frames in each `in`/`inOut` audio buffer
	 */
	template<AudioDataLayout layout, typename SampleT>
	void routeFromPlugin(AudioData<layout, const SampleT> in, CoreAudioBusMut inOut) const
	{
		assert(inOut.frames <= MAXIMUM_BUFFER_SIZE);

		// Ignore all unused track channels for better performance
		const auto inOutSizeConstrained = m_trackChannelsUpperBound / 2;
		assert(inOutSizeConstrained <= inOut.bus.size());

		for (std::uint8_t outChannelPairIdx = 0; outChannelPairIdx < inOutSizeConstrained; ++outChannelPairIdx)
		{
			SampleFrame* outPtr = inOut.bus[outChannelPairIdx]; // L/R track channel pair
			const auto outChannel = static_cast<std::uint8_t>(outChannelPairIdx * 2);

			// TODO C++20: Use explicit non-type template parameter instead of `outChannelOffset` auto parameter
			const auto mixInputs = [&](std::uint8_t outChannel, auto outChannelOffset) {
				constexpr auto outChannelOffsetConst = outChannelOffset();
				WorkingBuffer.fill(0); // used as buffer out

				// Counter for # of in channels routed to the current out channel
				mix_ch_t numRouted = 0;

				std::uint8_t inChannel = 0;
				for (f_cnt_t inSampleIdx = 0; inSampleIdx < in.size(); inSampleIdx += inOut.frames, ++inChannel)
				{
					if (!m_out.enabled(outChannel + outChannelOffsetConst, inChannel)) { continue; }

					const SampleType<layout, const SampleT>* inPtr = &in[inSampleIdx];
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						WorkingBuffer[frame] += inPtr[frame];
					}
					++numRouted;
				}

				switch (numRouted)
				{
					case 0:
						// Nothing needs to be written to `inOut` for audio bypass,
						// since it already contains the LMMS core input audio.
						break;
					case 1:
					{
						// Normalization not needed, but copying is
						for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
						{
							outPtr[frame][outChannelOffsetConst] = WorkingBuffer[frame];
						}
						break;
					}
					default: // >= 2
					{
						// Normalize output
						for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
						{
							outPtr[frame][outChannelOffsetConst] = WorkingBuffer[frame] / numRouted;
						}
						break;
					}
				}
			};

			// Left SampleFrame channel first
			mixInputs(outChannel, std::integral_constant<int, 0>{});

			// Right SampleFrame channel second
			mixInputs(outChannel, std::integral_constant<int, 1>{});
		}
	}


	// TODO: `SampleFrame` routing


	/**
	 * SerializingObject implementation
	 */
	void saveSettings(QDomDocument& doc, QDomElement& elem) override;
	void loadSettings(const QDomElement& elem) override;
	auto nodeName() const -> QString override { return "pins"; }

	auto instantiateView(QWidget* parent = nullptr) -> gui::PluginPinConnectorView*;
	auto getChannelCountText() const -> QString;

	static constexpr std::size_t MaxTrackChannels = 256; // TODO: Move somewhere else

public slots:
	void setTrackChannelCount(int count);

private:
	Matrix m_in;  //!< LMMS --> Plugin
	Matrix m_out; //!< Plugin --> LMMS

	//! TODO: Move this somewhere else; Will be >= 2 once there is support for adding new track channels
	static constexpr std::size_t s_totalTrackChannels = DEFAULT_CHANNELS;

	//! This value is <= to the total number of track channels (currently always 2)
	unsigned int m_trackChannelsUpperBound = DEFAULT_CHANNELS;

	// TODO: When full routing is added, get LMMS channel counts from bus or router class
};

} // namespace lmms

#endif // LMMS_PLUGIN_PIN_CONNECTOR_H
