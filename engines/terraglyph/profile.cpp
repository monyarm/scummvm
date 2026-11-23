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
#include "engines/metaengine.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/signin_room.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const uint32 kSaveMagic = MKTAG('S', 'C', 'B', 'Y');
static const uint32 kSaveVersion = 1;
// MovieHistory0-74 (0x42DE10's ids).
static const int kMovieIds = 75;

// The rooms that don't become StartRoom (0x40B920 skips game +0x48 for them).
static const char *const kMenuRooms[] = {
	"ROOM_Main_Menu", "ROOM_Options", "ROOM_Suspect_Chooser", "ROOM_Sign_In",
	"ROOM_Difficulty", "ROOM_Quit", "ROOM_Big_Head"
};

bool TerraglyphEngine::isMenuRoom(const Common::String &room) {
	for (uint i = 0; i < ARRAYSIZE(kMenuRooms); i++) {
		if (room == kMenuRooms[i])
			return true;
	}
	return false;
}

void TerraglyphEngine::buildProfile(Common::INIFile &profile) {
	// 0x42F3A0 and 0x40A270 write the Game section; 0x402130 writes an
	// exit's Exits key when it is taken, 0x401350 a clickable's state when
	// its room is left, and 0x407780 the Inventory section.
	if (!_startRoom.empty())
		profile.setKey("StartRoom", "Game", _startRoom);
	profile.setKey("Difficulty", "Game", Common::String::format("%d", _difficulty));
	for (int id = 0; id < kMovieIds; id++)
		profile.setKey(Common::String::format("MovieHistory%d", id), "Game", _seenMovies.contains(id) ? "Yes" : "No");
	// Each room's section (room_setup.cpp).
	saveRoomKeys(profile);
	for (Common::HashMap<Common::String, bool, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo>::const_iterator it = _takenExits.begin();
	     it != _takenExits.end(); ++it)
		profile.setKey(it->_key, "Exits", "1");
	const RoomScript::StateMap &states = _script->getClickableStates();
	for (RoomScript::StateMap::const_iterator it = states.begin(); it != states.end(); ++it)
		profile.setKey(it->_key, "Clickables", it->_value);
	// Each room class's own keys (room vtable +0x1C).
	Common::Array<RoomClass *> classes = getAllRoomClasses();
	for (uint i = 0; i < classes.size(); i++)
		classes[i]->saveKeys(profile);
	_inventory->saveProfile(profile);
}

bool TerraglyphEngine::applyProfile(const Common::INIFile &profile) {
	// 0x42F640: the Game section, then each room's keys, then the
	// inventory (0x407430), then StartRoom.
	_profile = profile;
	Common::String v;
	if (profile.getKey("StartRoom", "Game", v) && !v.empty())
		_startRoom = v;
	// 0x40A540: atoi.
	if (profile.getKey("Difficulty", "Game", v) && !v.empty())
		_difficulty = atoi(v.c_str());
	_seenMovies.clear();
	for (int id = 0; id < kMovieIds; id++) {
		// 0x40A170: only "Yes" is true.
		if (profile.getKey(Common::String::format("MovieHistory%d", id), "Game", v) && v == "Yes")
			_seenMovies[id] = true;
	}
	// Each room's keys (room_setup.cpp).
	loadRoomKeys(profile);
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
	// Each room class's own keys (room vtable +0x20).
	Common::Array<RoomClass *> classes = getAllRoomClasses();
	for (uint i = 0; i < classes.size(); i++)
		classes[i]->loadKeys(profile);
	_inventory->loadProfile(profile);
	debugC(1, kDebugScript, "profile read: StartRoom %s, %u movies seen, %u exits taken, %u clickable states", _startRoom.c_str(),
	       _seenMovies.size(), _takenExits.size(), _script->getClickableStates().size());
	return !_startRoom.empty();
}

Common::String TerraglyphEngine::getProfileText() {
	Common::INIFile profile = _profile;
	buildProfile(profile);
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
	profile.saveToStream(out);
	return Common::String((const char *)out.getData(), out.size());
}

bool TerraglyphEngine::readProfileText(const Common::String &text, Common::INIFile &profile) {
	Common::MemoryReadStream in((const byte *)text.c_str(), text.size());
	return profile.loadFromStream(in);
}

bool TerraglyphEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	// Not while a scene or an exit plays, nor before a room is up.
	return _currentRoom && _script && _inventory && !_startRoom.empty() && !_script->isBusy() && !_inventory->isInputBlocked();
}

bool TerraglyphEngine::canSaveAutosaveCurrently() {
	// ScummVM autosaves before every load; from Sign In that would put a
	// blank new game over the last autosave.
	return canSaveGameStateCurrently() && !isMenuRoom(_currentRoom->getName());
}

