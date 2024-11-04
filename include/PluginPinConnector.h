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

#include <array>
#include <cassert>
#include <vector>

#include "AudioEngine.h"
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


namespace detail
{

template<AudioDataLayout layout, typename SampleT>
struct GetAudioDataType { using type = AudioData<layout, SampleT>; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, SampleFrame> { using type = CoreAudioDataMut; };

template<>
struct GetAudioDataType<AudioDataLayout::Interleaved, const SampleFrame> { using type = CoreAudioData; };

} // namespace detail


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
	 * `out`    : plugin input channels in Split form (Interleaved is not needed or implemented yet)
	 */
	template<AudioDataLayout layout, typename SampleT>
	void routeToPlugin(CoreAudioBus in, AudioData<layout, SampleT> out) const;

	//! Overload for SampleFrame-based plugins
	template<AudioDataLayout layout, typename SampleT>
	void routeToPlugin(CoreAudioBus in, CoreAudioDataMut out) const;

	/*
	 * Routes audio from plugin outputs to LMMS track channels according to the plugin pin connector configuration.
	 *
	 * Iterates through each output channel, mixing together all input audio routed to the output channel.
	 * If no audio is routed to an output channel, `inOut` remains unchanged for audio bypass.
	 *
	 * `in`      : plugin output channels in Split form (Interleaved is not needed or implemented yet)
	 * `inOut`   : track channels from/to LMMS core (inplace processing)
	 *            `inOut.frames` provides the number of frames in each `in`/`inOut` audio buffer
	 */
	template<AudioDataLayout layout, typename SampleT>
	void routeFromPlugin(AudioData<layout, const SampleT> in, CoreAudioBusMut inOut) const;

	//! Overload for SampleFrame-based plugins
	template<AudioDataLayout layout, typename SampleT>
	void routeFromPlugin(CoreAudioData in, CoreAudioBusMut inOut) const;


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
	unsigned int m_trackChannelsUpperBound = DEFAULT_CHANNELS; // TODO: Need to recalculate when pins are set/unset

	/**
	 * Caches whether any plugin output channels are routed to a given track channel (meaning the
	 * track channel is not "bypassed"), which eliminates need for O(N) checking in `routeFromPlugin`.
	 *
	 * This means m_routedChannels[i] == true iif m_out.enabled(i, x) == true for any plugin channel x.
	 */
	std::vector<bool> m_routedChannels; // TODO: Need to calculate when pins are set/unset

	// TODO: When full routing is added, get LMMS channel counts from bus or router class
};


// Out-of-class definitions

