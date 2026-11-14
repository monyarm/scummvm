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

#include "audio/decoders/raw.h"
#include "common/algorithm.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/substream.h"
#include "common/textconsole.h"

#include "scooby/detection.h"
#include "scooby/sound_archive.h"

namespace Scooby {

// File: u32 version (1), u32 clip count, u32 index offset; clips from 0x800,
// each on a 2048-byte sector; the index (68-byte records: u32 clip offset,
// char name[64]) ends at the end of the file (0x43C5A0).
// Clip: u32 data size, u32 rate, u8 channels, u8 bits, u8 pan, u8 volume,
// u8 loop, u8 lip-sync fps, u16 lip-sync size, the lip-sync bytes, then the
// PCM from the next sector (0x446010).
static const uint32 kSector = 2048;
static const uint32 kHeaderSize = 12;
static const uint32 kRecordSize = 68;
static const uint32 kNameSize = 64;
static const uint32 kClipHeaderSize = 16;

struct ClipOffsetLess {
	bool operator()(const SoundArchive::Clip &a, const SoundArchive::Clip &b) const {
		return a.offset < b.offset;
	}
};

struct ClipNameLess {
	bool operator()(const SoundArchive::Clip &a, const SoundArchive::Clip &b) const {
		return a.name < b.name;
	}
};

SoundArchive::SoundArchive() {
}

bool SoundArchive::open(const Common::Path &filename) {
	_filename.clear();
	_clips.clear();
	_byName.clear();

	Common::File file;
	if (!file.open(filename)) {
		warning("%s: cannot open", filename.toString().c_str());
		return false;
	}
	const uint32 fileSize = file.size();
	const uint32 version = file.readUint32LE();
	const uint32 count = file.readUint32LE();
	const uint32 index = file.readUint32LE();
	if (file.err() || version != 1 || index < kHeaderSize || index > fileSize ||
	    count != (fileSize - index) / kRecordSize || (fileSize - index) % kRecordSize != 0) {
		warning("%s: bad header (version %u, %u clips, index at 0x%x)", filename.toString().c_str(), version, count, index);
		return false;
	}

	file.seek(index);
	char name[kNameSize];
	for (uint32 i = 0; i < count; i++) {
		Clip c;
		c.offset = file.readUint32LE();
		file.read(name, kNameSize);
		const char *nul = (const char *)memchr(name, 0, kNameSize);
		if (file.err() || !nul || nul == name) {
			warning("%s: index record %u is bad", filename.toString().c_str(), i);
			_clips.clear();
			return false;
		}
		c.name = Common::String(name, nul - name);
		_clips.push_back(c);
	}

	// Each clip runs up to the next one, or to the index for the last.
	Common::sort(_clips.begin(), _clips.end(), ClipOffsetLess());
	for (uint i = 0; i < _clips.size(); i++) {
		Clip &c = _clips[i];
		const uint32 end = i + 1 < _clips.size() ? _clips[i + 1].offset : index;
		if (c.offset % kSector != 0 || c.offset > end || end - c.offset < kClipHeaderSize) {
			warning("%s: clip %s is outside its slot", filename.toString().c_str(), c.name.c_str());
			_clips.clear();
			return false;
		}
		file.seek(c.offset);
		c.dataSize = file.readUint32LE();
		c.rate = file.readUint32LE();
		c.channels = file.readByte();
		c.bits = file.readByte();
		c.pan = file.readByte();
		c.volume = file.readByte();
		c.loop = file.readByte() != 0;
		c.lipSyncFps = file.readByte();
		c.lipSyncSize = file.readUint16LE();
		const uint32 head = (kClipHeaderSize + c.lipSyncSize + kSector - 1) / kSector * kSector;
		c.dataOffset = c.offset + head;
		const uint32 frame = c.channels * (c.bits / 8);
		if (file.err() || (c.channels != 1 && c.channels != 2) || (c.bits != 8 && c.bits != 16) || c.rate == 0 ||
		    c.dataSize % frame != 0 || head > end - c.offset || c.dataSize > end - c.dataOffset) {
			warning("%s: clip %s has a bad header", filename.toString().c_str(), c.name.c_str());
			_clips.clear();
			return false;
		}
	}

	Common::sort(_clips.begin(), _clips.end(), ClipNameLess());
	for (uint i = 0; i < _clips.size(); i++) {
		if (_byName.contains(_clips[i].name))
			warning("%s: duplicate clip %s; keeping the first", filename.toString().c_str(), _clips[i].name.c_str());
		else
			_byName[_clips[i].name] = i;
	}

	_filename = filename;
	debugC(1, kDebugAudio, "%s: %u clips", filename.toString().c_str(), _clips.size());
	return true;
}

const SoundArchive::Clip *SoundArchive::findClip(const Common::String &name) const {
	uint i;
	if (!_byName.tryGetVal(name, i))
		return nullptr;
	return &_clips[i];
}

Common::SeekableReadStream *SoundArchive::createDataStream(const Clip &clip) const {
	Common::File *file = new Common::File();
	if (!file->open(_filename)) {
		delete file;
		return nullptr;
	}
	return new Common::SeekableSubReadStream(file, clip.dataOffset, clip.dataOffset + clip.dataSize, DisposeAfterUse::YES);
}

bool SoundArchive::readLipSync(const Clip &clip, Common::Array<byte> &bytes) const {
	// The open (0x446010) keeps (count, fps, bytes) from just after the
	// header (0x442FB0). open() checked that they fit before the PCM.
	bytes.clear();
	if (!clip.lipSyncSize)
		return true;
	Common::File file;
	if (!file.open(_filename) || !file.seek(clip.offset + kClipHeaderSize))
		return false;
	bytes.resize(clip.lipSyncSize);
	if (file.read(bytes.data(), bytes.size()) != bytes.size()) {
		bytes.clear();
		return false;
	}
	return true;
}

Audio::SeekableAudioStream *SoundArchive::createAudioStream(const Clip &clip) const {
	Common::SeekableReadStream *data = createDataStream(clip);
	if (!data)
		return nullptr;

	// The original hands the bytes to DirectSound as PCM: 16-bit samples are
	// signed little-endian, 8-bit ones unsigned.
	byte flags = clip.bits == 16 ? (Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN) : Audio::FLAG_UNSIGNED;
	if (clip.channels == 2)
		flags |= Audio::FLAG_STEREO;
	return Audio::makeRawStream(data, clip.rate, flags, DisposeAfterUse::YES);
}

} // End of namespace Scooby
