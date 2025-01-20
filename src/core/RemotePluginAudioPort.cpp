/*
 * RemotePluginAudioPort.cpp - PluginAudioPort implementation for RemotePlugin
 *
 * Copyright (c) 2025 Dalton Messmer <messmer.dalton/at/gmail.com>
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

#include "RemotePluginAudioPort.h"

#include "AudioEngine.h"
#include "Engine.h"
#include "RemotePlugin.h"
#include "lmms_basics.h"

namespace lmms {

RemotePluginAudioPortController::RemotePluginAudioPortController(PluginPinConnector& pinConnector)
	: m_pinConnector{&pinConnector}
	, m_frames{Engine::audioEngine()->framesPerPeriod()}
{
}

void RemotePluginAudioPortController::remotePluginUpdateBuffers(int channelsIn, int channelsOut, fpp_t frames)
{
	assert(m_remotePlugin != nullptr);
	m_remotePlugin->updateBuffer(channelsIn, channelsOut, frames);
}

auto RemotePluginAudioPortController::remotePluginInputBuffer() const -> float*
{
	assert(m_remotePlugin != nullptr);
	return m_remotePlugin->inputBuffer().data();
}

auto RemotePluginAudioPortController::remotePluginOutputBuffer() const -> float*
{
	assert(m_remotePlugin != nullptr);
	return m_remotePlugin->outputBuffer().data();
}

} // namespace lmms