template<AudioDataLayout layout, typename SampleT>
inline void PluginPinConnector::routeToPlugin(CoreAudioBus in, AudioData<layout, SampleT> out) const
{
	static_assert(layout == AudioDataLayout::Split, "Only split data is implemented so far");

	assert(m_in.channelCount() != DynamicChannelCount);
	if (m_in.channelCount() == 0) { return; }

	// Ignore all unused track channels for better performance
	const auto inSizeConstrained = m_trackChannelsUpperBound / 2;
	assert(inSizeConstrained <= in.bus.size());

	// Zero the output buffer
	std::fill(out.begin(), out.end(), SampleT{});

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

template<AudioDataLayout layout, typename SampleT>
inline void PluginPinConnector::routeToPlugin(CoreAudioBus in, CoreAudioDataMut out) const
{
	static_assert(layout == AudioDataLayout::Interleaved);

	assert(m_in.channelCount() != DynamicChannelCount);
	if (m_in.channelCount() == 0) { return; }
	assert(m_in.channelCount() == 2); // SampleFrame routing only allows exactly 0 or 2 channels

	// Ignore all unused track channels for better performance
	const auto inSizeConstrained = m_trackChannelsUpperBound / 2;
	assert(inSizeConstrained <= in.bus.size());

	// Zero the output buffer
	std::fill(out.begin(), out.end(), SampleFrame{});

	// Counters for # of in channels routed to the current pair of out channels
	mix_ch_t numRoutedL = 0;
	mix_ch_t numRoutedR = 0;

	const auto samples = in.frames * 2;
	sample_t* outPtr = out.data()->data();

	/*
	* This is essentially a function template with specializations for each
	* of the 16 total routing combinations of an input `SampleFrame*` to an
	* output `SampleFrame*`. The purpose is to eliminate all branching within
	* the inner for-loop in hopes of better performance.
	*
	* TODO C++20: Use explicit non-type template parameter instead of `enabledPins` auto parameter
	*/
	auto route2x2 = [&](const sample_t* inPtr, auto enabledPins) {
		constexpr auto epL =  static_cast<std::uint8_t>(enabledPins() >> 2); // for L out channel
		constexpr auto epR = static_cast<std::uint8_t>(enabledPins() & 0b0011); // for R out channel

		if constexpr (enabledPins() == 0) { return; }

		for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
		{
			// Route to left output channel
			if constexpr ((epL & 0b01) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx + 1];
				++numRoutedL;
			}
			if constexpr ((epL & 0b10) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx];
				++numRoutedL;
			}

			// Route to right output channel
			if constexpr ((epR & 0b01) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx + 1];
				++numRoutedR;
			}
			if constexpr ((epR & 0b10) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx];
				++numRoutedR;
			}
		}
	};


	for (std::uint8_t inChannelPairIdx = 0; inChannelPairIdx < inSizeConstrained; ++inChannelPairIdx)
	{
		const sample_t* inPtr = in.bus[inChannelPairIdx]->data(); // L/R track channel pair

		const std::uint8_t inChannel = inChannelPairIdx * 2;
		const std::uint8_t enabledPins =
			(static_cast<std::uint8_t>(m_in.enabled(inChannel, 0)) << 3u)
			| (static_cast<std::uint8_t>(m_in.enabled(inChannel + 1, 0)) << 2u)
			| (static_cast<std::uint8_t>(m_in.enabled(inChannel, 1)) << 1u)
			| static_cast<std::uint8_t>(m_in.enabled(inChannel + 1, 1));

		switch (enabledPins)
		{
			case 0: break;
			case 1: route2x2(inPtr, std::integral_constant<std::uint8_t, 1>{}); break;
			case 2: route2x2(inPtr, std::integral_constant<std::uint8_t, 2>{}); break;
			case 3: route2x2(inPtr, std::integral_constant<std::uint8_t, 3>{}); break;
			case 4: route2x2(inPtr, std::integral_constant<std::uint8_t, 4>{}); break;
			case 5: route2x2(inPtr, std::integral_constant<std::uint8_t, 5>{}); break;
			case 6: route2x2(inPtr, std::integral_constant<std::uint8_t, 6>{}); break;
			case 7: route2x2(inPtr, std::integral_constant<std::uint8_t, 7>{}); break;
			case 8: route2x2(inPtr, std::integral_constant<std::uint8_t, 8>{}); break;
			case 9: route2x2(inPtr, std::integral_constant<std::uint8_t, 9>{}); break;
			case 10: route2x2(inPtr, std::integral_constant<std::uint8_t, 10>{}); break;
			case 11: route2x2(inPtr, std::integral_constant<std::uint8_t, 11>{}); break;
			case 12: route2x2(inPtr, std::integral_constant<std::uint8_t, 12>{}); break;
			case 13: route2x2(inPtr, std::integral_constant<std::uint8_t, 13>{}); break;
			case 14: route2x2(inPtr, std::integral_constant<std::uint8_t, 14>{}); break;
			case 15: route2x2(inPtr, std::integral_constant<std::uint8_t, 15>{}); break;
			default:
				unreachable();
				break;
		}
	}

	// If the number of channels routed to a specific output is <= 1, no normalization is needed,
	// otherwise each output sample needs to be divided by the number that were routed.

	if (numRoutedL > 1)
	{
		if (numRoutedR > 1)
		{
			// Normalize both output channels
			for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
			{
				outPtr[sampleIdx] /= numRoutedL;
				outPtr[sampleIdx + 1] /= numRoutedR;
			}
		}
		else
		{
			// Normalize left output channel
			for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
			{
				outPtr[sampleIdx] /= numRoutedL;
			}
		}
	}
	else
	{
		// Either no input channels were routed to either output channel and output stays zeroed,
		// or only one channel was routed and normalization is not needed
		if (numRoutedR <= 1) { return; }

		// Normalize right output channel
		for (f_cnt_t sampleIdx = 1; sampleIdx < samples; sampleIdx += 2)
		{
			outPtr[sampleIdx] /= numRoutedR;
		}
	}
}

