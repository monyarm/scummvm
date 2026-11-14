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

#ifndef SCOOBY_SOUND_ARCHIVE_H
#define SCOOBY_SOUND_ARCHIVE_H

#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/path.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace Audio {
class SeekableAudioStream;
}

namespace Scooby {

/**
 * One Gen 1 sound file: Sfx.dat, Voice.dat or Music.dat
 * (docs/formats/audio-archives.md). Clips are found by their
 * <Section>.<Clip> name.
 */
class SoundArchive {
public:
	struct Clip {
		Common::String name;
		uint32 offset;     // clip header in the file
		uint32 dataOffset; // PCM: the sector after the header and lip-sync bytes
		uint32 dataSize;
		uint32 rate;
		byte channels;
		byte bits;
		byte pan;
		byte volume;       // percent
		bool loop;
		byte lipSyncFps;
		uint16 lipSyncSize;
	};

	SoundArchive();

	/**
	 * Read the header, the index and every clip header. Returns false, after
	 * a warning, if the file is missing or malformed.
	 */
	bool open(const Common::Path &filename);
	bool isOpen() const { return !_filename.empty(); }
	const Common::Path &getFilename() const { return _filename; }

	/** Clips in name order. */
	const Common::Array<Clip> &getClips() const { return _clips; }
	const Clip *findClip(const Common::String &name) const;

	/** The clip's PCM bytes as a stream of their own, or nullptr. */
	Common::SeekableReadStream *createDataStream(const Clip &clip) const;

	/**
	 * The clip's lip-sync bytes, one per frame at lipSyncFps; empty if it has
	 * none. Returns false if the file can't be read.
	 */
	bool readLipSync(const Clip &clip, Common::Array<byte> &bytes) const;

	/** The clip as a raw PCM audio stream, or nullptr. */
	Audio::SeekableAudioStream *createAudioStream(const Clip &clip) const;

private:
	Common::Path _filename;
	Common::Array<Clip> _clips;
	Common::HashMap<Common::String, uint, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _byName;
};

} // End of namespace Scooby

#endif // SCOOBY_SOUND_ARCHIVE_H
