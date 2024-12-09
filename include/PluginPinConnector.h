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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "AudioData.h"
#include "AutomatableModel.h"
#include "lmms_basics.h"
#include "lmms_export.h"
#include "SampleFrame.h"
#include "SerializingObject.h"

class QWidget;

#ifdef LMMS_TESTING
class PluginPinConnectorTest;
#endif

namespace lmms {

namespace gui {

class PluginPinConnectorView;

} // namespace gui

class PluginPinConnector;

namespace detail {

//! Base class for PluginPinConnector::Router
template<bool wetDryEnabled = false>
class RouterBase
{
public:
	RouterBase(PluginPinConnector& parent)
		: m_pc{&parent}
	{
	}
protected:
	PluginPinConnector* m_pc;
};

template<>
class RouterBase<true>
{
public:
	RouterBase(PluginPinConnector& parent, SampleFrame* wetDryBuffer, float wet, float dry)
		: m_pc{&parent}
		, m_wetDryBuffer{wetDryBuffer}
		, m_wet{wet}
		, m_dry{dry}
	{
		assert(wetDryBuffer != nullptr);
	}
protected:
	PluginPinConnector* m_pc;
	SampleFrame* m_wetDryBuffer;
	float m_wet;
	float m_dry;
};

} // namespace detail


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
		auto pins(ch_cnt_t trackChannel) const -> const std::vector<BoolModel*>& { return m_pins[trackChannel]; }

		auto channelCount() const -> int { return m_channelCount; }

		auto channelName(int channel) const -> QString;

		auto enabled(ch_cnt_t trackChannel, pi_ch_t pluginChannel) const -> bool
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

	PluginPinConnector(Model* parent);
	PluginPinConnector(int pluginChannelCountIn, int pluginChannelCountOut, Model* parent);

	/**
	 * Getters
	 */
	auto in() const -> const Matrix& { return m_in; }
	auto out() const -> const Matrix& { return m_out; }
	auto trackChannelCount() const -> std::size_t { return s_totalTrackChannels; }

	//! The pin connector is initialized once the number of in/out channels are known
	auto initialized() const -> bool { return m_in.m_channelCount != 0 || m_out.m_channelCount != 0; }

	/**
	 * Setters
	 */
	void setPluginChannelCounts(int inCount, int outCount);
	void setPluginChannelCountIn(int inCount);
	void setPluginChannelCountOut(int outCount);


	/*
	 * Pin connector router
	 *
	 * `routeToPlugin`
	 *     Routes audio from LMMS track channels to plugin inputs according to the plugin pin connector configuration.
	 *
	 *     Iterates through each output channel, mixing together all input audio routed to the output channel.
	 *     If no audio is routed to an output channel, the output channel's buffer is zeroed.
	 *
	 *     `in`     : track channels from LMMS core (currently just the main track channel pair)
	 *                `in.frames` provides the number of frames in each `in`/`out` audio buffer
	 *     `out`    : plugin input channel buffers
	 *
	 * `routeFromPlugin`
	 *     Routes audio from plugin outputs to LMMS track channels according to the plugin pin connector configuration.
	 *
	 *     Iterates through each output channel, mixing together all input audio routed to the output channel.
	 *     If no audio is routed to an output channel, `inOut` remains unchanged for audio bypass behavior.
	 *
	 *     `in`      : plugin output channel buffers
	 *     `inOut`   : track channels from/to LMMS core
	 *                 `inOut.frames` provides the number of frames in each `in`/`inOut` audio buffer
	 *
	 * The `wetDry` template parameter specifies whether wet/dry mixing should be performed
	 *     on non-bypassed outputs. This is used by effect plugins.
	 */
	template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut, bool wetDry>
	class Router;

	//! Non-`SampleFrame` routing
	template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut, bool wetDry>
	class Router : public detail::RouterBase<wetDry>
	{
	public:
		static_assert(layout == AudioDataLayout::Split, "Only split data is implemented so far");

		using detail::RouterBase<wetDry>::RouterBase;

