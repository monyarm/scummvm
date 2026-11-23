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

#include "terraglyph/detection.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

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

bool Sound::hasClipPrefix(const Common::String &prefix) const {
	Common::String lower = prefix;
	lower.toLowercase();
	for (uint f = 0; f < kFileCount; f++) {
		const Common::Array<SoundArchive::Clip> &clips = _archives[f].getClips();
		for (uint i = 0; i < clips.size(); i++) {
			Common::String name = clips[i].name;
			name.toLowercase();
			if (name.hasPrefix(lower))
				return true;
		}
	}
	return false;
}

bool Sound::play(const Common::String &name, bool background) {
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
	if (f == kSfx) {
		// 0x43CC20 gives each Sfx.dat clip a buffer of its own: another
		// clip goes on playing, and the same clip starts again.
		for (uint i = 0; i < _sfxEarlier.size();) {
			if (!_mixer->isSoundHandleActive(_sfxEarlier[i].handle)) {
				_sfxEarlier.remove_at(i);
			} else if (_sfxEarlier[i].name.equalsIgnoreCase(clip->name)) {
				_mixer->stopHandle(_sfxEarlier[i].handle);
				_sfxEarlier.remove_at(i);
			} else {
				i++;
			}
		}
		if (_mixer->isSoundHandleActive(c.handle) && !c.name.equalsIgnoreCase(clip->name)) {
			// The mixer has 32 slots, DirectSound no such limit: past
			// kSfxEarlier effects at once, the oldest stops.
			if (_sfxEarlier.size() >= kSfxEarlier) {
				_mixer->stopHandle(_sfxEarlier[0].handle);
				_sfxEarlier.remove_at(0);
			}
			_sfxEarlier.push_back(c);
			c.handle = Audio::SoundHandle();
		}
	} else if (f == kVoice) {
		// 0x43CC20 makes each Voice.dat play a stream of its own (0x4428A0)
		// and frees no other clip's, so earlier lines play on to their ends.
		// A line played again gets a new stream too: its name then finds the
		// new one, and the old one plays on out of reach.
		for (uint i = 0; i < _voiceEarlier.size();) {
			if (!_mixer->isSoundHandleActive(_voiceEarlier[i].handle)) {
				_voiceEarlier.remove_at(i);
				continue;
			}
			if (_voiceEarlier[i].name.equalsIgnoreCase(clip->name))
				_voiceEarlier[i].named = false;
			i++;
		}
		if (_mixer->isSoundHandleActive(c.handle)) {
			_voiceEarlier.push_back(c);
			_voiceEarlier.back().named = !c.name.equalsIgnoreCase(clip->name);
			c.handle = Audio::SoundHandle();
		}
	}
	if (_mixer->isSoundHandleActive(c.handle))
		_mixer->stopHandle(c.handle);
	c.lipSyncFps = clip->lipSyncFps;
	if (!_archives[f].readLipSync(*clip, c.lipSync))
		c.lipSync.clear();

	if (f != kSfx && streamsPlaying() >= kStreams) {
		// The driver has four streams (0x4428A0). With all four playing the
		// original logs "Failed to create stream", and the clip plays nothing.
		// The mixer's 32 slots hold these four and the effects.
		delete stream;
		c.name = clip->name;
		c.lipSync.clear();
		if (!background)
			_last = f;
		debugC(1, kDebugAudio, "Failed to create stream for %s: %u streams playing", clip->name.c_str(), kStreams);
		return true;
	}

	// The volume byte is a percentage. Looping is the flag at +0x0C, an
	// inference that fits the 10 music tracks and 3 ambient effects that set
	// it. Pan is 0 on every clip, so the balance stays centred.
	Audio::AudioStream *playable = clip->loop ? Audio::makeLoopingAudioStream(stream, 0) : stream;
	// A clip starts at its file's percent (0x43CC20 reads the file's +0x18).
	c.volume = (byte)(MIN<uint>(clip->volume, 100) * Audio::Mixer::kMaxChannelVolume / 100);
	const byte volume = (byte)(c.volume * getFilePercent(f) / 100);
	_mixer->playStream(kSoundTypes[f], &c.handle, playable, -1, volume);
	c.name = clip->name;
	if (!background)
		_last = f;
	debugC(1, kDebugAudio, "play %s from %s: %u Hz, %u ch, %u bits, %u bytes, volume %u%%%s", clip->name.c_str(),
	       kFileNames[f], clip->rate, clip->channels, clip->bits, clip->dataSize, clip->volume, clip->loop ? ", looping" : "");
	return true;
}

uint Sound::streamsPlaying() const {
	uint n = 0;
	for (uint f = kVoice; f <= kMusic; f++)
		n += _mixer->isSoundHandleActive(_channels[f].handle) ? 1 : 0;
	for (uint i = 0; i < _voiceEarlier.size(); i++)
		n += _mixer->isSoundHandleActive(_voiceEarlier[i].handle) ? 1 : 0;
	return n;
}

