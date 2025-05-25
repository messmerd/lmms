/*
 * PreProcessor.h - Hook for pre-process step of AudioEngine
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

#ifndef LMMS_PREPROCESSOR_H
#define LMMS_PREPROCESSOR_H

#include <cstddef>
#include <list>

#include "lmms_export.h"

namespace lmms
{

class LMMS_EXPORT PreProcessor
{
public:
	constexpr static std::size_t MaxNumber = 512;

	PreProcessor();
	virtual ~PreProcessor();

	virtual void preprocess() {}

	void setPreProcessRemovalKey(std::list<PreProcessor*>::iterator key)
	{
		m_removalKey = key;
	}

	auto getPreProcessRemovalKey() const -> std::list<PreProcessor*>::iterator
	{
		return m_removalKey;
	}

private:
	std::list<PreProcessor*>::iterator m_removalKey{};

};

} // namespace lmms

#endif // LMMS_PREPROCESSOR_H