		void routeToPlugin(CoreAudioBus in, SplitAudioData<SampleT, channelCountIn> out) const;

		void routeFromPlugin(SplitAudioData<const SampleT, channelCountOut> in, CoreAudioBusMut inOut) const;
	};

	//! `SampleFrame` routing
	template<AudioDataLayout layout, int channelCountIn, int channelCountOut, bool wetDry>
	class Router<layout, SampleFrame, channelCountIn, channelCountOut, wetDry>
		: public detail::RouterBase<wetDry>
	{
	public:
		static_assert(layout == AudioDataLayout::Interleaved);

		using detail::RouterBase<wetDry>::RouterBase;

		void routeToPlugin(CoreAudioBus in, CoreAudioDataMut out) const;

		void routeFromPlugin(CoreAudioData in, CoreAudioBusMut inOut) const;
	};


	template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut>
	auto getRouter() -> Router<layout, SampleT, channelCountIn, channelCountOut, false>
	{
		return Router<layout, SampleT, channelCountIn, channelCountOut, false>{*this};
	}

	template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut>
	auto getRouter(SampleFrame* wetDryBuffer, float wet, float dry)
		-> Router<layout, SampleT, channelCountIn, channelCountOut, true>
	{
		return Router<layout, SampleT, channelCountIn, channelCountOut, true> {
			*this, wetDryBuffer, wet, dry
		};
	}


	/**
	 * SerializingObject implementation
	 */
	void saveSettings(QDomDocument& doc, QDomElement& elem) override;
	void loadSettings(const QDomElement& elem) override;
	auto nodeName() const -> QString override { return "pins"; }

	auto instantiateView(QWidget* parent = nullptr) -> gui::PluginPinConnectorView*;
	auto getChannelCountText() const -> QString;

	static constexpr std::size_t MaxTrackChannels = 256; // TODO: Move somewhere else

#ifdef LMMS_TESTING
	friend class ::PluginPinConnectorTest;
#endif

public slots:
	void setTrackChannelCount(int count);
	void updateRoutedChannels(unsigned int trackChannel);

private:
	void updateAllRoutedChannels();

	Matrix m_in;  //!< LMMS --> Plugin
	Matrix m_out; //!< Plugin --> LMMS

	//! TODO: Move this somewhere else; Will be >= 2 once there is support for adding new track channels
	static constexpr std::size_t s_totalTrackChannels = DEFAULT_CHANNELS;

	// TODO: When full routing is added, get LMMS channel counts from bus or router class

	//! This value is <= to the total number of track channels (currently always 2)
	unsigned int m_trackChannelsUpperBound = DEFAULT_CHANNELS; // TODO: Need to recalculate when pins are set/unset

	/**
	 * Caches whether any plugin output channels are routed to a given track channel (meaning the
	 * track channel is not "bypassed"), which eliminates need for O(N) checking in `routeFromPlugin`.
	 *
	 * This means m_routedChannels[i] == true iif m_out.enabled(i, x) == true for any plugin channel x.
	 */
	std::vector<bool> m_routedChannels; // TODO: Need to calculate when pins are set/unset
};


// Non-`SampleFrame` Router out-of-class definitions

template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut, bool wetDry>
inline void PluginPinConnector::Router<layout, SampleT, channelCountIn, channelCountOut, wetDry>::routeToPlugin(
	CoreAudioBus in, SplitAudioData<SampleT, channelCountIn> out) const
{
	if constexpr (channelCountIn == 0) { return; }

	assert(this->m_pc->m_in.channelCount() != DynamicChannelCount);
	if (this->m_pc->m_in.channelCount() == 0) { return; }

	// Ignore all unused track channels for better performance
	const auto inSizeConstrained = this->m_pc->m_trackChannelsUpperBound / 2;
	assert(inSizeConstrained <= in.channelPairs);

	// Zero the output buffer - TODO: std::memcpy?
	assert(out.sourceBuffer() != nullptr);
	std::fill_n(out.sourceBuffer(), out.channels() * out.frames(), SampleT{});
	//std::memset(out.sourceBuffer(), 0, out.channels() * out.frames() * sizeof(SampleT));

	for (std::uint32_t outChannel = 0; outChannel < out.channels(); ++outChannel)
	{
		mix_ch_t numRouted = 0; // counter for # of in channels routed to the current out channel
		SampleType<layout, SampleT>* outPtr = out.buffer(outChannel);

		for (std::uint8_t inChannelPairIdx = 0; inChannelPairIdx < inSizeConstrained; ++inChannelPairIdx)
		{
			const SampleFrame* inPtr = in.bus[inChannelPairIdx]; // L/R track channel pair

			const std::uint8_t inChannel = inChannelPairIdx * 2;
			const std::uint8_t enabledPins =
				(static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel, outChannel)) << 1u)
				| static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel + 1, outChannel));

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

