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

// The Gen 1 player profile. The original keeps no save slots: the Sign In
// room picks a player, whose <name>.sav (an INI file) the game reads on
// entry (game vtable +0x58, 0x42F640) and writes as it plays (+0x54,
// 0x42F3A0). A ScummVM save here holds that profile's text, so it carries
// the original's sections and keys, and keeps the keys this engine doesn't
// model yet (the story's) as it found them.

#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/textconsole.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const uint32 kSaveMagic = MKTAG('S', 'C', 'B', 'Y');
static const uint32 kSaveVersion = 1;
// MovieHistory0-74 (0x42DE10's ids).
static const int kMovieIds = 75;

// The rooms that don't become StartRoom (0x40B920 skips game +0x48 for them).
static const char *const kMenuRooms[] = {
	"ROOM_Main_Menu", "ROOM_Options", "ROOM_Suspect_Chooser", "ROOM_Sign_In",
	"ROOM_Difficulty", "ROOM_Quit", "ROOM_Big_Head"
};

bool ScoobyEngine::isMenuRoom(const Common::String &room) {
	for (uint i = 0; i < ARRAYSIZE(kMenuRooms); i++) {
		if (room == kMenuRooms[i])
			return true;
	}
	return false;
}

void ScoobyEngine::buildProfile(Common::INIFile &profile) {
	// 0x42F3A0 and 0x40A270 write the Game section; 0x402130 writes an
	// exit's Exits key when it is taken, 0x401350 a clickable's state when
	// its room is left, and 0x407780 the Inventory section.
	if (!_startRoom.empty())
		profile.setKey("StartRoom", "Game", _startRoom);
	for (int id = 0; id < kMovieIds; id++)
		profile.setKey(Common::String::format("MovieHistory%d", id), "Game", _seenMovies.contains(id) ? "Yes" : "No");
	for (Common::HashMap<Common::String, bool, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo>::const_iterator it = _takenExits.begin();
	     it != _takenExits.end(); ++it)
		profile.setKey(it->_key, "Exits", "1");
	const RoomScript::StateMap &states = _script->getClickableStates();
	for (RoomScript::StateMap::const_iterator it = states.begin(); it != states.end(); ++it)
		profile.setKey(it->_key, "Clickables", it->_value);
	_inventory->saveProfile(profile);
}

bool ScoobyEngine::applyProfile(const Common::INIFile &profile) {
	// 0x42F640: the Game section, then each room's keys, then the
	// inventory (0x407430), then StartRoom.
	_profile = profile;
	Common::String v;
	if (profile.getKey("StartRoom", "Game", v) && !v.empty())
		_startRoom = v;
	_seenMovies.clear();
	for (int id = 0; id < kMovieIds; id++) {
		// 0x40A170: only "Yes" is true.
		if (profile.getKey(Common::String::format("MovieHistory%d", id), "Game", v) && v == "Yes")
			_seenMovies[id] = true;
	}
	_takenExits.clear();
	_script->resetClickableStates();
	const Common::INIFile::SectionList &sections = profile.getSections();
	for (Common::INIFile::SectionList::const_iterator s = sections.begin(); s != sections.end(); ++s) {
		const bool exits = s->name.equalsIgnoreCase("Exits");
		const bool clickables = s->name.equalsIgnoreCase("Clickables");
		if (!exits && !clickables)
			continue;
		for (Common::INIFile::SectionKeyList::const_iterator k = s->keys.begin(); k != s->keys.end(); ++k) {
			Common::String value = k->value;
			value.trim();
			// 0x401850 counts an exit as taken for 1, on, On, ON or TRUE.
			if (exits && (value == "1" || value == "on" || value == "On" || value == "ON" || value == "TRUE"))
				_takenExits[k->key] = true;
			else if (clickables && !value.empty())
				_script->setClickableState(k->key, value);
		}
	}
	_inventory->loadProfile(profile);
	debugC(1, kDebugScript, "profile read: StartRoom %s, %u movies seen, %u exits taken, %u clickable states", _startRoom.c_str(),
	       _seenMovies.size(), _takenExits.size(), _script->getClickableStates().size());
	return !_startRoom.empty();
}

Common::String ScoobyEngine::getProfileText() {
	Common::INIFile profile = _profile;
	buildProfile(profile);
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
	profile.saveToStream(out);
	return Common::String((const char *)out.getData(), out.size());
}

bool ScoobyEngine::readProfileText(const Common::String &text, Common::INIFile &profile) {
	Common::MemoryReadStream in((const byte *)text.c_str(), text.size());
	return profile.loadFromStream(in);
}

bool ScoobyEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	// Not while a scene or an exit plays, nor before a room is up.
	return _currentRoom && _script && _inventory && !_startRoom.empty() && !_script->isBusy() && !_inventory->isInputBlocked();
}

bool ScoobyEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _currentRoom && _script && _inventory && !_script->isBusy() && !_inventory->isInputBlocked();
}

