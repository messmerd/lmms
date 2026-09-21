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
#include <QMessageBox>
#include <stdexcept>

#include "ConfigManager.h"
#include "GuiApplication.h"

namespace lmms {

auto serialize(SampleImportModification modification) -> QString
{
	switch (modification)
	{
		case SampleImportModification::Unmodified: return "0";
		case SampleImportModification::UpmixMonoToStereo: return "1";
		case SampleImportModification::DownmixMultiChannelToStereo: return "2";
		default: break;
	}

	throw std::invalid_argument{"cannot serialize"};
}

auto deserializeSampleImportModification(const QString& modification) -> SampleImportOption
{
	bool ok = false;
	const int parsed = modification.toInt(&ok);
	if (!ok) { return SampleImportOption::Legacy; }

	switch (parsed)
	{
		case 0: return SampleImportOption::Unmodified;
		case 1: return SampleImportOption::UpmixMonoToStereo;
		case 2: return SampleImportOption::DownmixMultiChannelToStereo;
		default:
		{
			qWarning() << "Unknown SampleImportOption:" << parsed;
			break;
		}
	}

	return SampleImportOption::Legacy;
}

auto getSampleImportModification(SampleImportOption options, const QString& sampleName, ch_cnt_t actualChannels)
	-> SampleImportModification
{
	if (options == SampleImportOption::Inquire)
	{
		return gui::inquireSampleImportModification(sampleName, actualChannels);
	}

	switch (options)
	{
		case SampleImportOption::Unmodified:
			return SampleImportModification::Unmodified;
		case SampleImportOption::UpmixMonoToStereo:
			return SampleImportModification::UpmixMonoToStereo;
		case SampleImportOption::DownmixMultiChannelToStereo:
			return SampleImportModification::DownmixMultiChannelToStereo;
		default:
			break;
	}

	throw std::logic_error{"Invalid SampleImportOption"};
}

namespace gui {

auto inquireSampleImportModification(const QString& sampleName, ch_cnt_t actualChannels)
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
			auto mb = QMessageBox {
				QMessageBox::Question,
				QObject::tr("Sample import preference"),
				QObject::tr("The sample '%1' is mono. Would you like to upmix it to stereo?")
					.arg(sampleName),
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
		auto mb = QMessageBox {
			QMessageBox::Question,
			QObject::tr("Sample import preference"),
			QObject::tr("The sample '%1' contains %2 channels. Would you like to downmix it to stereo?")
				.arg(sampleName).arg(actualChannels),
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
