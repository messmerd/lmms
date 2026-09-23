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

class QDomElement;

namespace lmms {

//! Specifies how samples should be imported
enum class SampleImportOption : std::uint8_t
{
	//! Sample will be imported as-is, even if mono or multi-channel
	Unmodified,

	//! Sample will be forced to mono
	//! @note If the original sample is stereo, it will be mixed down to mono, otherwise
	//!        channels >1 will be discarded
	ForceMono,

	//! Sample will be forced to stereo
	//! @note This was the only possible option in older versions of LMMS
	ForceStereo,

	//! Resolves to one of the other options using @ref inquireSampleImportModification
	//! @note This option cannot be used in headless mode
	Inquire
};

//! Indicates which modifications were made to a sample when imported
enum class SampleImportModification
{
	//! The original sample was not modified upon import
	Unmodified,

	//! The original sample was either stereo mixed down to mono, or multi-channel
	//! with channels >1 removed.
	ForcedMono,

	//! The original sample was mono, but was upmixed to stereo upon import
	UpmixMonoToStereo,

	//! The original sample was multi-channel, but was downmixed to stereo upon import
	DownmixMultiChannelToStereo
};

inline constexpr const char* SampleImportModificationAttributeName = "samplechannels";

//! Serialize a sample import modification to a project file
void serialize(QDomElement& elem, SampleImportModification modification,
	const char* attrName = SampleImportModificationAttributeName);

//! Deserialize a sample import modification previously saved to a project file.
//! It deserializes as a @a SampleImportOption so it can be used to import the sample.
//! @returns true if the XML attribute @a attrName existed and was valid
auto deserialize(const QDomElement& elem, SampleImportOption& out,
	const char* attrName = SampleImportModificationAttributeName) -> bool;

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
