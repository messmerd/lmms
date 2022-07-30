/*
 * MidiExport.h - support for Exporting MIDI-files
 *
 * Copyright (c) 2015 Mohamed Abdel Maksoud <mohamed at amaksoud.com>
 * Copyright (c) 2017 Hyunjin Song <tteu.ingog/at/gmail.com>
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

#ifndef _MIDI_EXPORT_H
#define _MIDI_EXPORT_H

#include <QString>

#include "ExportFilter.h"
#include "MidiFile.hpp"

class QDomNode;

namespace lmms
{


const int BUFFER_SIZE = 50*1024;
using MTrack = MidiFile::MIDITrack<BUFFER_SIZE>;

struct MidiNote
{
	int time;
	uint8_t pitch;
	int duration;
	uint8_t volume;

	inline bool operator<(const MidiNote &b) const
	{
		return this->time < b.time;
	}
} ;

using MidiNoteVector = std::vector<MidiNote>;
using MidiNoteIterator = std::vector<MidiNote>::iterator;

class MidiExport: public ExportFilter
{
// 	Q_OBJECT
public:
	MidiExport();
	~MidiExport() override = default;

	gui::PluginView* instantiateView(QWidget *) override
	{
		return nullptr;
	}

	bool tryExport(const TrackContainer::TrackList &tracks,
				const TrackContainer::TrackList &patternTracks,
				int tempo, int masterPitch, const QString &filename) override;
	
private:
	void writeMidiClip(MidiNoteVector &midiClip, const QDomNode& n,
				int base_pitch, double base_volume, int base_time);
	void writeMidiClipToTrack(MTrack &mtrack, MidiNoteVector &nv);
	void writePatternClip(MidiNoteVector &src, MidiNoteVector &dst,
				int len, int base, int start, int end);
	void processPatternNotes(MidiNoteVector &nv, int cutPos);

	void error();


} ;


} // namespace lmms

#endif
