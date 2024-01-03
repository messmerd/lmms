/*
 * ClapFxControlDialog.h - ClapFxControlDialog implementation
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

#ifndef LMMS_GUI_CLAP_FX_CONTROL_DIALOG_H
#define LMMS_GUI_CLAP_FX_CONTROL_DIALOG_H

#include "ClapViewBase.h"
#include "EffectControlDialog.h"

namespace lmms
{

class ClapFxControls;

namespace gui
{

class ClapFxControlDialog : public EffectControlDialog, public ClapViewBase
{
	Q_OBJECT

public:
	ClapFxControlDialog(ClapFxControls* controls);

private:
	auto clapControls() -> ClapFxControls*;
	void modelChanged() final;
};

} // namespace gui

} // namespace lmms

#endif // LMMS_GUI_CLAP_FX_CONTROL_DIALOG_H
