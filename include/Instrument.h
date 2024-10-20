/*
 * Instrument.h - declaration of class Instrument, which provides a
 *                standard interface for all instrument plugins
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef LMMS_INSTRUMENT_H
#define LMMS_INSTRUMENT_H

#include <QString>

#include "Flags.h"
#include "lmms_export.h"
#include "lmms_basics.h"
#include "Plugin.h"
#include "TimePos.h"

#include <cmath>


namespace lmms
{

// forward-declarations
class InstrumentTrack;
class MidiEvent;
class NotePlayHandle;
class Track;
class SampleFrame;


class LMMS_EXPORT Instrument : public Plugin
{
public:
	enum class Flag
	{
		NoFlags = 0x00,
		IsSingleStreamed = 0x01,	/*! Instrument provides a single audio stream for all notes */
		IsMidiBased = 0x02,			/*! Instrument is controlled by MIDI events rather than NotePlayHandles */
		IsNotBendable = 0x04,		/*! Instrument can't react to pitch bend changes */
	};

	using Flags = lmms::Flags<Flag>;

	Instrument(InstrumentTrack * _instrument_track,
			const Descriptor * _descriptor,
			const Descriptor::SubPluginFeatures::Key * key = nullptr,
			Flags flags = Flag::NoFlags);
	~Instrument() override = default;

	// if the plugin doesn't play each note, it can create an instrument-
	// play-handle and re-implement this method, so that it mixes its
	// output buffer only once per audio engine period
	void play(SampleFrame* workingBuffer)
	{
		playImpl(workingBuffer, nullptr);
	}

	void playNote(NotePlayHandle* notesToPlay, SampleFrame* workingBuffer)
	{
		playImpl(workingBuffer, notesToPlay);
	}

	// --------------------------------------------------------------------
	// functions that can/should be re-implemented:
	// --------------------------------------------------------------------

	virtual bool hasNoteInput() const { return true; }

	// This method can be overridden by instruments that need a certain
	// release time even if no envelope is active. It returns the time
	// in milliseconds that these instruments would like to have for
	// their release stage.
	virtual float desiredReleaseTimeMs() const
	{
		return 0.f;
	}

	// Converts the desired release time in milliseconds to the corresponding
	// number of frames depending on the sample rate.
	f_cnt_t desiredReleaseFrames() const
	{
		const sample_rate_t sampleRate = getSampleRate();

		return static_cast<f_cnt_t>(std::ceil(desiredReleaseTimeMs() * sampleRate / 1000.f));
	}

	sample_rate_t getSampleRate() const;

	bool isSingleStreamed() const
	{
		return m_flags.testFlag(Instrument::Flag::IsSingleStreamed);
	}

	//! Returns whether the instrument is MIDI-based or NotePlayHandle-based
	virtual bool isMidiBased() const = 0;

	bool isBendable() const
	{
		return !m_flags.testFlag(Instrument::Flag::IsNotBendable);
	}

	//! Returns nullptr if the effect does not have a pin connector
	virtual auto pinConnector() const -> const PluginPinConnector*
	{
		return nullptr;
	}

	QString fullDisplayName() const override;

	// --------------------------------------------------------------------
	// provided functions:
	// --------------------------------------------------------------------

	//! instantiate instrument-plugin with given name or return NULL
	//! on failure
	static Instrument * instantiate(const QString & _plugin_name,
		InstrumentTrack * _instrument_track,
		const Plugin::Descriptor::SubPluginFeatures::Key* key,
		bool keyFromDnd = false);

	virtual bool isFromTrack( const Track * _track ) const;

	inline InstrumentTrack * instrumentTrack() const
	{
		return m_instrumentTrack;
	}


protected:
	//! To be implemented by AudioProcessor
	virtual void playImpl(SampleFrame* workingBuffer, NotePlayHandle* notesToPlay) = 0;

	// fade in to prevent clicks
	void applyFadeIn(SampleFrame* buf, NotePlayHandle * n);

	// instruments may use this to apply a soft fade out at the end of
	// notes - method does this only if really less or equal
	// desiredReleaseFrames() frames are left
	void applyRelease( SampleFrame* buf, const NotePlayHandle * _n );

	float computeReleaseTimeMsByFrameCount(f_cnt_t frames) const;


private:
	InstrumentTrack * m_instrumentTrack;
	Flags m_flags;
};


LMMS_DECLARE_OPERATORS_FOR_FLAGS(Instrument::Flag)


} // namespace lmms

#endif // LMMS_INSTRUMENT_H
