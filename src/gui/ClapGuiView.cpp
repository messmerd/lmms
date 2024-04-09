/*
 * ClapGuiView.cpp - CLAP plugin GUI
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

#include "ClapGuiView.h"

#ifdef LMMS_HAVE_CLAP

#include "ClapGui.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "SubWindow.h"

namespace lmms::gui
{

namespace
{

auto wantsLogicalSize() noexcept -> bool
{
#ifdef LMMS_BUILD_APPLE
	return true;
#else
	return false;
#endif
}

} // namespace


ClapGuiView::ClapGuiView(ClapGui* gui, QWidget* parent)
	: QWidget{parent}
	, m_gui{gui}
{
	gui->setView(this);
	m_subWindow = getGUI()->mainWindow()->addWindowedWidget(this, // TODO: nullptr?
		Qt::SubWindow | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint);

	gui->create();
}

void ClapGuiView::toggleGui()
{
	bool isVisible = !this->isVisible();
	this->setVisible(isVisible);
	if (isVisible) { m_subWindow->show(); } else { m_subWindow->hide(); }
	m_gui->setVisibility(isVisible);
}

auto ClapGuiView::resize(std::uint32_t width, std::uint32_t height) -> bool
{
	const auto ratio = wantsLogicalSize() ? 1 : devicePixelRatio();
	auto sw = width / ratio;
	auto sh = height / ratio;
	setFixedSize(sw, sh);
	show();
	adjustSize();
	return true;
}

/*
auto ClapGuiView::open() -> bool
{
	show();
	return true;
}

auto ClapGuiView::close() -> bool
{
	hide();
	return true;
}
*/

} // namespace lmms::gui

#endif // LMMS_HAVE_CLAP