template<AudioDataLayout layout, typename SampleT, int channelCountIn, int channelCountOut, bool wetDry>
inline void PluginPinConnector::Router<layout, SampleT, channelCountIn, channelCountOut, wetDry>::routeFromPlugin(
	SplitAudioData<const SampleT, channelCountOut> in, CoreAudioBusMut inOut) const
{
	if constexpr (channelCountOut == 0) { return; }

	assert(this->m_pc->m_out.channelCount() != DynamicChannelCount);
	if (this->m_pc->m_out.channelCount() == 0) { return; }

	// Ignore all unused track channels for better performance
	const auto inOutSizeConstrained = this->m_pc->m_trackChannelsUpperBound / 2;
	assert(inOutSizeConstrained <= inOut.channelPairs);

	/*
	 * Routes plugin audio to track channel pair and normalizes the result. For track channels
	 * without any plugin audio routed to it, the track channel is unmodified for "bypass"
	 * behavior.
	 */
	const auto routeNx2 = [&](SampleFrame* outPtr, ch_cnt_t outChannel, auto routedChannels) {
		constexpr std::uint8_t rc = routedChannels();

		if constexpr (rc == 0b00)
		{
			// Both track channels bypassed - nothing to do
			return;
		}

		// We know at this point that we are writing to at least one of the output channels
		// rather than bypassing, so it is safe to set the output buffer of those channels
		// to zero prior to accumulation. If wet/dry mixing is to be performed, the wet/dry buffer
		// is treated as the ouput buffer instead, and the actual output buffer is multiplied by
		// the dry level rather than zeroed. Later this dry signal will be combined with the
		// wet signal (`outPtrWet`) to produce the final output signal.

		SampleFrame* outPtrWet;
		if constexpr (wetDry) { outPtrWet = this->m_wetDryBuffer; }
		else { outPtrWet = outPtr; }

		if constexpr (rc == 0b11)
		{
			// TODO: std::memcpy?
			std::fill_n(outPtrWet, inOut.frames, SampleFrame{});
			if constexpr (wetDry) { std::fill_n(outPtr, inOut.frames, SampleFrame{}); }
		}
		else
		{
			for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
			{
				if constexpr ((rc & 0b10) != 0)
				{
					outPtrWet[frame].leftRef() = 0.f;
					if constexpr (wetDry) { outPtr[frame].leftRef() *= this->m_dry; }
				}
				if constexpr ((rc & 0b01) != 0)
				{
					outPtrWet[frame].rightRef() = 0.f;
					if constexpr (wetDry) { outPtr[frame].rightRef() *= this->m_dry; }
				}
			}
		}

		// Counters for # of in channels routed to the current pair of out channels
		ch_cnt_t numRoutedL = 0;
		ch_cnt_t numRoutedR = 0;

		for (pi_ch_t inChannel = 0; inChannel < in.channels(); ++inChannel)
		{
			const SampleType<layout, const SampleT>* inPtr = in.buffer(inChannel);

			if constexpr (rc == 0b11)
			{
				// This input channel could be routed to either left, right, both, or neither output channels
				if (this->m_pc->m_out.enabled(outChannel, inChannel))
				{
					++numRoutedL;
					if (this->m_pc->m_out.enabled(outChannel + 1, inChannel))
					{
						++numRoutedR;
						for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
						{
							outPtrWet[frame].leftRef() += inPtr[frame];
							outPtrWet[frame].rightRef() += inPtr[frame];
						}
					}
					else
					{
						for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
						{
							outPtrWet[frame].leftRef() += inPtr[frame];
						}
					}
				}
				else if (this->m_pc->m_out.enabled(outChannel + 1, inChannel))
				{
					++numRoutedR;
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtrWet[frame].rightRef() += inPtr[frame];
					}
				}
			}
			else if constexpr (rc == 0b10)
			{
				// This input channel may or may not be routed to the left output channel
				if (!this->m_pc->m_out.enabled(outChannel, inChannel)) { continue; }

				++numRoutedL;
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					outPtrWet[frame].leftRef() += inPtr[frame];
				}
			}
			else if constexpr (rc == 0b01)
			{
				// This input channel may or may not be routed to the right output channel
				if (!this->m_pc->m_out.enabled(outChannel + 1, inChannel)) { continue; }

				++numRoutedR;
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					outPtrWet[frame].rightRef() += inPtr[frame];
				}
			}
		}

		// Lastly, normalize wet signal (divide accumulated plugin outputs with number of channels routed),
		// and if wet/dry processing is being performed, multiply with the wet level and combine
		// that with the dry signal.

		if constexpr (wetDry)
		{
			// If num routed is not 0, we need to combine wet signal with dry signal in the real output.
			// The equation is `out = in * dryLevel + (pluginOut / numRouted) * wetLevel`.
			// Normalization is not needed when num routed == 1, but for simplicity's sake
			// that optimization is not taken here.

			if (numRoutedL != 0)
			{
				if (numRoutedR != 0)
				{
					// Normalize wet signal (both channels) and combine with dry signal in output
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtr[frame].leftRef() += (outPtrWet[frame].left() / numRoutedL) * this->m_wet;
						outPtr[frame].rightRef() += (outPtrWet[frame].right() / numRoutedR) * this->m_wet;
					}
				}
				else
				{
					// Normalize wet signal (left channel) and combine with dry signal in output
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtr[frame].leftRef() += (outPtrWet[frame].left() / numRoutedL) * this->m_wet;
					}
				}
			}
			else if (numRoutedR != 0)
			{
				// Normalize wet signal (right channel) and combine with dry signal in output
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					outPtr[frame].rightRef() += (outPtrWet[frame].right() / numRoutedR) * this->m_wet;
				}
			}
		}
		else
		{
			// If num routed is 0 or 1, either no plugin channels were routed to the output
			// and the output stays zeroed, or only one channel was routed and normalization is not needed

			if (numRoutedL > 1)
			{
				if (numRoutedR > 1)
				{
					// Normalize output (both channels)
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtrWet[frame].leftRef() /= numRoutedL;
						outPtrWet[frame].rightRef() /= numRoutedR;
					}
				}
				else
				{
					// Normalize output (left channel)
					for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
					{
						outPtrWet[frame].leftRef() /= numRoutedL;
					}
				}
			}
			else if (numRoutedR > 1)
			{
				// Normalize output (right channel)
				for (f_cnt_t frame = 0; frame < inOut.frames; ++frame)
				{
					outPtrWet[frame].rightRef() /= numRoutedR;
				}
			}
		}
	};


	for (ch_cnt_t outChannelPairIdx = 0; outChannelPairIdx < inOutSizeConstrained; ++outChannelPairIdx)
	{
		SampleFrame* outPtr = inOut.bus[outChannelPairIdx]; // L/R track channel pair
		const auto outChannel = static_cast<ch_cnt_t>(outChannelPairIdx * 2);

		const std::uint8_t routedChannels =
				(static_cast<std::uint8_t>(this->m_pc->m_routedChannels[outChannel]) << 1u)
				| static_cast<std::uint8_t>(this->m_pc->m_routedChannels[outChannel + 1]);

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

// `SampleFrame` Router out-of-class definitions

template<AudioDataLayout layout, int channelCountIn, int channelCountOut, bool wetDry>
inline void PluginPinConnector::Router<layout, SampleFrame, channelCountIn, channelCountOut, wetDry>::routeToPlugin(
	CoreAudioBus in, CoreAudioDataMut out) const
{
	if constexpr (channelCountIn == 0) { return; }

	assert(this->m_pc->m_in.channelCount() != DynamicChannelCount);
	if (this->m_pc->m_in.channelCount() == 0) { return; }
	assert(this->m_pc->m_in.channelCount() == 2); // SampleFrame routing only allows exactly 0 or 2 channels

	// Ignore all unused track channels for better performance
	const auto inSizeConstrained = this->m_pc->m_trackChannelsUpperBound / 2;
	assert(inSizeConstrained <= in.channelPairs);

	// Zero the output buffer - TODO: std::memcpy?
	assert(out.data() != nullptr);
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
	 */
	auto route2x2 = [&, samples, outPtr](const sample_t* inPtr, auto enabledPins) {
		constexpr auto epL =  static_cast<std::uint8_t>(enabledPins() >> 2); // for L out channel
		constexpr auto epR = static_cast<std::uint8_t>(enabledPins() & 0b0011); // for R out channel

		if constexpr (enabledPins() == 0) { return; }

		if constexpr (epL == 0b11) { numRoutedL += 2; }
		else if constexpr (epL != 0) { ++numRoutedL; }

		if constexpr (epR == 0b11) { numRoutedR += 2; }
		else if constexpr (epR != 0) { ++numRoutedR; }

		for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
		{
			// Route to left output channel
			if constexpr ((epL & 0b01) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx + 1];
			}
			if constexpr ((epL & 0b10) != 0)
			{
				outPtr[sampleIdx] += inPtr[sampleIdx];
			}

			// Route to right output channel
			if constexpr ((epR & 0b01) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx + 1];
			}
			if constexpr ((epR & 0b10) != 0)
			{
				outPtr[sampleIdx + 1] += inPtr[sampleIdx];
			}
		}
	};


	for (std::uint8_t inChannelPairIdx = 0; inChannelPairIdx < inSizeConstrained; ++inChannelPairIdx)
	{
		const sample_t* inPtr = in.bus[inChannelPairIdx]->data(); // L/R track channel pair

		const std::uint8_t inChannel = inChannelPairIdx * 2;
		const std::uint8_t enabledPins =
			(static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel, 0)) << 3u)
			| (static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel + 1, 0)) << 2u)
			| (static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel, 1)) << 1u)
			| static_cast<std::uint8_t>(this->m_pc->m_in.enabled(inChannel + 1, 1));

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

