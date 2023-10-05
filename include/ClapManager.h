/*
 * ClapManager.h - Implementation of ClapManager class
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

#ifndef LMMS_CLAP_MANAGER_H
#define LMMS_CLAP_MANAGER_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_CLAP

#include "ClapFile.h"
#include "ClapInstance.h"

#include <filesystem>
#include <vector>
#include <string_view>
#include <memory>

#include <QString>
#include <clap/clap.h>

namespace lmms
{


//! Manages loaded .clap files, plugin info, and plugin instances
class ClapManager
{
public:
	ClapManager();
	~ClapManager();

	//! Allows access to loaded .clap files
	auto files() const -> const std::vector<ClapFile>& { return m_files; }

	/**
	 * Returns a cached plugin info vector
	 * ClapManager doesn't own the ClapPluginInfo objects, so pointers may be invalidated.
	*/
	auto pluginInfo() const -> const std::vector<std::weak_ptr<const ClapPluginInfo>>& { return m_pluginInfo; }

	/**
	 * Returns a cached URI-to-PluginInfo map
	 * ClapManager doesn't own the ClapPluginInfo objects, so pointers may be invalidated.
	*/
	auto uriToPluginInfo() const -> const std::unordered_map<std::string, std::weak_ptr<const ClapPluginInfo>>& { return m_uriToPluginInfo; }

	//! Return plugin info with URI @p uri or nullptr if none exists
	auto pluginInfo(const std::string& uri) -> std::weak_ptr<const ClapPluginInfo>;
	//! Return plugin info with URI @p uri or nullptr if none exists
	auto pluginInfo(const QString& uri) -> std::weak_ptr<const ClapPluginInfo>;

	//! Called by Engine at LMMS startup
	void initPlugins();

	//! Creates a plugin instance given plugin info; Plugin instance is owned by ClapManager
	//auto createInstance(const ClapPluginInfo* info) -> std::weak_ptr<ClapPluginInstance>;

	/**
	 * Transport update methods
	 * TODO: Add pre-roll and beat position
	 */
	static void updateTransport();
	static void setPlaying(bool isPlaying);
	static void setRecording(bool isRecording);
	static void setLooping(bool isLooping);
	static void setBeatPosition();
	static void setTimePosition(int elapsedMilliseconds);
	static void setTempo(bpm_t tempo);
	static void setTimeSignature(int num, int denom);

	static auto transport() -> const clap_event_transport* { return &s_transport; }

	static auto clapGuiApi() -> const char*;

	static auto debugging() -> bool { return s_debug; }

private:

	//! Finds all CLAP search paths and populates m_searchPaths
	void findSearchPaths();

	//! Returns search paths found by prior call to findSearchPaths()
	auto getSearchPaths() const -> const std::vector<std::filesystem::path>& { return m_searchPaths; }

	//! Finds and loads all .clap files in the provided search paths @p searchPaths
	void loadClapFiles(const std::vector<std::filesystem::path>& searchPaths);

	std::vector<std::filesystem::path> m_searchPaths; //!< Owns all CLAP search paths; Populated by findSearchPaths()
	std::vector<ClapFile> m_files; //!< Owns all loaded .clap files; Populated by loadClapFiles()

	// Non-owning plugin caches (for fast iteration/lookup)
	std::vector<std::weak_ptr<const ClapPluginInfo>> m_pluginInfo; //!< Non-owning vector of info for all successfully loaded CLAP plugins
	std::unordered_map<std::string, std::weak_ptr<const ClapPluginInfo>> m_uriToPluginInfo; //!< Non-owning map of plugin URIs (IDs) to ClapPluginInfo
	//std::vector<std::weak_ptr<ClapInstance>> m_instances; //!< Vector of all CLAP plugin instances (for guaranteeing correct clean-up order)

	static clap_event_transport s_transport;
	static bool s_debug; //!< If LMMS_CLAP_DEBUG is set, debug output will be printed
};



} // namespace lmms

#endif // LMMS_HAVE_CLAP

#endif // LMMS_CLAP_MANAGER_H