void Sound::stopEarlierEffects(bool unnamed) {
	for (uint i = 0; i < _sfxEarlier.size(); i++) {
		if (_mixer && _mixer->isSoundHandleActive(_sfxEarlier[i].handle))
			_mixer->stopHandle(_sfxEarlier[i].handle);
	}
	_sfxEarlier.clear();
	// 0x43C850 frees the streams the clips' names hold; a line played over
	// by itself has none, and plays on.
	for (uint i = 0; i < _voiceEarlier.size();) {
		const bool active = _mixer && _mixer->isSoundHandleActive(_voiceEarlier[i].handle);
		if (active && !_voiceEarlier[i].named && !unnamed) {
			i++;
			continue;
		}
		if (active)
			_mixer->stopHandle(_voiceEarlier[i].handle);
		_voiceEarlier.remove_at(i);
	}
}

void Sound::stop() {
	stopEarlierEffects(true);
	for (uint f = 0; f < kFileCount; f++) {
		if (_mixer && _mixer->isSoundHandleActive(_channels[f].handle))
			_mixer->stopHandle(_channels[f].handle);
		_channels[f].name.clear();
	}
}

void Sound::stopEffects() {
	stopEarlierEffects(false);
	for (uint f = kSfx; f <= kVoice; f++) {
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
	for (uint i = 0; i < _sfxEarlier.size(); i++) {
		const Channel &c = _sfxEarlier[i];
		if (c.name.equalsIgnoreCase(name) && _mixer->isSoundHandleActive(c.handle))
			return &c;
	}
	// The original reads a clip by its id, which holds its newest stream.
	for (uint i = 0; i < _voiceEarlier.size(); i++) {
		const Channel &c = _voiceEarlier[i];
		if (c.named && c.name.equalsIgnoreCase(name) && _mixer->isSoundHandleActive(c.handle))
			return &c;
	}
	return nullptr;
}

void Sound::stopClip(const Common::String &name) {
	const Channel *c = findChannel(name);
	if (c)
		_mixer->stopHandle(c->handle);
}

bool Sound::isClipPlaying(const Common::String &name) const {
	return findChannel(name) != nullptr;
}

void Sound::fadeMusic(uint target, uint interval, bool up) {
	_fadeUp = up;
	_fadeDown = !up;
	_fadeTarget = target;
	_fadeInterval = interval;
	_fadeCountdown = interval;
	debugC(1, kDebugAudio, "music fades %s to %u%%, a step every %u ticks, from %u%%", up ? "up" : "down", target, interval, _musicPercent);
}

void Sound::cancelMusicFade() {
	_fadeUp = _fadeDown = false;
	_musicPercent = 100;
	applyMusicPercent();
}

void Sound::tickMusicFade() {
	// 0x434C00: the countdown runs every tick; at 0 it starts again, and a
	// fade moves the music one percent, stopping at its target.
	if (--_fadeCountdown != 0)
		return;
	_fadeCountdown = _fadeInterval;
	if (_fadeUp) {
		const uint v = _musicPercent + 1;
		if (_fadeTarget <= v) {
			_fadeUp = false;
			_musicPercent = _fadeTarget;
		} else {
			_musicPercent = v;
		}
	} else if (_fadeDown) {
		uint v = _musicPercent - 1;
		if (v <= _fadeTarget) {
			_fadeDown = false;
			v = _fadeTarget;
		}
		_musicPercent = v;
	} else {
		return;
	}
	applyMusicPercent();
}

Common::String Sound::describeMusicFade() const {
	if (!_fadeUp && !_fadeDown)
		return "-";
	return Common::String::format("%s %u/%u", _fadeUp ? "up" : "down", _fadeTarget, _fadeInterval);
}

byte Sound::getMusicVolume() const {
	const Channel &c = _channels[kMusic];
	return _mixer && _mixer->isSoundHandleActive(c.handle) ? (byte)_mixer->getChannelVolume(c.handle) : 0;
}

void Sound::setFilePercent(const Common::String &name, uint percent) {
	File f;
	if (!findClip(name, &f))
		return;
	if (f == kMusic) {
		setMusicPercent(percent);
		return;
	}
	_filePercent[f] = percent;
	const Channel *c = findChannel(name);
	if (_mixer && c)
		_mixer->setChannelVolume(c->handle, (byte)(c->volume * percent / 100));
	debugC(1, kDebugAudio, "%s's file at %u%%", name.c_str(), percent);
}

int32 Sound::getClipMillis(const Common::String &name) const {
	// The play cursor over the bytes per second (0x4451F0); the mixer's
	// elapsed time is the same position.
	const Channel *c = findChannel(name);
	return c ? (int32)_mixer->getElapsedTime(c->handle).msecs() : -1;
}

void Sound::applyMusicPercent() {
	Channel &c = _channels[kMusic];
	if (_mixer && _mixer->isSoundHandleActive(c.handle))
		_mixer->setChannelVolume(c.handle, (byte)(c.volume * _musicPercent / 100));
}

bool Sound::getLipShape(const Common::String &name, uint &shape, uint32 *index, uint *high) const {
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
	// 0x442DD0 gives the high nibble too: Big Head shows that clue picture.
	if (high)
		*high = c->lipSync[i] >> 4;
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

} // End of namespace Terraglyph
