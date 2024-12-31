/*
 * RemotePlugin.h - base class providing RPC like mechanisms
 *
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef LMMS_REMOTE_PLUGIN_H
#define LMMS_REMOTE_PLUGIN_H

#include "AudioData.h"
#include "RemotePluginBase.h"

#include "PluginPinConnector.h"
#include "SharedMemory.h"
#include "lmms_basics.h"

#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
	#include <QRecursiveMutex>
#endif

namespace lmms
{


class RemotePlugin;
class SampleFrame;

class ProcessWatcher : public QThread
{
	Q_OBJECT
public:
	explicit ProcessWatcher(RemotePlugin* plugin);
	~ProcessWatcher() override = default;

	void stop()
	{
		m_quit = true;
		quit();
	}

	void reset()
	{
		m_quit = false;
	}

private:
	void run() override;

	RemotePlugin * m_plugin;
	volatile bool m_quit;

} ;


class LMMS_EXPORT RemotePlugin : public QObject, public RemotePluginBase
{
	Q_OBJECT
public:
	explicit RemotePlugin(PluginPinConnector* pinConnector, Model* parent = nullptr);
	~RemotePlugin() override;

	inline bool isRunning()
	{
#ifdef DEBUG_REMOTE_PLUGIN
		return true;
#else
		return m_process.state() != QProcess::NotRunning;
#endif // DEBUG_REMOTE_PLUGIN
	}

	bool init( const QString &pluginExecutable, bool waitForInitDoneMsg, QStringList extraArgs = {} );

	inline void waitForHostInfoGotten()
	{
		m_failed = waitForMessage( IdHostInfoGotten ).id
							!= IdHostInfoGotten;
	}

	inline void waitForInitDone( bool _busyWaiting = true )
	{
		m_failed = waitForMessage( IdInitDone, _busyWaiting ).id != IdInitDone;
	}

	bool processMessage( const message & _m ) override;

	bool process();

	void updateBuffer(int channelsIn, int channelsOut);

	void processMidiEvent( const MidiEvent&, const f_cnt_t _offset );

	void updateSampleRate( sample_rate_t _sr )
	{
		lock();
		sendMessage( message( IdSampleRateInformation ).addInt( _sr ) );
		waitForMessage( IdInformationUpdated, true );
		unlock();
	}


	virtual void toggleUI()
	{
		lock();
		sendMessage( IdToggleUI );
		unlock();
	}

	int isUIVisible()
	{
		lock();
		sendMessage( IdIsUIVisible );
		unlock();
		message m = waitForMessage( IdIsUIVisible );
		return m.id != IdIsUIVisible ? -1 : m.getInt() ? 1 : 0;
	}

	inline bool failed() const
	{
		return m_failed;
	}

	inline void lock()
	{
		m_commMutex.lock();
	}

	inline void unlock()
	{
		m_commMutex.unlock();
	}

public slots:
	virtual void showUI();
	virtual void hideUI();

protected:
	bool m_failed;

	//! Signal to derived classes
	virtual void bufferUpdated() {}

	auto frames() const -> f_cnt_t { return m_frames; }
	auto channelsIn() const -> pi_ch_t { return m_channelsIn; }
	auto channelsOut() const -> pi_ch_t { return m_channelsOut; }

	auto inputBuffer() const -> Span<float> { return m_inputBuffer; }
	auto outputBuffer() const -> Span<float> { return m_outputBuffer; }

	PluginPinConnector* const m_pinConnector = nullptr;

private:
	QProcess m_process;
	ProcessWatcher m_watcher;

	QString m_exec;
	QStringList m_args;

#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
	QRecursiveMutex m_commMutex;
#else
	QMutex m_commMutex;
#endif

	SharedMemory<float[]> m_audioBuffer; // NOLINT
	std::size_t m_audioBufferSize = 0; // TODO: Move to `SharedMemory`?

	f_cnt_t m_frames = 0;
	pi_ch_t m_channelsIn = 0;
	pi_ch_t m_channelsOut = 0;

	Span<float> m_inputBuffer;
	Span<float> m_outputBuffer;

#ifndef SYNC_WITH_SHM_FIFO
	int m_server;
	QString m_socketFile;
#endif // not SYNC_WITH_SHM_FIFO

	friend class ProcessWatcher;


private slots:
	void processFinished( int exitCode, QProcess::ExitStatus exitStatus );
	void processErrored(QProcess::ProcessError err );
} ;

inline std::string QSTR_TO_STDSTR(QString const& qstr)
{
	return qstr.toStdString();
}

} // namespace lmms

#endif // LMMS_REMOTE_PLUGIN_H
