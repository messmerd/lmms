/*
 * ClapGui.cpp - Implements CLAP gui extension
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

#include "ClapGui.h"

#ifdef LMMS_HAVE_CLAP

#include <cassert>

#include "ClapGuiView.h"
#include "ClapInstance.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "SubWindow.h"

namespace lmms
{

ClapGui::ClapGui(ClapInstance* instance)
	: QObject{instance}
	, ClapExtension{instance}
{
	if (!gui::getGUI()) { return; }
	const auto windowId = gui::getGUI()->mainWindow()->winId();

#if defined(LMMS_BUILD_WIN32)
	m_embedMethod = WindowEmbed::Method::Win32;
	m_window.api = CLAP_WINDOW_API_WIN32;
	m_window.win32 = reinterpret_cast<clap_hwnd>(windowId);
#elif defined(LMMS_BUILD_APPLE)
	m_embedMethod = WindowEmbed::Method::Cocoa;
	m_window.api = CLAP_WINDOW_API_COCOA;
	m_window.cocoa = reinterpret_cast<clap_nsview>(windowId);
#elif defined(LMMS_BUILD_LINUX)
	m_embedMethod = WindowEmbed::Method::XEmbed;
	m_window.api = CLAP_WINDOW_API_X11;
	m_window.x11 = windowId;
#else
	instance->log(CLAP_LOG_ERROR, "The host does not implement the CLAP gui extension for this platform");
	m_window.api = nullptr;
#endif
}

auto ClapGui::initImpl() noexcept -> bool
{
	if (!m_window.api) { return false; } // unsupported host platform

	m_supportsEmbed = windowSupported(*pluginExt(), false)
		&& pluginExt()->is_api_supported(plugin(), m_window.api, false);

	m_supportsFloating = windowSupported(*pluginExt(), true)
		&& pluginExt()->is_api_supported(plugin(), m_window.api, true);

	if (!m_supportsEmbed)
	{
		if (!m_supportsFloating)
		{
			logger().log(CLAP_LOG_ERROR, "Plugin does not support any GUI API that the host implements");
			return false;
		}

		// No choice but to use floating windows
		m_embedMethod = WindowEmbed::Method::Floating;
	}

	return true;
}

void ClapGui::deinitImpl() noexcept
{
	destroy();
}

auto ClapGui::hostExtImpl() const -> const clap_host_gui*
{
	static clap_host_gui ext {
		&clapResizeHintsChanged,
		&clapRequestResize,
		&clapRequestShow,
		&clapRequestHide,
		&clapRequestClosed
	};
	return gui::getGUI() ? &ext : nullptr;
}

auto ClapGui::checkSupported(const clap_plugin_gui& ext) -> bool
{
	return ext.is_api_supported && ext.get_preferred_api && ext.create && ext.destroy
		&& ext.set_scale && ext.get_size && ext.show && ext.hide
		&& (windowSupported(ext, true) || windowSupported(ext, false));
}

auto ClapGui::windowSupported(const clap_plugin_gui& ext, bool floating) -> bool
{
	// NOTE: This method only checks if the needed API functions are implemented.
	//       Still need to call clap_plugin_gui.is_api_supported()

	if (floating)
	{
		// Needed for floating windows
		return ext.set_transient && ext.suggest_title;
	}
	else
	{
		// Needed for embedded windows
		return ext.can_resize && ext.get_resize_hints && ext.adjust_size && ext.set_size && ext.set_parent;
	}
}

auto ClapGui::create() -> bool
{
#if QT_VERSION < 0x50C00
	// Workaround for a bug in Qt versions below 5.12,
	// where argument-dependent-lookup fails for QFlags operators
	// declared inside a namepsace.
	// This affects the Q_DECLARE_OPERATORS_FOR_FLAGS macro in Instrument.h
	// See also: https://codereview.qt-project.org/c/qt/qtbase/+/225348

	using ::operator|;

#endif

	assert(supported());
	destroy();

	if (!gui::getGUI() || !m_pluginView) { return false; }

	const auto windowId = m_pluginView->winId();
#if defined(LMMS_BUILD_WIN32)
	m_window.win32 = reinterpret_cast<clap_hwnd>(windowId);
#elif defined(LMMS_BUILD_APPLE)
	m_window.cocoa = reinterpret_cast<clap_nsview>(windowId);
#elif defined(LMMS_BUILD_LINUX)
	m_window.x11 = windowId;
#endif

	logger().log(CLAP_LOG_DEBUG, isFloating() ? "Creating floating gui" : "Creating embedded gui");
	if (!pluginExt()->create(plugin(), m_window.api, isFloating()))
	{
		logger().log(CLAP_LOG_ERROR, "Failed to create the plugin GUI");
		return false;
	}

	m_created = true;
	assert(m_visible == false);

	if (isFloating())
	{
		logger().log(CLAP_LOG_DEBUG, "Setting transient then suggest title");
		pluginExt()->set_transient(plugin(), &m_window);
		if (const auto name = instance()->info().descriptor().name; name && name[0] != '\0')
		{
			pluginExt()->suggest_title(plugin(), name);
		}
	}
	else
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;

		if (!pluginExt()->get_size(plugin(), &width, &height))
		{
			logger().log(CLAP_LOG_PLUGIN_MISBEHAVING, "Could not get the size of the plugin gui");
			m_created = false;
			pluginExt()->destroy(plugin());
			return false;
		}

		logger().log(CLAP_LOG_DEBUG, "Got embedded size; width:" + std::to_string(width) + " height:" + std::to_string(height));

		//mainWindow()->resizePluginView(width, height);
		requestResize(width, height);

		if (!pluginExt()->set_parent(plugin(), &m_window))
		{
			logger().log(CLAP_LOG_ERROR, "Failed to embed the plugin GUI");
			m_created = false;
			pluginExt()->destroy(plugin());
			return false;
		}

		logger().log(CLAP_LOG_DEBUG, "Set parent");
	}

	setVisibility(true);

	return true;
}

void ClapGui::destroy()
{
	if (supported() && m_created)
	{
		pluginExt()->destroy(plugin());
	}

	m_created = false;
	m_visible = false;
}

void ClapGui::setVisibility(bool isVisible)
{
	assert(ClapThreadCheck::isMainThread());
	if (!m_created) { return; }

	if (isVisible && !m_visible)
	{
		pluginExt()->show(plugin());
		m_visible = true;
	}
	else if (!isVisible && m_visible)
	{
		pluginExt()->hide(plugin());
		m_visible = false;
	}
}

/*
auto ClapGui::createView() -> gui::ClapGuiView*
{
	if (!gui::getGUI()) { return nullptr; }
	if (m_pluginView) { return m_pluginView; }
	m_pluginView = new gui::ClapGuiView{this};
	return m_pluginView;
}*/

