/*
 * ClapControlBase.cpp - CLAP control base class
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

#include "ClapControlBase.h"

#ifdef LMMS_HAVE_CLAP

#include <algorithm>
#include <cassert>

#include "ClapManager.h"
#include "ClapInstance.h"
#include "ClapTransport.h"
#include "Engine.h"

namespace lmms
{

ClapControlBase::ClapControlBase(Model* that, const QString& uri)
{
	init(that, uri.toStdString());

#if 0
	/*
		loc: flag: 1 name: Factory ROM kind: 1 location:
		clapBeginPreset: name: AR Dirty Square FD loadKey: patches/Factory Arps/AR Dirty Square FD.synthpatch
		clapBeginPreset: name: AR Everlasting 1 loadKey: patches/Factory Arps/AR Everlasting 1.synthpatch
		clapBeginPreset: name: AR Everlasting 2 loadKey: patches/Factory Arps/AR Everlasting 2.synthpatch
	*/

	PresetLoadData test; // Nakst Apricot
	test.location = "internal:"; // internal plugin
	test.loadKey = "patches/Factory Arps/AR Everlasting 2.synthpatch";

	ClapLog::globalLog(CLAP_LOG_INFO, "~~~PRESET LOAD START~~~");
	if (loadPreset(test))
	{
		ClapLog::globalLog(CLAP_LOG_INFO, "Success!");
	}
	else
	{
		ClapLog::globalLog(CLAP_LOG_INFO, "FAILURE");
	}

#endif
}

void ClapControlBase::init(Model* that, const std::string& uri)
{
	// CLAP API requires main thread for plugin loading
	assert(ClapThreadCheck::isMainThread());

	m_valid = false;
	auto manager = Engine::getClapManager();
	m_info = manager->pluginInfo(uri);
	if (!m_info)
	{
		std::string msg = "No plugin found for ID \"" + uri + "\"";
		ClapLog::globalLog(CLAP_LOG_ERROR, msg);
		return;
	}

	ClapTransport::update();

	ClapLog::globalLog(CLAP_LOG_DEBUG, "Creating CLAP instance (#1)");
	m_instances.clear();
	if (auto first = ClapInstance::create(*m_info, that))
	{
		if (first->audioPorts().hasStereoOutput())
		{
			// Stereo plugin - Only one instance needed for stereo output
			m_instances.emplace_back(std::move(first));
		}
		else
		{
			// Mono plugin - A second instance is needed for stereo output
			ClapLog::globalLog(CLAP_LOG_DEBUG, "Creating CLAP instance (#2)");
			if (auto second = ClapInstance::create(*m_info, that))
			{
				m_instances.emplace_back(std::move(first));
				m_instances.emplace_back(std::move(second));
			}
			else
			{
				ClapLog::globalLog(CLAP_LOG_ERROR, "Failed instantiating CLAP instance (#2)");
				return;
			}
		}
	}
	else
	{
		ClapLog::globalLog(CLAP_LOG_ERROR, "Failed instantiating CLAP instance (#1)");
		return;
	}

	m_channelsPerInstance = DEFAULT_CHANNELS / m_instances.size();
	m_valid = true;

	m_parameters.linkAllModels();
	m_presets.linkAllModels();

	//linkAllModels();
}

auto ClapControlBase::Parameters::getGroup(std::size_t idx) -> LinkedModelGroup*
{
	const auto instance = m_parent->control(idx);
	if (!instance) { return nullptr; }
	return &instance->params();
}

auto ClapControlBase::Parameters::getGroup(std::size_t idx) const -> const LinkedModelGroup*
{
	const auto instance = m_parent->control(idx);
	if (!instance) { return nullptr; }
	return &instance->params();
}

