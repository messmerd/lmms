/*
 * SampleDecoder.cpp - Decodes audio files in various formats
 *
 * Copyright (c) 2023 saker <sakertooth@gmail.com>
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

#include "SampleDecoder.h"

#include <QFile>
#include <QString>
#include <memory>
#include <sndfile.h>

#ifdef LMMS_HAVE_OGGVORBIS
#include <vorbis/vorbisfile.h>
#endif

#include "AudioEngine.h"
#include "DrumSynth.h"
#include "Engine.h"
#include "LmmsTypes.h"
#include "MixHelpers.h"

namespace lmms {

namespace {

using Decoder = std::optional<SampleDecoder::Result> (*)(const QString&, SampleImportOption);

auto decodeSampleSF(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>;
auto decodeSampleDS(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>;
#ifdef LMMS_HAVE_OGGVORBIS
auto decodeSampleOggVorbis(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>;
#endif

static constexpr std::array<Decoder, 3> decoders = {&decodeSampleSF,
#ifdef LMMS_HAVE_OGGVORBIS
	&decodeSampleOggVorbis,
#endif
	&decodeSampleDS};

void postProcess(AudioBuffer& dst, SampleImportModification mod,
	const float* src, ch_cnt_t srcChannels)
{
	const auto frames = dst.frames();
	if (mod == SampleImportModification::UpmixMonoToStereo)
	{
		// Upmix mono sample to stereo
		assert(srcChannels == 1);
		const float* channelBuffer[1] = { src };
		const auto srcAsPlanar = PlanarBufferView{channelBuffer, 1, frames};

		MixHelpers::monoUpmix(dst.allBuffers(), srcAsPlanar);
	}
	else
	{
		const auto srcAsInterleaved = InterleavedBufferView{src, srcChannels, frames};
		auto dstBuffers = dst.allBuffers();

		if (mod == SampleImportModification::ForcedMono)
		{
			f_cnt_t idx = 0;
			if (srcChannels == 2)
			{
				// Downmix stereo to mono
				for (const float* frame : srcAsInterleaved.framesView())
				{
					dstBuffers[0][idx] = (frame[0] + frame[1]) / 2;
					++idx;
				}
			}
			else
			{
				// Multichannel - discard channels >1
				assert(srcChannels > 2);
				for (const float* frame : srcAsInterleaved.framesView())
				{
					dstBuffers[0][idx] = frame[0];
					++idx;
				}
			}
		}
		else if (mod == SampleImportModification::DownmixMultiChannelToStereo)
		{
			assert(srcChannels > 2);
			f_cnt_t idx = 0;
			for (const float* frame : srcAsInterleaved.framesView())
			{
				dstBuffers[0][idx] = frame[0];
				dstBuffers[1][idx] = frame[1];
				++idx;
			}
		}
		else
		{
			assert(mod == SampleImportModification::Unmodified);
			toPlanar(srcAsInterleaved, dstBuffers);
		}
	}
}

auto decodeSampleSF(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>
{
	SNDFILE* sndFile = nullptr;
	auto sfInfo = SF_INFO{};

	// TODO: Remove use of QFile
	auto file = QFile{audioFile};
	if (!file.open(QIODevice::ReadOnly)) { return std::nullopt; }

	sndFile = sf_open_fd(file.handle(), SFM_READ, &sfInfo, false);
	if (sf_error(sndFile) != 0) { return std::nullopt; }

	auto buffer = std::vector<float>(sfInfo.channels * sfInfo.frames);
	sf_read_float(sndFile, buffer.data(), buffer.size());

	sf_close(sndFile);
	file.close();

	// Determine what modifications to make to the sample we're importing (if any)
	const auto mod = getSampleImportModification(option, static_cast<ch_cnt_t>(sfInfo.channels), audioFile);
	ch_cnt_t desiredChannels;
	switch (mod)
	{
		case SampleImportModification::Unmodified:
			desiredChannels = static_cast<ch_cnt_t>(sfInfo.channels);
			break;
		case SampleImportModification::ForcedMono:
			desiredChannels = 1;
			break;
		default:
			assert(false);
			[[fallthrough]];
		case SampleImportModification::UpmixMonoToStereo: [[fallthrough]];
		case SampleImportModification::DownmixMultiChannelToStereo:
			desiredChannels = 2;
			break;
	}

	const auto frames = static_cast<f_cnt_t>(sfInfo.frames);
	auto result = AudioBuffer{frames, desiredChannels};

	postProcess(result, mod, buffer.data(), static_cast<ch_cnt_t>(sfInfo.channels));

	return SampleDecoder::Result{std::move(result), static_cast<int>(sfInfo.samplerate), mod};
}

auto decodeSampleDS(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>
{
	// Populated by DrumSynth::GetDSFileSamples
	int_sample_t* dataPtr = nullptr;

	auto ds = DrumSynth{};
	const auto engineRate = Engine::audioEngine()->outputSampleRate();
	const auto frames = ds.GetDSFileSamples(audioFile, dataPtr, 1, engineRate);
	const auto data = std::unique_ptr<int_sample_t[]>{dataPtr}; // NOLINT, we have to use a C-style array here

	if (frames <= 0 || !data) { return std::nullopt; }

	// Determine what modifications to make to the sample we're importing (if any)
	const auto mod = getSampleImportModification(option, 1, audioFile);
	ch_cnt_t desiredChannels;
	switch (mod)
	{
		case SampleImportModification::Unmodified: [[fallthrough]];
		case SampleImportModification::ForcedMono:
			desiredChannels = 1;
			break;
		default:
			[[fallthrough]];
		case SampleImportModification::DownmixMultiChannelToStereo:
			assert(false);
			[[fallthrough]];
		case SampleImportModification::UpmixMonoToStereo:
			desiredChannels = 2;
			break;
	}

	auto result = AudioBuffer{frames, desiredChannels};

	src_short_to_float_array(data.get(), result.buffer(0).data(), frames);
	if (mod == SampleImportModification::UpmixMonoToStereo)
	{
		std::ranges::copy(result.buffer(0), result.buffer(1).data());
	}

	return SampleDecoder::Result{std::move(result), static_cast<int>(engineRate), mod};
}

#ifdef LMMS_HAVE_OGGVORBIS
auto decodeSampleOggVorbis(const QString& audioFile, SampleImportOption option)
	-> std::optional<SampleDecoder::Result>
{
	static auto s_read = [](void* buffer, size_t size, size_t count, void* stream) -> size_t {
		auto file = static_cast<QFile*>(stream);
		return file->read(static_cast<char*>(buffer), size * count);
	};

	static auto s_seek = [](void* stream, ogg_int64_t offset, int whence) -> int {
		auto file = static_cast<QFile*>(stream);
		if (whence == SEEK_SET) { file->seek(offset); }
		else if (whence == SEEK_CUR) { file->seek(file->pos() + offset); }
		else if (whence == SEEK_END) { file->seek(file->size() + offset); }
		else { return -1; }
		return 0;
	};

	static auto s_close = [](void* stream) -> int {
		auto file = static_cast<QFile*>(stream);
		file->close();
		return 0;
	};

	static auto s_tell = [](void* stream) -> long {
		auto file = static_cast<QFile*>(stream);
		return file->pos();
	};

	static ov_callbacks s_callbacks = {s_read, s_seek, s_close, s_tell};

	// TODO: Remove use of QFile
	auto file = QFile{audioFile};
	if (!file.open(QIODevice::ReadOnly)) { return std::nullopt; }

	auto vorbisFile = OggVorbis_File{};
	if (ov_open_callbacks(&file, &vorbisFile, nullptr, 0, s_callbacks) < 0) { return std::nullopt; }

	const auto vorbisInfo = ov_info(&vorbisFile, -1);
	if (vorbisInfo == nullptr) { return std::nullopt; }

	const auto numChannels = vorbisInfo->channels;
	const auto sampleRate = vorbisInfo->rate;
	const auto numSamples = ov_pcm_total(&vorbisFile, -1);
	if (numSamples < 0) { return std::nullopt; }

	auto buffer = std::vector<float>(numSamples);
	auto output = static_cast<float**>(nullptr);

	auto totalSamplesRead = 0;
	while (true)
	{
		auto samplesRead = ov_read_float(&vorbisFile, &output, numSamples, 0);

		if (samplesRead < 0) { return std::nullopt; }
		else if (samplesRead == 0) { break; }

		std::copy_n(*output, samplesRead, buffer.begin() + totalSamplesRead);
		totalSamplesRead += samplesRead;
	}

	// Determine what modifications to make to the sample we're importing (if any)
	const auto mod = getSampleImportModification(option, numChannels, audioFile);
	ch_cnt_t desiredChannels;
	switch (mod)
	{
		case SampleImportModification::Unmodified:
			desiredChannels = static_cast<ch_cnt_t>(numChannels);
			break;
		case SampleImportModification::ForcedMono:
			desiredChannels = 1;
			break;
		default:
			assert(false);
			[[fallthrough]];
		case SampleImportModification::UpmixMonoToStereo: [[fallthrough]];
		case SampleImportModification::DownmixMultiChannelToStereo:
			desiredChannels = 2;
			break;
	}

	const auto frames = static_cast<f_cnt_t>(totalSamplesRead / numChannels);
	auto result = AudioBuffer{frames, desiredChannels};

	postProcess(result, mod, buffer.data(), static_cast<ch_cnt_t>(numChannels));

	ov_clear(&vorbisFile);

	return SampleDecoder::Result{std::move(result), static_cast<int>(sampleRate), mod};
}
#endif // LMMS_HAVE_OGGVORBIS
} // namespace

auto SampleDecoder::supportedAudioTypes() -> const std::vector<AudioType>&
{
	static const auto s_audioTypes = [] {
		auto types = std::vector<AudioType>();

		// Add DrumSynth by default since that support comes from us
		types.push_back(AudioType{"DrumSynth", "ds"});

		auto sfFormatInfo = SF_FORMAT_INFO{};
		auto simpleTypeCount = 0;
		sf_command(nullptr, SFC_GET_SIMPLE_FORMAT_COUNT, &simpleTypeCount, sizeof(int));

		// TODO: Ideally, this code should be iterating over the major formats, but some important extensions such as
		// *.ogg are not included. This is planned for future versions of sndfile.
		for (int simple = 0; simple < simpleTypeCount; ++simple)
		{
			sfFormatInfo.format = simple;
			sf_command(nullptr, SFC_GET_SIMPLE_FORMAT, &sfFormatInfo, sizeof(sfFormatInfo));

			auto it = std::find_if(types.begin(), types.end(),
				[&](const AudioType& type) { return sfFormatInfo.extension == type.extension; });
			if (it != types.end()) { continue; }

			auto name = std::string{sfFormatInfo.extension};
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return std::toupper(ch); });

			types.push_back(AudioType{std::move(name), sfFormatInfo.extension});
		}

		std::sort(types.begin(), types.end(), [&](const AudioType& a, const AudioType& b) { return a.name < b.name; });
		return types;
	}();
	return s_audioTypes;
}

auto SampleDecoder::decode(const QString& audioFile, SampleImportOption option) -> std::optional<Result>
{
	auto result = std::optional<Result>{};
	for (const auto& decoder : decoders)
	{
		result = decoder(audioFile, option);
		if (result) { break; }
	}

	return result;
}

} // namespace lmms