Common::Error ScoobyEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	const Common::String text = getProfileText();
	stream->writeUint32BE(kSaveMagic);
	stream->writeUint32LE(kSaveVersion);
	stream->writeUint32LE(text.size());
	stream->write(text.c_str(), text.size());
	debugC(1, kDebugScript, "profile saved: %u bytes, StartRoom %s", text.size(), _startRoom.c_str());
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error ScoobyEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (stream->readUint32BE() != kSaveMagic || stream->readUint32LE() > kSaveVersion)
		return Common::kUnknownError;
	const uint32 size = stream->readUint32LE();
	if (size > (uint32)(stream->size() - stream->pos()))
		return Common::kReadingFailed;
	Common::String text;
	for (uint32 i = 0; i < size; i++)
		text += (char)stream->readByte();
	Common::INIFile profile;
	if (stream->err() || !readProfileText(text, profile))
		return Common::kReadingFailed;
	return loadProfile(profile) ? Common::kNoError : Common::kReadingFailed;
}

bool ScoobyEngine::loadProfile(const Common::INIFile &profile) {
	if (!applyProfile(profile)) {
		warning("The profile names no StartRoom");
		return false;
	}
	_sound->stop();
	const Common::String room = _startRoom;
	if (!showRoom(room)) {
		warning("The profile's StartRoom %s can't be shown", room.c_str());
		return false;
	}
	updateHover(_mousePos.x, _mousePos.y);
	return true;
}

void ScoobyEngine::checkProfile() {
	// A state in every kind the profile holds, written, read back into a new
	// game and written again; then, if "profile_check_file" names a file, an
	// original-format profile read in and written out.
	const Common::String first = _art->getRoomName(0);
	const Common::INIFile::SectionList &iniSections = _objectIni.getSections();
	Common::Array<Common::String> items;
	for (Common::INIFile::SectionList::const_iterator it = iniSections.begin(); it != iniSections.end(); ++it) {
		Common::String id;
		if (!_objectIni.getKey("ID", it->name, id))
			continue;
		id.trim();
		if (id.equalsIgnoreCase("inventoryobject"))
			items.push_back(it->name);
	}
	Common::sort(items.begin(), items.end());

	// The state: two items in their slots and a third taken and used up, 7
	// snacks, a suspect met, movies 1 and 32 seen, the first room's first
	// exit taken, its first clickable on its second state.
	showRoom(first);
	_inventory->reset();
	_seenMovies.clear();
	_takenExits.clear();
	_script->resetClickableStates();
	_profile = Common::INIFile();
	for (uint i = 0; i < 3 && i < items.size(); i++)
		_inventory->addItem(items[i]);
	if (items.size() > 2)
		_inventory->removeItem(items[2]);
	_inventory->setSnacks(7);
	_inventory->meetSuspect("ARTIE");
	_seenMovies[1] = _seenMovies[32] = true;
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (!_currentRoom->getDestination(s).empty()) {
			_takenExits[_currentRoom->getObjectName(s)] = true;
			break;
		}
	}
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (_currentRoom->isClickable(s)) {
			_script->setClickableState(_currentRoom->getObjectName(s), _currentRoom->getAnim(s) + "_NEXT");
			break;
		}
	}
	_startRoom = first;

	const Common::String written = getProfileText();
	logProfile("written", written);

	// Into a new game, and out again.
	_inventory->reset();
	_seenMovies.clear();
	_takenExits.clear();
	_script->resetClickableStates();
	_startRoom.clear();
	_profile = Common::INIFile();
	Common::INIFile read;
	readProfileText(written, read);
	loadProfile(read);
	const Common::String again = getProfileText();
	debugC(3, kDebugScript, "profile check round trip: %s, room %s, slots %s %s, snacks %u", again == written ? "same" : "DIFFERENT",
	       _currentRoom->getName().c_str(), _inventory->getSlot(0).c_str(), _inventory->getSlot(1).c_str(), _inventory->getSnacks());

	// An original-format profile.
	if (ConfMan.hasKey("profile_check_file")) {
		Common::FSNode node(ConfMan.getPath("profile_check_file"));
		Common::SeekableReadStream *in = node.createReadStream();
		if (!in) {
			warning("profile check: cannot open %s", node.getPath().toString().c_str());
		} else {
			Common::String text;
			while (!in->eos() && !in->err()) {
				const byte b = in->readByte();
				if (!in->eos())
					text += (char)b;
			}
			delete in;
			_inventory->reset();
			_seenMovies.clear();
			_takenExits.clear();
			_script->resetClickableStates();
			_startRoom.clear();
			Common::INIFile imported;
			readProfileText(text, imported);
			const bool ok = loadProfile(imported);
			debugC(3, kDebugScript, "profile check import: %s, room %s", ok ? "loaded" : "FAILED", _currentRoom->getName().c_str());
			logProfile("imported", getProfileText());
		}
	}

	// Back to a new game.
	_inventory->reset();
	_seenMovies.clear();
	_takenExits.clear();
	_script->resetClickableStates();
	_startRoom.clear();
	_profile = Common::INIFile();
	debugC(3, kDebugScript, "profile check: done");
}

void ScoobyEngine::logProfile(const char *what, const Common::String &text) {
	Common::INIFile profile;
	readProfileText(text, profile);
	uint keys = 0;
	const Common::INIFile::SectionList &sections = profile.getSections();
	for (Common::INIFile::SectionList::const_iterator s = sections.begin(); s != sections.end(); ++s) {
		for (Common::INIFile::SectionKeyList::const_iterator k = s->keys.begin(); k != s->keys.end(); ++k, keys++)
			debugC(3, kDebugScript, "profile check %s [%s] %s=%s", what, s->name.c_str(), k->key.c_str(), k->value.c_str());
	}
	debugC(3, kDebugScript, "profile check %s: %u keys", what, keys);
}

} // End of namespace Scooby
