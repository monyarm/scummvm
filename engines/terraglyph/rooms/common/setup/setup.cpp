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

#include "common/debug.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/rooms/common/setup/setup.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"
#include "terraglyph/rooms/common/setup/phantomknight.h"
#include "terraglyph/rooms/common/setup/showdown.h"

namespace Terraglyph {

RoomSetup getRoomSetup(const Common::String &gameId, const Common::String &room) {
	const bool phantom = gameId == "phantomknight";
	const RoomSetup *table = phantom ? kPhantom : kShowdown;
	const uint count = phantom ? kPhantomCount : kShowdownCount;
	for (uint i = 0; i < count; i++) {
		if (room == table[i].room)
			return table[i];
	}
	const RoomSetup none = { "", "", "", 0 };
	return none;
}

void Music::enterRoom(const Common::String &music) {
	// 0x40B920: a number is a group; anything else (an empty string too)
	// leaves no group and plays that clip, if it exists.
	const int group = atoi(music.c_str());
	if (group < 1) {
		setGroup(-1);
		if (!music.empty() && _sound.play(music, true)) {
			_clip = music;
			debugC(1, kDebugAudio, "music %s", music.c_str());
		}
		return;
	}
	setGroup(group);
}

void Music::setGroup(int group) {
	// 0x40AA10: nothing while the group is the same; else its next variant,
	// back to 1 when that one doesn't exist. Stop it, then play it.
	if (group == _group)
		return;
	_group = group;
	if (group < 0)
		return;
	int &variant = _variants.getOrCreateVal(group);
	variant++;
	Common::String clip = Common::String::format("Group%d_%d.Music", group, variant);
	if (!_sound.findClip(clip)) {
		variant = 1;
		clip = Common::String::format("Group%d_%d.Music", group, variant);
	}
	_clip = clip;
	_sound.stopClip(clip);
	_sound.play(clip, true);
	debugC(1, kDebugAudio, "music group %d: %s", group, clip.c_str());
}

void Music::setGroup(int group, int volume) {
	// 0x40AA10: the group already playing only takes the volume (0x434B60);
	// a new one starts, then takes it.
	if (group == _group) {
		if (group > 0 && volume != 100)
			_sound.setMusicPercent(volume);
		return;
	}
	setGroup(group);
	if (group >= 0 && volume != 100)
		_sound.setMusicPercent(volume);
}

void Music::tick() {
	// 0x409CB3: with a group playing, its clip over picks the next variant.
	if (_group < 0 || _sound.isClipPlaying(_clip))
		return;
	const int group = _group;
	setGroup(-1);
	setGroup(group);
}

void TerraglyphEngine::saveRoomKeys(Common::INIFile &profile) {
	// 0x42F3A0: every room in the game's list writes its own section: the
	// base room's keys (0x40C410); its class's follow (vtable +0x1C).
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String room = _art->getRoomName(i);
		const RoomScript::Memory &m = _script->getMemory(room);
		profile.setKey("OpeningPlayed", room, m.openingPlayed ? "Yes" : "No");
		profile.setKey("SnacksGiven", room, Common::String::format("%u", m.snacksGiven));
	}
}

void TerraglyphEngine::loadRoomKeys(const Common::INIFile &profile) {
	// 0x42F640 -> each room's vtable +0x20, the base part (0x40C470): only
	// Yes is true, and a missing key reads as empty: No, 0 (atoi).
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String room = _art->getRoomName(i);
		RoomScript::Memory &m = _script->getMemory(room);
		Common::String v;
		m.openingPlayed = profile.getKey("OpeningPlayed", room, v) && v == "Yes";
		m.snacksGiven = profile.getKey("SnacksGiven", room, v) ? (uint)MAX(0, atoi(v.c_str())) : 0;
	}
}

bool TerraglyphEngine::isClueOwner(const Common::String &name) const {
	// 0x40A870: the five ClueOwners names (game +0xD8).
	for (int i = 0; i < 5; i++) {
		if (getGameString(Common::String::format("ClueOwners%d", i)) == name)
			return true;
	}
	return false;
}

} // End of namespace Terraglyph