template<AudioDataLayout layout, int channelCountIn, int channelCountOut, bool wetDry>
inline void PluginPinConnector::Router<layout, SampleFrame, channelCountIn, channelCountOut, wetDry>::routeFromPlugin(
	CoreAudioData in, CoreAudioBusMut inOut) const
{
	if constexpr (channelCountOut == 0) { return; }

	assert(this->m_pc->m_out.channelCount() != DynamicChannelCount);
	if (this->m_pc->m_out.channelCount() == 0) { return; }
	assert(this->m_pc->m_out.channelCount() == 2); // SampleFrame routing only allows exactly 0 or 2 channels

	// Ignore all unused track channels for better performance
	const auto inOutSizeConstrained = this->m_pc->m_trackChannelsUpperBound / 2;
	assert(inOutSizeConstrained <= inOut.channelPairs);

	/*
	 * This is essentially a function template with specializations for each
	 * of the 16 total routing combinations of an input `SampleFrame*` to an
	 * output `SampleFrame*`. The purpose is to eliminate all branching within
	 * the inner for-loop in hopes of better performance.
	 */
	auto route2x2 = [this, samples = inOut.frames * 2, inPtr = in.data()->data()](sample_t* outPtr, auto enabledPins) {
		constexpr auto epL =  static_cast<std::uint8_t>(enabledPins() >> 2); // for L out channel
		constexpr auto epR = static_cast<std::uint8_t>(enabledPins() & 0b0011); // for R out channel

		if constexpr (enabledPins() == 0) { return; }

		// We know at this point that we are writing to at least one of the output channels
		// rather than bypassing, so it is safe to overwrite the contents of the output buffer

		for (f_cnt_t sampleIdx = 0; sampleIdx < samples; sampleIdx += 2)
		{
			if constexpr (wetDry)
			{
				// Route to left output channel and perform wet/dry mixing
				if constexpr (epL == 0b11)
				{
					outPtr[sampleIdx] = outPtr[sampleIdx] * this->m_dry
						+ ((inPtr[sampleIdx] + inPtr[sampleIdx + 1]) / 2) * this->m_wet;
				}
				else if constexpr (epL == 0b01)
				{
					outPtr[sampleIdx] = outPtr[sampleIdx] * this->m_dry + inPtr[sampleIdx + 1] * this->m_wet;
				}
				else if constexpr (epL == 0b10)
				{
					outPtr[sampleIdx] = outPtr[sampleIdx] * this->m_dry + inPtr[sampleIdx] * this->m_wet;
				}

				// Route to right output channel and perform wet/dry mixing
				if constexpr (epR == 0b11)
				{
					outPtr[sampleIdx + 1] = outPtr[sampleIdx + 1] * this->m_dry
						+ ((inPtr[sampleIdx] + inPtr[sampleIdx + 1]) / 2) * this->m_wet;
				}
				else if constexpr (epR == 0b01)
				{
					outPtr[sampleIdx + 1] = outPtr[sampleIdx + 1] * this->m_dry + inPtr[sampleIdx + 1] * this->m_wet;
				}
				else if constexpr (epR == 0b10)
				{
					outPtr[sampleIdx + 1] = outPtr[sampleIdx + 1] * this->m_dry + inPtr[sampleIdx] * this->m_wet;
				}
			}
			else
			{
				(void)this;

				// Route to left output channel
				if constexpr (epL == 0b11)
				{
					outPtr[sampleIdx] = (inPtr[sampleIdx] + inPtr[sampleIdx + 1]) / 2;
				}
				else if constexpr (epL == 0b01)
				{
					outPtr[sampleIdx] = inPtr[sampleIdx + 1];
				}
				else if constexpr (epL == 0b10)
				{
					outPtr[sampleIdx] = inPtr[sampleIdx];
				}

				// Route to right output channel
				if constexpr (epR == 0b11)
				{
					outPtr[sampleIdx + 1] = (inPtr[sampleIdx] + inPtr[sampleIdx + 1]) / 2;
				}
				else if constexpr (epR == 0b01)
				{
					outPtr[sampleIdx + 1] = inPtr[sampleIdx + 1];
				}
				else if constexpr (epR == 0b10)
				{
					outPtr[sampleIdx + 1] = inPtr[sampleIdx];
				}
			}
		}
	};


	for (ch_cnt_t outChannelPairIdx = 0; outChannelPairIdx < inOutSizeConstrained; ++outChannelPairIdx)
	{
		sample_t* outPtr = inOut.bus[outChannelPairIdx]->data(); // L/R track channel pair
		assert(outPtr != nullptr);

		const ch_cnt_t outChannel = outChannelPairIdx * 2;
		const std::uint8_t enabledPins =
			(static_cast<std::uint8_t>(this->m_pc->m_out.enabled(outChannel, 0)) << 3u)
			| (static_cast<std::uint8_t>(this->m_pc->m_out.enabled(outChannel, 1)) << 2u)
			| (static_cast<std::uint8_t>(this->m_pc->m_out.enabled(outChannel + 1, 0)) << 1u)
			| static_cast<std::uint8_t>(this->m_pc->m_out.enabled(outChannel + 1, 1));

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
	}
}


} // namespace lmms

#endif // LMMS_PLUGIN_PIN_CONNECTOR_H
