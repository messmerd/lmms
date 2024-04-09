/*
 * ClapGuiView.h - CLAP plugin GUI
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

#ifndef LMMS_CLAP_GUI_VIEW_H
#define LMMS_CLAP_GUI_VIEW_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_CLAP

#include <QWidget>
#include "SubWindow.h"

namespace lmms
{

class ClapGui;

namespace gui
{

class SubWindow;

class ClapGuiView : public QWidget // public SubWindow
{
	Q_OBJECT
public:
	ClapGuiView(ClapGui* gui, QWidget* parent = nullptr);

	auto resize(std::uint32_t width, std::uint32_t height) -> bool;
	//auto open() -> bool;
	//auto close() -> bool;

	auto subWindow() const { return m_subWindow; }

public slots:
	void toggleGui();

private:
	ClapGui* m_gui = nullptr;
	SubWindow* m_subWindow = nullptr;
};

} // namespace gui

} // namespace lmms

#endif // LMMS_HAVE_CLAP

#endif // LMMS_CLAP_GUI_VIEW_H
