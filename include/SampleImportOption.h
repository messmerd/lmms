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

//! Specifies how samples should be imported (or how they were imported)
enum class SampleImportOption : std::uint8_t
{
	//! Sample imported as-is, even if mono or multi-channel
	Unmodified                  = 0,

	//! Mono sample upmixed to stereo
	UpmixMonoToStereo           = 1 << 0,

	//! Multi-channel sample downmixed to stereo
	DownmixMultiChannelToStereo = 1 << 1,

	//! The import behavior in older versions of LMMS which lacked both mono and multi-channel support.
	//! Samples imported this way are always stereo.
	Legacy                      = UpmixMonoToStereo | DownmixMultiChannelToStereo,

	//! Resolves to one of the other options using @ref inquireSampleImportModification
	//! @note This option cannot be used in headless mode or saved to a project file.
	Inquire                     = 1 << 2
};

//! Indicates which modifications were made to a sample when imported.
enum class SampleImportModification
{
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
auto getSampleImportModification(SampleImportOption options, const QString& sampleName, ch_cnt_t actualChannels)
	-> SampleImportModification;

namespace gui {

//! Asks the user how they want to import a sample with the given channel count,
//! or uses the options specified in the config file.
auto inquireSampleImportModification(const QString& sampleName, ch_cnt_t actualChannels)
	-> SampleImportModification;

} // namespace gui
} // namespace lmms

#endif // LMMS_SAMPLE_IMPORT_OPTION_H