template<AudioDataLayout layout, typename SampleT>
inline void PluginPinConnector::routeFromPlugin(AudioData<layout, const SampleT> in, CoreAudioBusMut inOut) const
{
	static_assert(layout == AudioDataLayout::Split, "Only split data is implemented so far");

	assert(m_out.channelCount() != DynamicChannelCount);
	if (m_out.channelCount() == 0) { return; }

	// Ignore all unused track channels for better performance
	const auto inOutSizeConstrained = m_trackChannelsUpperBound / 2;
	assert(inOutSizeConstrained <= inOut.bus.size());

	for (std::uint8_t outChannelPairIdx = 0; outChannelPairIdx < inOutSizeConstrained; ++outChannelPairIdx)
	{
		SampleFrame* outPtr = inOut.bus[outChannelPairIdx]; // L/R track channel pair
		const auto outChannel = static_cast<std::uint8_t>(outChannelPairIdx * 2);

		/*
		 * Routes plugin audio to track channel pair and normalizes the result. For track channels
		 * without any plugin audio routed to it, the track channel is unmodified for "bypass"
		 * behavior.
		 *
		 * TODO C++20: Use explicit non-type template parameter instead of `routedChannels` auto parameter
		 */
		const auto routeNx2 = [&](SampleFrame* outPtr, std::uint8_t outChannel, auto routedChannels) {
			constexpr std::uint8_t rc = routedChannels();

			if constexpr (rc == 0b00)
			{
				// Both track channels bypassed - nothing to do
				return;
			}

			// We know at this point that we are writing to at least one of the output channels
			// rather than bypassing, so it is safe to set output buffer of those channels
			// to zero prior to accumulation

			for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
			{
				if constexpr ((rc & 0b10) != 0)
				{
					outPtr[frame].leftRef() = 0.f;
				}
				if constexpr ((rc & 0b01) != 0)
				{
					outPtr[frame].rightRef() = 0.f;
				}
			}

			// Counters for # of in channels routed to the current pair of out channels
			mix_ch_t numRoutedL = 0;
			mix_ch_t numRoutedR = 0;

			unsigned int inChannel = 0; // plugin out channel
			for (f_cnt_t inSampleIdx = 0; inSampleIdx < in.size(); inSampleIdx += inOut.frames, ++inChannel)
			{
				if constexpr (rc == 0b10)
				{
					if (!m_out.enabled(outChannel, inChannel)) { continue; }
					++numRoutedL;
				}

				if constexpr (bc == 0b01)
				{
					if (!m_out.enabled(outChannel + 1, inChannel)) { continue; }
					++numRoutedR;
				}

				if constexpr (bc == 0b11)
				{
					if (!m_out.enabled(outChannel, inChannel)
						&& !m_out.enabled(outChannel + 1, inChannel)) { continue; }
					++numRoutedL;
					++numRoutedR;
				}

				const SampleType<layout, const SampleT>* inPtr = &in[inSampleIdx];
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					if constexpr ((bc & 0b10) != 0)
					{
						outPtr[frame].leftRef() += inPtr[frame];
					}
					if constexpr ((bc & 0b01) != 0)
					{
						outPtr[frame].rightRef() += inPtr[frame];
					}
				}
			}

			// If num routed is 0 or 1, either no plugin channels were routed to the output
			// and the output stays zeroed, or only one channel was routed and normalization is not needed

			if (numRoutedL > 1)
			{
				if (numRoutedR > 1)
				{
					// Normalize output - both channels
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtr[frame].leftRef() /= numRoutedL;
						outPtr[frame].rightRef() /= numRoutedR;
					}
				}
				else
				{
					// Normalize output - left channel
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtr[frame].leftRef() /= numRoutedL;
					}
				}
			}
			else if (numRoutedR > 1)
			{
				// Normalize output - right channel
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					outPtr[frame].rightRef() /= numRoutedR;
				}
			}
		};


		const std::uint8_t routedChannels =
				(static_cast<std::uint8_t>(m_routedChannels[outChannel]) << 1u)
				| static_cast<std::uint8_t>(m_routedChannels[outChannel + 1]);

		switch (routedChannels)
		{
			case 0b00:
				// Both track channels are bypassed, so nothing needs to be written to output
				break;
			case 0b01:
				routeNx2(outPtr, outChannel, std::integral_constant<std::uint8_t, 0b01>{});
				break;
			case 0b10:
				routeNx2(outPtr, outChannel, std::integral_constant<std::uint8_t, 0b10>{});
				break;
			case 0b11:
				routeNx2(outPtr, outChannel, std::integral_constant<std::uint8_t, 0b11>{});
				break;
			default:
				unreachable();
				break;
		}
	}
}

