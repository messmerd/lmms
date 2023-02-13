/*
 * ClapControlBase.h - CLAP control base class
 *
 * Copyright (c) 2023 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#ifndef LMMS_CLAP_CONTROL_BASE_H
#define LMMS_CLAP_CONTROL_BASE_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_CLAP

#include "ClapFile.h"
#include "ClapInstance.h"
#include "DataFile.h"
#include "LinkedModelGroups.h"
#include "lmms_export.h"
#include "Plugin.h"

#include <memory>
#include <clap/clap.h>

namespace lmms
{

class ClapInstance;
class ClapProc;
class PluginIssue;

/**
	Common base class for Lv2 plugins

	This class contains a vector of Lv2Proc, usually 1 (for stereo plugins) or
	2 (for mono plugins). Most of the logic is done there, this class primarily
	forwards work to the Lv2Proc and collects the results.

	This class provides everything Lv2 plugins have in common. It's not
	named Lv2Plugin, because
	* it does not inherit Instrument
	* the Plugin subclass Effect does not inherit this class

	This class would usually be a Model subclass. However, Qt doesn't allow
	this:
	* inheriting only from Model will cause diamond inheritance for QObject,
	  which will cause errors with Q_OBJECT
	* making this a direct subclass of Instrument resp. EffectControls would
	  require CRTP, which would make this class a template class, which would
	  conflict with Q_OBJECT

	The consequence is that this class can neither inherit QObject or Model, nor
	Instrument or EffectControls, which means in fact:
	* this class contains no signals or slots, but it offers stubs for slots
	  that shall be called by child classes
	* this class can not override virtuals of Instrument or EffectControls, so
	  it will offer functions that must be called by virtuals in its child class
*/
class LMMS_EXPORT ClapControlBase : public LinkedModelGroups
{
public:
	const ClapPluginInfo* getPluginInfo() const { return m_info; }

	ClapInstance* control(std::size_t idx) { return m_instances[idx].get(); }
	const ClapInstance* control(std::size_t idx) const { return m_instances[idx].get(); }

	bool hasGui() const { return m_hasGUI; }
	void setHasGui(bool val) { m_hasGUI = val; }

protected:

	//! @param that the class inheriting this class and inheriting Model;
	//!   this is the same pointer as this, but a different type
	//! @param uri the CLAP URI telling this class what plugin to construct
	ClapControlBase(class Model* that, const QString& uri);
	ClapControlBase(const ClapControlBase&) = delete;
	~ClapControlBase() override;

	ClapControlBase& operator=(const ClapControlBase&) = delete;

	//! Must be checked after ctor or reload
	auto isValid() const -> bool { return m_valid; }

	/*
		overrides
	*/
	auto getGroup(std::size_t idx) -> LinkedModelGroup* override;
	auto getGroup(std::size_t idx) const -> const LinkedModelGroup* override;

	/*
		utils for the run thread
	*/
	//! Copy values from the LMMS core (connected models, MIDI events, ...) into
	//! the respective ports
	void copyModelsFromLmms();
	//! Bring values from all ports to the LMMS core
	void copyModelsToLmms() const;

	//! Copy buffer passed by LMMS into our ports
	void copyBuffersFromLmms(const sampleFrame* buf, fpp_t frames);
	//! Copy our ports into buffers passed by LMMS
	void copyBuffersToLmms(sampleFrame* buf, fpp_t frames) const;
	//! Run the CLAP plugin instance(s) for @param frames frames
	void run(fpp_t frames);

	/*
		load/save, must be called from virtuals
	*/
	void saveSettings(QDomDocument& doc, QDomElement& that);
	void loadSettings(const QDomElement& that);
	void loadFile(const QString& file);
	//! TODO: not implemented
	void reloadPlugin();

	/*
		more functions that must be called from virtuals
	*/
	auto controlCount() const -> std::size_t;
	auto nodeName() const -> QString { return "clapcontrols"; }
	auto hasNoteInput() const -> bool;
	void handleMidiInputEvent(const class MidiEvent& event,
		const class TimePos& time, f_cnt_t offset);

private:
	//! Return the DataFile settings type
	virtual auto settingsType() -> DataFile::Types = 0;
	//! Inform the plugin about a file name change
	virtual void setNameFromFile(const QString& fname) = 0;

	//! Independent processors
	//! If this is a mono effect, the vector will have size 2 in order to
	//! fulfill LMMS' requirement of having stereo input and output
	std::vector<std::unique_ptr<ClapInstance>> m_instances;

	bool m_valid = true;
	bool m_hasGUI = false;
	unsigned m_channelsPerInstance;

	const ClapPluginInfo* m_info;
	//const ClapPluginInstance* m_plugin;
	//std::unique_ptr<ClapInstance> m_instance;
};


} // namespace lmms

#endif // LMMS_HAVE_CLAP

#endif // LMMS_CLAP_CONTROL_BASE_H