bool TerraglyphEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _currentRoom && _script && _inventory && !_script->isBusy() && !_inventory->isInputBlocked();
}

Common::Error TerraglyphEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	const Common::String text = getProfileText();
	stream->writeUint32BE(kSaveMagic);
	stream->writeUint32LE(kSaveVersion);
	stream->writeUint32LE(text.size());
	stream->write(text.c_str(), text.size());
	debugC(1, kDebugScript, "profile saved: %u bytes, StartRoom %s", text.size(), _startRoom.c_str());
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error TerraglyphEngine::loadGameStream(Common::SeekableReadStream *stream) {
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

// ---------------------------------------------------------------------------
// Players and new games

void TerraglyphEngine::newGame() {
	// 0x4087B0, through Showdown's game vtable +0x4C (0x42DBC0): the level,
	// the speech lock, the suspects and the clues, the player, and StartRoom
	// ROOM_Difficulty. Then every room object's reset (vtable +0x08,
	// 0x435780) and the inventory's (+0x50). The rooms' exits and
	// clickables start again with them, and the movies seen (0x42DBC0).
	_difficulty = 0;
	_newClue = false;
	_playerName.clear();
	_playerSlot = -1;
	_startRoom = "ROOM_Difficulty";
	_profile = Common::INIFile();
	_seenMovies.clear();
	_takenExits.clear();
	_script->resetClickableStates();
	// The base room's reset (0x40CD20): no opening played, no snacks eaten.
	_script->resetMemories();
	Common::Array<RoomClass *> classes = getAllRoomClasses();
	for (uint i = 0; i < classes.size(); i++)
		classes[i]->reset();
	_inventory->reset();
	setStoryValues();
	debugC(1, kDebugScript, "new game");
}

void TerraglyphEngine::setStoryValues() {
	if (getGameId() != "showdown") {
		// Phantom's game vtable +0x4C (0x42E880), written as its 0x42FFE0
		// names them: every puzzle and ball flag off, Jane screaming, the
		// villain (rand() % 5), the crest's sequence 1-4 shuffled by five
		// swaps of two rand() & 3 places, and the skull's four digits
		// (rand() & 3 each).
		static const char *const kNo[] = {
			"EwanPuzzle", "MariaPuzzle", "BradPuzzle", "SkullPuzzle", "SkullMovie", "SkullPuzzleAttempted", "CrestPuzzle",
			"RedBallFound", "BlueBallFound", "BlueBallSeen", "YellowBallFound", "RedBallTaken", "BlueBallTaken",
			"YellowBallTaken", "WellVisited", "MazeEntered", "PlaqueSeen"
		};
		static const char *const kVillains[] = { "MARIA", "BRAD", "EWAN", "WIZARD", "JESTER" };
		for (uint i = 0; i < ARRAYSIZE(kNo); i++)
			_profile.setKey(kNo[i], "Game", "No");
		_profile.setKey("Screaming", "Game", "Yes");
		_profile.setKey("Whodunit", "Game", kVillains[_randomSource.getRandomNumber(4)]);
		int crest[4] = { 1, 2, 3, 4 };
		for (uint k = 0; k < 5; k++) {
			const uint a = _randomSource.getRandomNumber(3);
			const uint b = _randomSource.getRandomNumber(3);
			SWAP(crest[a], crest[b]);
		}
		for (uint i = 0; i < 4; i++)
			_profile.setKey(Common::String::format("CrestSequence%u", i), "Game", Common::String::format("%d", crest[i]));
		for (uint i = 0; i < 4; i++)
			_profile.setKey(Common::String::format("SkullCombo%u", i), "Game", Common::String::format("%u", _randomSource.getRandomNumber(3)));
		return;
	}
	// 0x42DBC0, written to the profile's Game section as 0x42F3A0 names
	// them: the puzzles' flags, the clock at 12:00, the grave (0-8), the
	// strong box (0-3), the four levers' aliases (0-3, each once), the
	// villain and the artifact.
	static const char *const kYes[] = { "MoaningActive", "SheriffWaiting" };
	static const char *const kNo[] = { "HotelHallDoor", "SheriffPuzzle", "GummyPuzzle", "TunnelLit", "VaultOpen", "BagsRandomized" };
	static const char *const kVillains[] = { "ARTIE", "ICDOUBLE", "JENNY", "SHERIFF", "GUMMY" };
	static const char *const kArtifacts[] = { "Bowl", "Deer", "Vase" };
	for (uint i = 0; i < ARRAYSIZE(kYes); i++)
		_profile.setKey(kYes[i], "Game", "Yes");
	for (uint i = 0; i < ARRAYSIZE(kNo); i++)
		_profile.setKey(kNo[i], "Game", "No");
	_profile.setKey("ClockHour", "Game", "12");
	_profile.setKey("ClockMinute", "Game", "0");
	_profile.setKey("GraveLocation", "Game", Common::String::format("%u", _randomSource.getRandomNumber(8)));
	_profile.setKey("StrongBox", "Game", Common::String::format("%u", _randomSource.getRandomNumber(3)));
	// Each lever draws until it differs from those before it.
	uint levers[4];
	for (uint i = 0; i < 4; i++) {
		bool again;
		do {
			levers[i] = _randomSource.getRandomNumber(3);
			again = false;
			for (uint k = 0; k < i; k++)
				again = again || levers[k] == levers[i];
		} while (again);
		_profile.setKey(Common::String::format("LeverAlias%u", i), "Game", Common::String::format("%u", levers[i]));
	}
	_profile.setKey("Whodunit", "Game", kVillains[_randomSource.getRandomNumber(4)]);
	_profile.setKey("ArtifactType", "Game", kArtifacts[_randomSource.getRandomNumber(2)]);
}

bool TerraglyphEngine::getGameFlag(const Common::String &key) const {
	return getGameString(key) == "Yes";
}

int TerraglyphEngine::getGameInt(const Common::String &key) const {
	return atoi(getGameString(key).c_str());
}

Common::String TerraglyphEngine::getGameString(const Common::String &key) const {
	Common::String v;
	return _profile.getKey(key, "Game", v) ? v : Common::String();
}

void TerraglyphEngine::markAllMoviesSeen() {
	for (int id = 0; id < kMovieIds; id++)
		_seenMovies[id] = true;
}

void TerraglyphEngine::listPlayers(Common::Array<Common::String> &names, Common::Array<int> &slots) {
	// The saves named as Sign In can show them, by name (the original's
	// file listing); the autosave and other saves aren't players. A name
	// saved twice is the lower slot.
	names.clear();
	slots.clear();
	if (!getMetaEngine())
		return;
	SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str());
	Common::Array<Common::String> sorted;
	Common::HashMap<Common::String, int> slotOf;
	for (SaveStateList::const_iterator it = saves.begin(); it != saves.end(); ++it) {
		const Common::String name = it->getDescription();
		const int slot = it->getSaveSlot();
		if (slot == getAutosaveSlot() || !SignInRoom::isPlayerName(name))
			continue;
		int known;
		if (slotOf.tryGetVal(name, known)) {
			slotOf[name] = MIN(known, slot);
			continue;
		}
		slotOf[name] = slot;
		sorted.push_back(name);
	}
	Common::sort(sorted.begin(), sorted.end());
	for (uint i = 0; i < sorted.size(); i++) {
		names.push_back(sorted[i]);
		slots.push_back(slotOf[sorted[i]]);
	}
}

