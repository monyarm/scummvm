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

#include "audio/audiostream.h"
#include "common/debug.h"
#include "common/md5.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "scooby/detection.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kFileNames[] = { "Sfx.dat", "Voice.dat", "Music.dat" };
static const Audio::Mixer::SoundType kSoundTypes[] = {
	Audio::Mixer::kSFXSoundType, Audio::Mixer::kSpeechSoundType, Audio::Mixer::kMusicSoundType
};

Sound::Sound(Audio::Mixer *mixer) : _mixer(mixer), _last(kSfx) {
	for (uint f = 0; f < kFileCount; f++)
		_channels[f].lipSyncFps = 0;
}

Sound::~Sound() {
	stop();
}

bool Sound::open() {
	for (uint f = 0; f < kFileCount; f++) {
		if (!_archives[f].open(kFileNames[f]))
			return false;
	}
	return true;
}

const SoundArchive::Clip *Sound::findClip(const Common::String &name, File *file) const {
	for (uint f = 0; f < kFileCount; f++) {
		const SoundArchive::Clip *clip = _archives[f].findClip(name);
		if (clip) {
			if (file)
				*file = (File)f;
			return clip;
		}
	}
	return nullptr;
}

bool Sound::play(const Common::String &name) {
	File f;
	const SoundArchive::Clip *clip = findClip(name, &f);
	if (!clip) {
		// The original logs "Failed to locate id %s" and plays nothing.
		debugC(1, kDebugAudio, "Failed to locate %s", name.c_str());
		return false;
	}
	Audio::SeekableAudioStream *stream = _archives[f].createAudioStream(*clip);
	if (!stream)
		return false;

	Channel &c = _channels[f];
	if (_mixer->isSoundHandleActive(c.handle))
		_mixer->stopHandle(c.handle);
	c.lipSyncFps = clip->lipSyncFps;
	if (!_archives[f].readLipSync(*clip, c.lipSync))
		c.lipSync.clear();

	// The volume byte is a percentage. Looping is the flag at +0x0C, an
	// inference that fits the 10 music tracks and 3 ambient effects that set
	// it. Pan is 0 on every clip, so the balance stays centred.
	Audio::AudioStream *playable = clip->loop ? Audio::makeLoopingAudioStream(stream, 0) : stream;
	const byte volume = (byte)MIN<uint>(clip->volume, 100) * Audio::Mixer::kMaxChannelVolume / 100;
	_mixer->playStream(kSoundTypes[f], &c.handle, playable, -1, volume);
	c.name = clip->name;
	_last = f;
	debugC(1, kDebugAudio, "play %s from %s: %u Hz, %u ch, %u bits, %u bytes, volume %u%%%s", clip->name.c_str(),
	       kFileNames[f], clip->rate, clip->channels, clip->bits, clip->dataSize, clip->volume, clip->loop ? ", looping" : "");
	return true;
}

void Sound::stop() {
	for (uint f = 0; f < kFileCount; f++) {
		if (_mixer && _mixer->isSoundHandleActive(_channels[f].handle))
			_mixer->stopHandle(_channels[f].handle);
		_channels[f].name.clear();
	}
}

bool Sound::isPlaying() const {
	return _mixer->isSoundHandleActive(_channels[_last].handle);
}

uint32 Sound::getElapsedMillis() const {
	return _mixer->getElapsedTime(_channels[_last].handle).msecs();
}

const Sound::Channel *Sound::findChannel(const Common::String &name) const {
	for (uint f = 0; f < kFileCount; f++) {
		const Channel &c = _channels[f];
		if (!c.name.empty() && c.name.equalsIgnoreCase(name) && _mixer->isSoundHandleActive(c.handle))
			return &c;
	}
	return nullptr;
}

bool Sound::isClipPlaying(const Common::String &name) const {
	return findChannel(name) != nullptr;
}

bool Sound::getLipShape(const Common::String &name, uint &shape, uint32 *index) const {
	// The original divides the play cursor by the bytes per second; the
	// mixer's elapsed time is the same position.
	const Channel *c = findChannel(name);
	if (!c || c->lipSync.empty())
		return false;
	const uint32 i = (uint32)((uint64)c->lipSyncFps * _mixer->getElapsedTime(c->handle).msecs() / 1000);
	if (i >= c->lipSync.size())
		return false;
	shape = c->lipSync[i] & 0x0F;
	if (index)
		*index = i;
	return true;
}

void Sound::checkAllClips() const {
	uint clips = 0, bad = 0;
	for (uint f = 0; f < kFileCount; f++) {
		const Common::Array<SoundArchive::Clip> &list = _archives[f].getClips();
		for (uint i = 0; i < list.size(); i++) {
			const SoundArchive::Clip &c = list[i];
			Common::SeekableReadStream *data = _archives[f].createDataStream(c);
			clips++;
			if (!data) {
				debugC(3, kDebugAudio, "clip %s BAD", c.name.c_str());
				bad++;
				continue;
			}
			debugC(3, kDebugAudio, "clip %s %s %u %u %u %u %u %u %s", kFileNames[f], c.name.c_str(), c.rate,
			       c.channels, c.bits, c.volume, c.loop, c.dataSize, Common::computeStreamMD5AsString(*data).c_str());
			delete data;
		}
	}
	debugC(3, kDebugAudio, "clip check: %u clips, %u bad", clips, bad);
}

} // End of namespace Scooby
