/*
 * SampleImportOption.cpp
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

// TODO: Add the sample import options to the Settings. They shouldn't only exist in the config file.

#include "SampleImportOption.h"

#include <QCheckBox>
#include <QDebug>
#include <QDomElement>
#include <QMessageBox>
#include <stdexcept>

#include "ConfigManager.h"
#include "GuiApplication.h"

namespace lmms {

void serialize(QDomElement& elem, SampleImportModification modification, const char* attrName)
{
	int value;
	switch (modification)
	{
		case SampleImportModification::Unmodified:
			value = 0;
			break;
		case SampleImportModification::ForcedMono:
			value = 1;
			break;
		case SampleImportModification::UpmixMonoToStereo: [[fallthrough]];
		case SampleImportModification::DownmixMultiChannelToStereo:
			value = 2;
			break;
		default:
			throw std::invalid_argument{"cannot serialize"};
	}

	// Only save if not Unmodified, since Unmodified is the default
	if (value != 0)
	{
		elem.setAttribute(attrName, value);
	}
}

auto deserialize(const QDomElement& elem, SampleImportOption& out, const char* attrName) -> bool
{
	const auto modification = elem.attribute(attrName);

	bool ok = false;
	const int parsed = modification.toInt(&ok);
	if (!ok)
	{
		// Projects without a "samplechannels" XML attribute are imported unmodified.
		// Note that old LMMS projects from before multi-channel sample support was added also
		// do not contain a "samplechannels" attribute, but they are handled in a DataFile upgrade
		// routine to ensure they are forced to stereo.
		out = SampleImportOption::Unmodified;
		return false;
	}

	switch (parsed)
	{
		case 0:
			out = SampleImportOption::Unmodified;
			return true;
		case 1:
			out = SampleImportOption::ForceMono;
			return true;
		case 2:
			out = SampleImportOption::ForceStereo;
			return true;
		default:
		{
			qWarning() << "Unknown SampleImportOption:" << parsed;
			out = SampleImportOption::ForceStereo;
		}
	}

	return false;
}

auto getSampleImportModification(SampleImportOption option, ch_cnt_t actualChannels, const QString& sampleName)
	-> SampleImportModification
{
	if (option == SampleImportOption::Inquire)
	{
		return gui::inquireSampleImportModification(actualChannels, sampleName);
	}

	if (actualChannels == 2)
	{
		return option == SampleImportOption::ForceMono
			? SampleImportModification::ForcedMono
			: SampleImportModification::Unmodified;
	}

	if (actualChannels < 2)
	{
		return option == SampleImportOption::ForceStereo
			? SampleImportModification::UpmixMonoToStereo
			: SampleImportModification::Unmodified;
	}

	// multi-channel sample
	switch (option)
	{
		case SampleImportOption::ForceStereo:
			return SampleImportModification::DownmixMultiChannelToStereo;
		case SampleImportOption::ForceMono:
			return SampleImportModification::ForcedMono;
		default: break;
	}

	return SampleImportModification::Unmodified;
}

namespace gui {

auto inquireSampleImportModification(ch_cnt_t actualChannels, const QString& sampleName)
	-> SampleImportModification
{
	if (!getGUI())
	{
		throw std::logic_error{"inquireSampleImportModification cannot be called in headless mode"};
	}

	if (actualChannels == 2)
	{
		// Keep as-is
		return SampleImportModification::Unmodified;
	}

	if (actualChannels < 2)
	{
		// Mono sample
		const auto importMono = ConfigManager::inst()->value("app", "importmonosamples", "ask");
		if (importMono == "ask")
		{
			const auto question = sampleName.isEmpty()
				? QObject::tr("The sample is mono. Would you like to upmix it to stereo?")
				: QObject::tr("The sample '%1' is mono. Would you like to upmix it to stereo?")
					.arg(sampleName);

			auto mb = QMessageBox {
				QMessageBox::Question,
				QObject::tr("Sample import preference"),
				question,
				QMessageBox::Yes | QMessageBox::No // TODO: Add Cancel button?
			};

			auto* cb = new QCheckBox(QObject::tr("Do not ask again"));
			mb.setCheckBox(cb);

			const auto button = static_cast<QMessageBox::StandardButton>(mb.exec());

			SampleImportModification mod;
			switch (button)
			{
				case QMessageBox::Yes:
					mod = SampleImportModification::UpmixMonoToStereo;
					break;
				case QMessageBox::No:
					mod = SampleImportModification::Unmodified;
					break;
				default:
					throw std::runtime_error{"Unexpected option"};
			}

			if (mb.checkBox()->isChecked())
			{
				ConfigManager::inst()->setValue("app", "importmonosamples",
					mod == SampleImportModification::Unmodified ? "always" : "never"
				);
			}

			return mod;
		}

		if (importMono == "always") { return SampleImportModification::Unmodified; }
		if (importMono == "never") { return SampleImportModification::UpmixMonoToStereo; }

		qWarning() << QObject::tr("Unknown value for 'importmonosamples' attribute: '%1'. Upmixing sample to stereo.")
			.arg(importMono);
		return SampleImportModification::UpmixMonoToStereo;
	}

	// Multi-channel sample
	const auto importMultichannel = ConfigManager::inst()->value("app", "importmultichannelsamples", "ask");

	if (importMultichannel == "ask")
	{
		const auto question = sampleName.isEmpty()
			? QObject::tr("The sample contains %1 channels. Would you like to downmix it to stereo?")
				.arg(actualChannels)
			: QObject::tr("The sample '%1' contains %2 channels. Would you like to downmix it to stereo?")
				.arg(sampleName).arg(actualChannels);

		auto mb = QMessageBox {
			QMessageBox::Question,
			QObject::tr("Sample import preference"),
			question,
			QMessageBox::Yes | QMessageBox::No // TODO: Add Cancel button?
		};

		auto* cb = new QCheckBox(QObject::tr("Do not ask again"));
		mb.setCheckBox(cb);

		const auto button = static_cast<QMessageBox::StandardButton>(mb.exec());

		SampleImportModification mod;
		switch (button)
		{
			case QMessageBox::Yes:
				mod = SampleImportModification::DownmixMultiChannelToStereo;
				break;
			case QMessageBox::No:
				mod = SampleImportModification::Unmodified;
				break;
			default:
				throw std::runtime_error{"Unexpected option"};
		}

		if (mb.checkBox()->isChecked())
		{
			ConfigManager::inst()->setValue("app", "importmultichannelsamples",
				mod == SampleImportModification::Unmodified ? "always" : "never"
			);
		}

		return mod;
	}

	if (importMultichannel == "always") { return SampleImportModification::Unmodified; }
	if (importMultichannel == "never") { return SampleImportModification::DownmixMultiChannelToStereo; }

	qWarning()
		<< QObject::tr("Unknown value for 'importmultichannelsamples' attribute: '%1'. Downmixing sample to stereo.")
		.arg(importMultichannel);
	return SampleImportModification::DownmixMultiChannelToStereo;
}

} // namespace gui
} // namespace lmms
