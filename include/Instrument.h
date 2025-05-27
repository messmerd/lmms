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
#include <cmath>

#include "Flags.h"
#include "SampleFrame.h"
#include "lmms_export.h"
#include "LmmsTypes.h"
#include "Plugin.h"
#include "TimePos.h"

namespace lmms
{

// forward-declarations
class AudioPortsModel;
class InstrumentTrack;
class MidiEvent;
class NotePlayHandle;
class Track;


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

	Instrument(const Descriptor* _descriptor,
			InstrumentTrack* _instrument_track,
			const Descriptor::SubPluginFeatures::Key* key = nullptr,
			Flags flags = Flag::NoFlags);
	~Instrument() override = default;

	//! Receives all incoming MIDI events; Return true if event was handled
	auto handleMidiEvent(const MidiEvent& event, const TimePos& time = TimePos(), f_cnt_t offset = 0) -> bool
	{
		return handleMidiEventImpl(event, time, offset);
	}

	// --------------------------------------------------------------------
	// functions that can/should be re-implemented:
	// --------------------------------------------------------------------

	/**
	 * Needed for deleting plugin-specific-data of a note - plugin has to
	 * cast void-ptr so that the plugin-data is deleted properly
	 * (call of dtor if it's a class etc.)
	 */
	virtual void deleteNotePluginData(NotePlayHandle* nph) = 0;

	/**
	 * Get number of sample-frames that should be used when playing beat
	 * (note with unspecified length)
	 * Per default this function returns 0. In this case, channel is using
	 * the length of the longest envelope (if one active).
	 */
	virtual auto beatLen(NotePlayHandle* nph) const -> f_cnt_t { return 0; }

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

	virtual bool isSingleStreamed() const = 0;
	//{
	//	return m_flags.testFlag(Instrument::Flag::IsSingleStreamed);
	//}

	//! Returns whether the instrument is MIDI-based or NotePlayHandle-based
	virtual bool isMidiBased() const = 0;
	//{
	//	return m_flags.testFlag(Instrument::Flag::IsMidiBased);
	//}

	bool isBendable() const
	{
		return !m_flags.testFlag(Instrument::Flag::IsNotBendable);
	}

	//! Returns nullptr if the instrument does not have audio ports
	virtual auto audioPortsModel() const -> const AudioPortsModel*
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
	//! Receives all incoming MIDI events; Return true if event was handled
	virtual auto handleMidiEventImpl(const MidiEvent& event, const TimePos& time, f_cnt_t offset) -> bool
	{
		return true;
	}

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


class LMMS_EXPORT SingleStreamedInstrument : public Instrument
{
public:
	using Instrument::Instrument;

	void processCore(std::span<SampleFrame> coreInOut)
	{
		processCoreImpl(coreInOut);
	}

	void handleNote(NotePlayHandle* nph)
	{
		handleNoteImpl(nph);
	}

protected:
	//! Called after `handleNoteImpl` is called for all NPHs (?)
	virtual void processCoreImpl(std::span<SampleFrame> coreInOut) = 0;

	//! Called for each playing NPH (?). Does not process audio.
	virtual void handleNoteImpl(NotePlayHandle* nph) = 0;

	auto isSingleStreamed() const -> bool final { return true; }
	auto isMidiBased() const -> bool override { return false; }
};

class LMMS_EXPORT SingleStreamedMidiInstrument : public SingleStreamedInstrument
{
public:
	using SingleStreamedInstrument::SingleStreamedInstrument;

protected:
	//! Receives all incoming MIDI events; Return true if event was handled. TODO: Is this thread-safe?
	virtual auto handleMidiEventImpl(const MidiEvent& event, const TimePos& time, f_cnt_t offset) -> bool = 0;

	//! Called for each playing NPH (?). Does not process audio.
	void handleNoteImpl(NotePlayHandle* nph) override {}

	//! Single-streamed MIDI-based instruments probably don't need to use this
	void deleteNotePluginData(NotePlayHandle* nph) override {}

	auto isMidiBased() const -> bool final { return true; }
};

// TODO: This will be tricky...
class LMMS_EXPORT MultiStreamedInstrument : public Instrument
{
public:
	using Instrument::Instrument;

	//! Called for each playing NPH (?). Processes audio.
	void processCore(NotePlayHandle* nph, std::span<SampleFrame> coreInOut)
	{
		processCoreImpl(nph, coreInOut);
	}

protected:
	//! Called for each playing NPH (?).
	virtual void processCoreImpl(NotePlayHandle* nph, std::span<SampleFrame> coreInOut) = 0;

	// TODO: Use these so that plugin implementations don't need to?
	// 	const fpp_t frames = nph->framesLeftForCurrentPeriod();
	// const f_cnt_t offset = nph->noteOffset();

	auto isSingleStreamed() const -> bool final { return false; }
	auto isMidiBased() const -> bool final { return false; } // TODO: Even though none currently are, could multi-streamed instruments be midi-based?
};




} // namespace lmms

#endif // LMMS_INSTRUMENT_H
