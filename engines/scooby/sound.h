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
	 * at its own volume, looping if its loop flag is set. Speech and music
	 * have one handle each, so a clip stops the one its file played before,
	 * and a voice line doesn't stop the music. Sfx.dat's clips each have
	 * their own (0x43CC20): another effect plays on, and the same one starts
	 * again. Returns false if the clip isn't found.
	 * A @p background clip (the room's music) doesn't become the clip
	 * played last.
	 */
	bool play(const Common::String &name, bool background = false);
	/** Stop every file's clip. */
	void stop();
	/** Stop the SFX and speech files' clips, not the music (0x434E10). */
	void stopEffects();
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
	bool getLipShape(const Common::String &name, uint &shape, uint32 *index = nullptr, uint *high = nullptr) const;

	/** Read every clip's PCM and log its MD5 on the audio channel. */
	void checkAllClips() const;

	/**
	 * The music's fade (the sound manager's +0x10-+0x1C, 0x475F48): one
	 * percent of the music file's volume (its +0x18, 100 at start) every
	 * @p interval game ticks, down (0x434F10) or up (0x434F30) to @p target.
	 */
	void fadeMusic(uint target, uint interval, bool up);
	/** 0x434F50: no fade, the music at 100%. */
	void cancelMusicFade();
	/** 0x434C00, once a game tick: the fade's next step. */
	void tickMusicFade();
	uint getMusicPercent() const { return _musicPercent; }
	/** 0x434B60 -> 0x43C940 on the music: its volume, as a percent of the clip's. */
	void setMusicPercent(uint percent) {
		_musicPercent = percent;
		applyMusicPercent();
	}
	/**
	 * 0x434B60 -> 0x43C940: the volume of the file that holds @p name (its
	 * +0x18), as a percent of each clip's own, for @p name now if it is
	 * playing and for every clip the file plays after. The music's is the
	 * fade's percent.
	 */
	void setFilePercent(const Common::String &name, uint percent);
	uint getFilePercent(File f) const { return f == kMusic ? _musicPercent : _filePercent[f]; }
	/** 0x434BA0: the seconds @p name has played, in milliseconds, or -1 if it isn't playing. */
	int32 getClipMillis(const Common::String &name) const;
	bool isMusicFading() const { return _fadeDown || _fadeUp; }
	/** The fade as "up 100/4", "down 90/2" or "-", for the checks. */
	Common::String describeMusicFade() const;
	/** The music channel's mixer volume now, and its clip's own. */
	byte getMusicVolume() const;
	byte getMusicClipVolume() const { return _channels[kMusic].volume; }

private:
	Audio::Mixer *_mixer;
	SoundArchive _archives[kFileCount];
	struct Channel {
		Audio::SoundHandle handle;
		Common::String name;
		byte volume = 0;          // the clip's own, for the mixer
		byte lipSyncFps;
		Common::Array<byte> lipSync;
	};

	const Channel *findChannel(const Common::String &name) const;

	Channel _channels[kFileCount];
	File _last;
	/** Sfx.dat clips still playing that a later Sfx.dat clip left alone (0x43CC20). */
	Common::Array<Channel> _sfxEarlier;
	static const uint kSfxEarlier = 16;
	void stopEarlierEffects();

	/** The music's channel volume: its clip's, at the music's percent (0x43C940). */
	void applyMusicPercent();
	bool _fadeDown = false, _fadeUp = false; // +0x10, +0x11
	uint _fadeTarget = 0;                    // +0x14
	uint _fadeInterval = 0;                  // +0x18
	int _fadeCountdown = 0;                  // +0x1C
	uint _musicPercent = 100;                // the music file's +0x18
	uint _filePercent[kFileCount] = { 100, 100, 100 }; // Sfx.dat's and Voice.dat's +0x18
};

} // End of namespace Scooby

#endif // SCOOBY_SOUND_H
