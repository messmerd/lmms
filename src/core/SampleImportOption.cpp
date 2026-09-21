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
		case SampleImportModification::Unnecessary: return "0";
		case SampleImportModification::Unmodified: return "1";
		case SampleImportModification::UpmixMonoToStereo: return "2";
		case SampleImportModification::DownmixMultiChannelToStereo: return "3";
		default: break;
	}

	throw std::invalid_argument{"cannot serialize"};
}

auto deserializeSampleImportModification(const QString& modification) -> SampleImportOption
{
	bool ok = false;
	const int parsed = modification.toInt(&ok);
	if (!ok) { return SampleImportOption::ForceStereo; }

	switch (parsed)
	{
		case 1: return SampleImportOption::Unmodified;
		case 0: [[fallthrough]];
		case 2: [[fallthrough]];
		case 3: return SampleImportOption::ForceStereo;
		default:
		{
			qWarning() << "Unknown SampleImportOption:" << parsed;
			break;
		}
	}

	return SampleImportOption::ForceStereo;
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
		return SampleImportModification::Unnecessary;
	}

	if (actualChannels < 2)
	{
		return option == SampleImportOption::ForceStereo
			? SampleImportModification::UpmixMonoToStereo
			: SampleImportModification::Unmodified;
	}

	return option == SampleImportOption::ForceStereo
		? SampleImportModification::DownmixMultiChannelToStereo
		: SampleImportModification::Unmodified;
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
		return SampleImportModification::Unnecessary;
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