template<AudioDataLayout layout, typename SampleT>
inline void PluginPinConnector::routeFromPlugin(CoreAudioData in, CoreAudioBusMut inOut) const
{
	static_assert(layout == AudioDataLayout::Interleaved);

	assert(m_out.channelCount() != DynamicChannelCount);
	if (m_out.channelCount() == 0) { return; }
	assert(m_out.channelCount() == 2); // SampleFrame routing only allows exactly 0 or 2 channels

	// Ignore all unused track channels for better performance
	const auto inOutSizeConstrained = m_trackChannelsUpperBound / 2;
	assert(inOutSizeConstrained <= inOut.bus.size());

	// Counters for # of in channels routed to the current pair of out channels
	mix_ch_t numRoutedL; // uninitialized
	mix_ch_t numRoutedR; // uninitialized

	const auto samples = inOut.frames * 2;
	const sample_t* inPtr = in.data()->data();

	/*
	* This is essentially a function template with specializations for each
	* of the 16 total routing combinations of an input `SampleFrame*` to an
	* output `SampleFrame*`. The purpose is to eliminate all branching within
	* the inner for-loop in hopes of better performance.
	*
	* TODO C++20: Use explicit non-type template parameter instead of `enabledPins` auto parameter
	*/
	auto route2x2 = [&](sample_t* outPtr, auto enabledPins) {
		constexpr auto epL =  static_cast<std::uint8_t>(enabledPins() >> 2); // for L out channel
		constexpr auto epR = static_cast<std::uint8_t>(enabledPins() & 0b0011); // for R out channel

		if constexpr (enabledPins() == 0) { return; }

		// We know at this point that we are writing to at least one of the output channels
		// rather than bypassing, so it is safe to set output buffer to zero prior to accumulation
		if constexpr (epL != 0 && epR != 0)
		{
			std::fill_n(outPtr, inOut.frames, 0.f);
		}
		else if constexpr (epL != 0)
		{
			for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
			{
				outPtr[sampleIdx] = 0.f;
			}
		}
		else if constexpr (epR != 0)
		{
			for (f_cnt_t sampleIdx = 1; sampleIdx < samples; sampleIdx += 2)
			{
				outPtr[sampleIdx] = 0.f;
			}
		}

		for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
		{
			// Route to left output channel
			if constexpr ((epL & 0b01) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx + 1];
				++numRoutedL;
			}
			if constexpr ((epL & 0b10) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx];
				++numRoutedL;
			}

			// Route to right output channel
			if constexpr ((epR & 0b01) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx + 1];
				++numRoutedR;
			}
			if constexpr ((epR & 0b10) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx];
				++numRoutedR;
			}
		}
	};


	for (std::uint8_t outChannelPairIdx = 0; outChannelPairIdx < inOutSizeConstrained; ++outChannelPairIdx)
	{
		sample_t* outPtr = inOut.bus[outChannelPairIdx]->data(); // L/R track channel pair

		numRoutedL = 0;
		numRoutedR = 0;

		const std::uint8_t outChannel = outChannelPairIdx * 2;
		const std::uint8_t enabledPins =
			(static_cast<std::uint8_t>(m_out.enabled(outChannel, 0)) << 3u)
			| (static_cast<std::uint8_t>(m_out.enabled(outChannel, 1)) << 2u)
			| (static_cast<std::uint8_t>(m_out.enabled(outChannel + 1, 0)) << 1u)
			| static_cast<std::uint8_t>(m_out.enabled(outChannel + 1, 1));

		switch (enabledPins)
		{
			case 0: break;
			case 1: route2x2(outPtr, std::integral_constant<std::uint8_t, 1>{}); break;
			case 2: route2x2(outPtr, std::integral_constant<std::uint8_t, 2>{}); break;
			case 3: route2x2(outPtr, std::integral_constant<std::uint8_t, 3>{}); break;
			case 4: route2x2(outPtr, std::integral_constant<std::uint8_t, 4>{}); break;
			case 5: route2x2(outPtr, std::integral_constant<std::uint8_t, 5>{}); break;
			case 6: route2x2(outPtr, std::integral_constant<std::uint8_t, 6>{}); break;
			case 7: route2x2(outPtr, std::integral_constant<std::uint8_t, 7>{}); break;
			case 8: route2x2(outPtr, std::integral_constant<std::uint8_t, 8>{}); break;
			case 9: route2x2(outPtr, std::integral_constant<std::uint8_t, 9>{}); break;
			case 10: route2x2(outPtr, std::integral_constant<std::uint8_t, 10>{}); break;
			case 11: route2x2(outPtr, std::integral_constant<std::uint8_t, 11>{}); break;
			case 12: route2x2(outPtr, std::integral_constant<std::uint8_t, 12>{}); break;
			case 13: route2x2(outPtr, std::integral_constant<std::uint8_t, 13>{}); break;
			case 14: route2x2(outPtr, std::integral_constant<std::uint8_t, 14>{}); break;
			case 15: route2x2(outPtr, std::integral_constant<std::uint8_t, 15>{}); break;
			default:
				unreachable();
				break;
		}

		// If the number of channels routed to a specific output is <= 1, no normalization is needed,
		// otherwise each output sample needs to be divided by the number that were routed.

		// TODO: Move the normalization into route2x2?
		if (numRoutedL > 1)
		{
			if (numRoutedR > 1)
			{
				// Normalize both output channels
				for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
				{
					outPtr[sampleIdx] /= numRoutedL;
					outPtr[sampleIdx + 1] /= numRoutedR;
				}
			}
			else
			{
				// Normalize left output channel
				for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
				{
					outPtr[sampleIdx] /= numRoutedL;
				}
			}
		}
		else
		{
			// Either no input channels were routed to either output channel and output stays zeroed,
			// or only one channel was routed and normalization is not needed
			if (numRoutedR <= 1) { return; }

			// Normalize right output channel
			for (f_cnt_t sampleIdx = 1; sampleIdx < samples; sampleIdx += 2)
			{
				outPtr[sampleIdx] /= numRoutedR;
			}
		}
	}
}

} // namespace lmms

#endif // LMMS_PLUGIN_PIN_CONNECTOR_H
