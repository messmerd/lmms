/*
 * SampleImportOption.h
 *
 * Copyright (c) 2026 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#ifndef LMMS_SAMPLE_IMPORT_OPTION_H
#define LMMS_SAMPLE_IMPORT_OPTION_H

#include <QString>

#include "LmmsTypes.h"

namespace lmms {

//! Specifies how samples should be imported
enum class SampleImportOption : std::uint8_t
{
	//! Sample will be forced to stereo
	//! @note This was the only possible option in older versions of LMMS
	ForceStereo,

	//! Sample will be imported as-is, even if mono or multi-channel
	Unmodified,

	//! Resolves to one of the other options using @ref inquireSampleImportModification
	//! @note This option cannot be used in headless mode or saved to a project file.
	Inquire
};

//! Indicates which modifications were made to a sample when imported
enum class SampleImportModification
{
	//! The sample was already 2 channels
	Unnecessary,

	//! The sample could have required modifications, but wasn't modified.
	//! This is the only value that implies non-2-channels.
	Unmodified,

	//! Mono sample upmixed to stereo
	UpmixMonoToStereo,

	//! Multi-channel sample downmixed to stereo
	DownmixMultiChannelToStereo
};

//! Serialize a sample import modification so it can be saved to a project file
auto serialize(SampleImportModification modification) -> QString;

//! Deserialize a sample import modification previously saved to a project file.
//! It deserializes as a @a SampleImportOption so it can be used to import the sample.
auto deserializeSampleImportModification(const QString& modification) -> SampleImportOption;

//! Determines how a sample should be modified when imported
auto getSampleImportModification(SampleImportOption option, ch_cnt_t actualChannels, const QString& sampleName = {})
	-> SampleImportModification;

namespace gui {

//! Asks the user how they want to import a sample with the given channel count,
//! or uses the options specified in the config file.
auto inquireSampleImportModification(ch_cnt_t actualChannels, const QString& sampleName = {})
	-> SampleImportModification;

} // namespace gui
} // namespace lmms

#endif // LMMS_SAMPLE_IMPORT_OPTION_H
