/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCOOBY_SOUND_H
#define SCOOBY_SOUND_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/str.h"

#include "scooby/sound_archive.h"

namespace Scooby {

/**
 * The Gen 1 sound files and playback through the mixer. Names are looked up
 * in Sfx.dat, then Voice.dat, then Music.dat, as the original does
 * (0x434DC0).
 */
class Sound {
public:
	enum File {
		kSfx = 0,
		kVoice = 1,
		kMusic = 2,
		kFileCount = 3
	};

	explicit Sound(Audio::Mixer *mixer);
	~Sound();

	/** Open all three files. Returns false, after a warning, if any fails. */
	bool open();

	const SoundArchive &getArchive(File f) const { return _archives[f]; }

	/** Find a clip by name in the original's search order. */
	const SoundArchive::Clip *findClip(const Common::String &name, File *file = nullptr) const;

	/**
	 * Play a clip on the mixer channel for its file (SFX, speech or music),
	 * at its own volume, looping if its loop flag is set. Each file has one
	 * handle, so a clip stops the one its file played before, and a voice
	 * line doesn't stop the music. Returns false if the clip isn't found.
	 */
	bool play(const Common::String &name);
	/** Stop every file's clip. */
	void stop();
	/** Stop @p name if it is playing (0x434CB0, by id). */
	void stopClip(const Common::String &name);

	// The clip played last, from any file.
	bool isPlaying() const;
	const Common::String &getPlayingName() const { return _channels[_last].name; }
	/** Milliseconds the mixer has played of the clip played last. */
	uint32 getElapsedMillis() const;

	/** Whether @p name is playing (case-insensitive). */
	bool isClipPlaying(const Common::String &name) const;
	/**
	 * The mouth shape of @p name now (0x442DD0): the low nibble of its
	 * lip-sync byte at trunc(fps x seconds played). False if the clip isn't
	 * playing, has no lip sync, or has played past its last byte.
	 */
	bool getLipShape(const Common::String &name, uint &shape, uint32 *index = nullptr) const;

	/** Read every clip's PCM and log its MD5 on the audio channel. */
	void checkAllClips() const;

private:
	Audio::Mixer *_mixer;
	SoundArchive _archives[kFileCount];
	struct Channel {
		Audio::SoundHandle handle;
		Common::String name;
		byte lipSyncFps;
		Common::Array<byte> lipSync;
	};

	const Channel *findChannel(const Common::String &name) const;

	Channel _channels[kFileCount];
	File _last;
};

} // End of namespace Scooby

#endif // SCOOBY_SOUND_H