auto ClapGui::requestResize(std::uint32_t width, std::uint32_t height) -> bool
{
	assert(ClapThreadCheck::isMainThread());
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::requestResize()");
	return m_pluginView->resize(width, height);
}

auto ClapGui::requestShow() -> bool
{
	assert(ClapThreadCheck::isMainThread());
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapRequestShow()");
	m_pluginView->show();
	//m_pluginView->subWindow()->show();
	return true;
}

auto ClapGui::requestHide() -> bool
{
	assert(ClapThreadCheck::isMainThread());
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapRequestHide()");
	m_pluginView->hide();
	//m_pluginView->subWindow()->hide();
	return true;
}

void ClapGui::clapResizeHintsChanged(const clap_host* host)
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapResizeHintsChanged() [NOT IMPLEMENTED YET]");
	// TODO
}

auto ClapGui::clapRequestResize(const clap_host* host, std::uint32_t width, std::uint32_t height) -> bool
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapRequestResize()");

	auto h = fromHost(host);
	if (!h) { return false; }
	auto& gui = h->gui();

	bool success = false;
	if (!QMetaObject::invokeMethod(&gui, "requestResize", Qt::QueuedConnection,
		Q_RETURN_ARG(bool, success),
		Q_ARG(std::uint32_t, width),
		Q_ARG(std::uint32_t, height)))
	{
		assert(false);
		return false;
	}

	return success;
}

auto ClapGui::clapRequestShow(const clap_host* host) -> bool
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapRequestShow()");

	auto h = fromHost(host);
	if (!h) { return false; }
	auto& gui = h->gui();

	bool success = false;
	if (!QMetaObject::invokeMethod(&gui, "requestShow", Qt::QueuedConnection,
		Q_RETURN_ARG(bool, success)))
	{
		assert(false);
		return false;
	}

	return success;
}

auto ClapGui::clapRequestHide(const clap_host* host) -> bool
{
	ClapLog::globalLog(CLAP_LOG_ERROR, "ClapGui::clapRequestHide()");

	auto h = fromHost(host);
	if (!h) { return false; }
	auto& gui = h->gui();

	bool success = false;
	if (!QMetaObject::invokeMethod(&gui, "requestHide", Qt::QueuedConnection,
		Q_RETURN_ARG(bool, success)))
	{
		assert(false);
		return false;
	}

	return success;
}

void ClapGui::clapRequestClosed(const clap_host* host, bool wasDestroyed)
{
	if (!wasDestroyed) { return; }

	auto h = fromHost(host);
	if (!h) { return; }
	auto& gui = h->gui();

	// TODO: Call this on main thread?
	gui.pluginExt()->destroy(gui.plugin());
	gui.m_created = false;
	gui.m_visible = false;
}

} // namespace lmms

#endif // LMMS_HAVE_CLAP