void ClapControlBase::Presets::saveSettings(QDomDocument& doc, QDomElement& elem)
{
	ClapLog::globalLog(CLAP_LOG_DEBUG, "Saving presets");

	// When saving a project file, we only want to save the active preset, not all presets
	if (elem.ownerDocument().doctype().name() != "clonedtrack")
	{
		if (m_parent->hasPresetSupport())
		{
			for (auto& instance :  m_parent->m_instances)
			{
				instance->presetLoader().saveActivePreset(doc, elem);
			}
		}
	}
	else
	{
		LinkedModelGroups::saveSettings(doc, elem);
	}
}

void ClapControlBase::Presets::loadSettings(const QDomElement& elem)
{
	ClapLog::globalLog(CLAP_LOG_DEBUG, "Loading presets");

	// When loading a project file, we only want to load the active preset, not all presets
	if (elem.ownerDocument().doctype().name() != "clonedtrack")
	{
		if (m_parent->hasPresetSupport())
		{
			for (auto& instance :  m_parent->m_instances)
			{
				instance->presetLoader().loadActivePreset(elem);
			}
		}
	}
	else
	{
		LinkedModelGroups::loadSettings(elem);
	}
}

auto ClapControlBase::Presets::getGroup(std::size_t idx) -> LinkedModelGroup*
{
	const auto instance = m_parent->control(idx);
	if (!instance) { return nullptr; }
	return &instance->presetLoader();
}

auto ClapControlBase::Presets::getGroup(std::size_t idx) const -> const LinkedModelGroup*
{
	const auto instance = m_parent->control(idx);
	if (!instance) { return nullptr; }
	return &instance->presetLoader();
}

/*
auto ClapControlBase::getGroup(std::size_t idx) -> LinkedModelGroup*
{
	return (idx < m_instances.size()) ? &m_instances[idx]->params() : nullptr;
}

auto ClapControlBase::getGroup(std::size_t idx) const -> const LinkedModelGroup*
{
	return (idx < m_instances.size()) ? &m_instances[idx]->params() : nullptr;
}
*/

void ClapControlBase::copyModelsFromLmms()
{
	for (const auto& instance : m_instances)
	{
		instance->copyModelsFromCore();
	}
}

void ClapControlBase::copyModelsToLmms() const
{
	for (const auto& instance : m_instances)
	{
		instance->copyModelsToCore();
	}
}

void ClapControlBase::copyBuffersFromLmms(const sampleFrame* buf, fpp_t frames)
{
	unsigned firstChan = 0; // tell the instances which channels they shall read from
	for (const auto& instance : m_instances) 
	{
		instance->audioPorts().copyBuffersFromCore(buf, firstChan, m_channelsPerInstance, frames);
		firstChan += m_channelsPerInstance;
	}
}

void ClapControlBase::copyBuffersToLmms(sampleFrame* buf, fpp_t frames) const
{
	unsigned firstChan = 0; // tell the instances which channels they shall write to
	for (const auto& instance : m_instances)
	{
		instance->audioPorts().copyBuffersToCore(buf, firstChan, m_channelsPerInstance, frames);
		firstChan += m_channelsPerInstance;
	}
}

void ClapControlBase::run(fpp_t frames)
{
	for (const auto& instance : m_instances)
	{
		instance->run(frames);
	}
}

void ClapControlBase::saveSettings(QDomDocument& doc, QDomElement& elem)
{
	elem.setAttribute("version", "0");

	if (elem.ownerDocument().doctype().name() != "clonedtrack")
	{
		// Saving to project file
		//LinkedModelGroups::saveSettings(doc, elem);
		m_presets.saveSettings(doc, elem);
		m_parameters.saveSettings(doc, elem);
		return;
	}

	// Cloning an instrument/effect - use clap_state if possible

	if (stateSupported())
	{
		m_presets.saveSettings(doc, elem);
		for (unsigned int idx = 0; idx < static_cast<unsigned int>(m_instances.size()); ++idx)
		{
			const auto state = m_instances[idx]->state().save(ClapState::Context::Duplicate);
			elem.setAttribute("state" + QString::number(idx),
				state ? QString::fromUtf8(state->data(), state->size()) : QString{});
		}
	}
	else
	{
		//LinkedModelGroups::saveSettings(doc, elem);
		m_presets.saveSettings(doc, elem);
		m_parameters.saveSettings(doc, elem);
	}
}