void TerraglyphEngine::signIn(const Common::String &name, int slot) {
	// Game +0x16C. A new player takes the first free slot after the
	// autosave's.
	_playerName = name;
	if (slot < 0 && getMetaEngine()) {
		SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str());
		const int last = getMetaEngine()->getMaximumSaveSlot();
		for (slot = 0; slot <= last; slot++) {
			bool used = slot == getAutosaveSlot();
			for (SaveStateList::const_iterator it = saves.begin(); it != saves.end() && !used; ++it)
				used = it->getSaveSlot() == slot;
			if (!used)
				break;
		}
		if (slot > last)
			slot = -1;
	}
	_playerSlot = slot;
	debugC(1, kDebugScript, "signed in as %s, slot %d", name.c_str(), slot);
}

void TerraglyphEngine::deletePlayer(int slot) {
	if (getMetaEngine() && !_checking)
		getMetaEngine()->removeSaveState(_targetName.c_str(), slot);
	if (slot == _playerSlot) {
		_playerSlot = -1;
		_playerName.clear();
	}
}

Common::Error TerraglyphEngine::loadGameState(int slot) {
	// A player's save signs them in; another save signs nobody in, so
	// nothing is written for it.
	_playerName.clear();
	_playerSlot = -1;
	if (getMetaEngine()) {
		const Common::String name = getMetaEngine()->querySaveMetaInfos(_targetName.c_str(), slot).getDescription();
		if (slot != getAutosaveSlot() && SignInRoom::isPlayerName(name)) {
			_playerName = name;
			_playerSlot = slot;
		}
	}
	return Engine::loadGameState(slot);
}

bool TerraglyphEngine::loadProfile(const Common::INIFile &profile) {
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

void TerraglyphEngine::checkProfile() {
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

void TerraglyphEngine::logProfile(const char *what, const Common::String &text) {
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

} // End of namespace Terraglyph