void ClapControlBase::loadSettings(const QDomElement& elem)
{
	[[maybe_unused]] const auto version = elem.attribute("version", "0").toInt();

	if (elem.ownerDocument().doctype().name() != "clonedtrack")
	{
		// Loading from project file
		//LinkedModelGroups::loadSettings(elem);
		m_presets.loadSettings(elem);
		m_parameters.loadSettings(elem);
		return;
	}

	// Cloning an instrument/effect - use clap_state if possible

	if (stateSupported())
	{
		m_presets.loadSettings(elem);
		for (unsigned int idx = 0; idx < m_instances.size(); ++idx)
		{
			const auto state = elem.attribute("state" + QString::number(idx), "").toStdString();
			if (!m_instances[idx]->state().load(state, ClapState::Context::Duplicate)) { continue; }

			// Parameters may have changed in the plugin;
			// Those values need to be reflected in host
			m_instances[idx]->params().rescan(CLAP_PARAM_RESCAN_VALUES);
		}
	}
	else
	{
		//LinkedModelGroups::loadSettings(elem);
		m_presets.loadSettings(elem);
		m_parameters.loadSettings(elem);
	}
}

void ClapControlBase::loadFile([[maybe_unused]] const QString& file)
{
	// TODO: load preset using clap_plugin_preset_load if supported by plugin
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapControlBase::loadFile() [NOT IMPLEMENTED YET]");
}

auto ClapControlBase::stateSupported() const -> bool
{
	return std::all_of(m_instances.begin(), m_instances.end(),
		[](const auto& instance) { return instance->state().supported(); });
}

void ClapControlBase::reload()
{
	for (const auto& instance : m_instances)
	{
		instance->restart();
	}
}

auto ClapControlBase::controlCount() const -> std::size_t
{
	std::size_t res = 0;
	for (const auto& instance : m_instances)
	{
		res += instance->controlCount();
	}
	return res;
}

auto ClapControlBase::control(std::size_t idx) -> ClapInstance*
{
	if (idx >= m_instances.size()) { return nullptr; }
	return m_instances[idx].get();
}

auto ClapControlBase::control(std::size_t idx) const -> const ClapInstance*
{
	if (idx >= m_instances.size()) { return nullptr; }
	return m_instances[idx].get();
}

auto ClapControlBase::hasNoteInput() const -> bool
{
	return std::all_of(m_instances.begin(), m_instances.end(),
		[](const auto& instance) { return instance->hasNoteInput(); });
}

auto ClapControlBase::hasPresetSupport() const -> bool
{
	return std::all_of(m_instances.begin(), m_instances.end(),
		[](const auto& instance) { return instance->presetLoader().supported(); });
}

void ClapControlBase::handleMidiInputEvent(const MidiEvent& event, const TimePos& time, f_cnt_t offset)
{
	for (const auto& instance : m_instances)
	{
		instance->handleMidiInputEvent(event, time, offset);
	}
}

auto ClapControlBase::loadPreset(const PresetLoadData& preset) -> bool
{
	for (const auto& instance : m_instances)
	{
		instance->presetLoader().activatePreset(preset);
	}
	return true;
}

auto ClapControlBase::savePreset() -> bool
{
	// [NOT IMPLEMENTED YET]
	return false;
}

void ClapControlBase::idle()
{
	for (const auto& instance : m_instances)
	{
		instance->idle();
	}
}

void ClapControlBase::nextPreset()
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "nextPreset");
	for (const auto& instance : m_instances)
	{
		instance->presetLoader().nextPreset();
	}
}

void ClapControlBase::prevPreset()
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "prevPreset");
	for (const auto& instance : m_instances)
	{
		instance->presetLoader().prevPreset();
	}
}

} // namespace lmms

#endif // LMMS_HAVE_CLAP
