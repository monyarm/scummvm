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

#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/room_setup.h"
#include "scooby/saloon_rooms.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kSaloon = "ROOM_P12_Saloon";
static const char *const kArtieRoom = "ROOM_Artie";
static const char *const kStage = "ROOM_P05_Stage";
static const char *const kPiano = "OBJ_P12_SALOON_HOTSPOT1";
static const char *const kPianoA = "ANIM_P12_SALOON_HOTSPOT1A";
static const char *const kPianoB = "ANIM_P12_SALOON_HOTSPOT1B";
static const char *const kBull = "OBJ_P12_SALOON_MECHBULL";
static const char *const kNickel = "OBJ_INV_NICKEL";
static const char *const kNickelCursor = "ANIM_CURSORBuffalo_Nickel";
static const char *const kPumpHandle = "OBJ_INV_PUMP_HANDLE";
static const char *const kArrow = "ANIM_CURSORARROW";
static const char *const kOpenHand = "ANIM_CURSOROPEN";

static int findSprite(const Room &room, const Common::String &object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

/** 0x4070C0: the item is in one of the slots. */
static bool held(const Inventory &inventory, const char *item) {
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (inventory.getSlot(i) == item)
			return true;
	}
	return false;
}

/** Hotspots 3-8 and their keys: k 0-5 from the name's last digit, or -1. */
static int keyIndex(const Common::String &object, const char *prefix) {
	if (!object.hasPrefix(prefix) || object.size() != strlen(prefix) + 1)
		return -1;
	const int k = object.lastChar() - '3';
	return k >= 0 && k < (int)SaloonRoom::kKeys ? k : -1;
}

static const char *const kHotspotPrefix = "OBJ_P12_SALOON_HOTSPOT";
static const char *const kKeyPrefix = "OBJ_SALOON_KEY_HOTSPOT";

/** A clickable's state with its last letter @p c (0x4318E0, 0x4323A0). */
static Common::String withLast(const Common::String &state, char c) {
	if (state.empty())
		return state;
	Common::String s(state.c_str(), state.size() - 1);
	s += c;
	return s;
}

// ---------------------------------------------------------------------------
// The saloon

void SaloonRoom::reset() {
	// 0x414490, after the base reset: Artie gagged and in three chains, and
	// three keys hidden behind three of the six hotspots (rand() % 6).
	_moan = 36;
	_music = false;
	_masked = true;
	_chained = 3;
	_shaggyPiano = false;
	_gives = false;
	_thanks = false;
	_bullRidden = false;
	for (uint k = 0; k < kKeys; k++) {
		_keyVisible[k] = false;
		_keyHidden[k] = false;
		_hotspots[k] = -1;
		_keys[k] = -1;
		_keyFound[k] = false;
		_open[k] = false;
	}
	for (uint hidden = 0; hidden < 3;) {
		const uint k = _engine.getRandomNumber(kKeys - 1);
		if (!_keyHidden[k]) {
			_keyHidden[k] = true;
			hidden++;
		}
	}
}

void SaloonRoom::hideKeys(uint mask) {
	for (uint k = 0; k < kKeys; k++)
		_keyHidden[k] = (mask >> k) & 1;
}

bool SaloonRoom::scene(const char *name, bool block) {
	if (!_engine.getScript()->startScene(name, block))
		return false;
	_lastScene = name;
	return true;
}

void SaloonRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

void SaloonRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x414FC0. Everything it builds goes under its RENDER_ENTITY (a buffer
	// sprite in layer 0 at depth -1001), but the bull:
	// - hotspot 1, the piano: a clickable (0x408DC0 with 1) at depth -11;
	// - hotspot 2, hotspots 3-8 and the keys: clickables at the depth
	//   0x408DC0 gives them (priority=, -1 without one); the keys hidden;
	// - the open door: a sprite at depth 0, looping, once the hotel hall's
	//   door is open (HotelHallDoor, game +0x255); else not built;
	// - the chandelier: a sprite at depth -1, looping, until the nickel is
	//   used (had and not held); then not built;
	// - the bull: a requires object (0x408C80 with -1: layer 0), looping
	//   MECHBULLA, or MECHBULLB once the nickel is used;
	// - the way to Pie Noon: not built while Artie is chained.
	// The clickables' states it sets are set on entry, once the base entry
	// has built them.
	Inventory &inventory = *_engine.getInventory();
	const bool used = inventory.hasItem(kNickel) && !held(inventory, kNickel);
	if (object == kPiano) {
		claim.build = Room::Claim::kClickable;
		claim.group = true;
		claim.depth = -11;
	} else if (object == "OBJ_P12_SALOON_HOTSPOT2" || keyIndex(object, kHotspotPrefix) >= 0) {
		claim.build = Room::Claim::kClickable;
		claim.group = true;
		claim.depth = -1;
	} else if (keyIndex(object, kKeyPrefix) >= 0) {
		claim.build = Room::Claim::kClickable;
		claim.group = true;
		claim.depth = -1;
		claim.hidden = true;
	} else if (object == "OBJ_P12_SALOON_OPEN_DOOR") {
		if (_engine.getGameFlag("HotelHallDoor")) {
			claim.build = Room::Claim::kSprite;
			claim.group = true;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == "OBJ_P12_SALOON_CHAND") {
		if (!used) {
			claim.build = Room::Claim::kSprite;
			claim.group = true;
			claim.depth = -1;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == kBull) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.anim = used ? "ANIM_P12_SALOON_MECHBULLB" : "ANIM_P12_SALOON_MECHBULLA";
	} else if (object == "OBJ_P12_SALOON_PIE_NOON") {
		if (_chained > 0)
			claim.build = Room::Claim::kSkip;
	}
}

void SaloonRoom::enter(const Common::String &from) {
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	_lastScene.clear();
	_lastSound.clear();
	_lastExit = RoomScript::Exit();
	_artieClicked = false;
	_shaggyNickel = false;

	// The claim's states (0x414FC0). The piano on its B once played; while
	// Artie is gagged, hotspots 3-8 without the open hand (0x401770); one
	// left open on its B, one with a key on its K, each stopped on step 0.
	_piano = findSprite(room, kPiano);
	if (_piano >= 0 && _shaggyPiano)
		script.setClickable(_piano, kPianoB);
	for (uint k = 0; k < kKeys; k++) {
		_hotspots[k] = findSprite(room, Common::String::format("%s%u", kHotspotPrefix, k + 3));
		_keys[k] = findSprite(room, Common::String::format("%s%u", kKeyPrefix, k + 3));
		_keyFound[k] = false;
		const int h = _hotspots[k];
		if (h < 0)
			continue;
		if (_masked)
			room.setRollover(h, "");
		if (_open[k]) {
			const Common::String st = withLast(script.getClickableState(h), 'B');
			script.setClickable(h, st);
			room.startAnim(h, st, 0, -1);
		}
		if (_keyHidden[k]) {
			const Common::String st = withLast(script.getClickableState(h), 'K');
			script.setClickable(h, st);
			room.startAnim(h, st, 0, -1);
		}
	}

	// 0x414540, after the base entry. From the hotel hall Fred walks in UR,
	// from Street 3 Daphne UL, from the stage Velma U.
	const char *who = nullptr, *dir = nullptr;
	if (from == "ROOM_P17_Hotel_Hall") {
		who = "FRED";
		dir = "UR";
	} else if (from == "ROOM_P11_Street3") {
		who = "DAPHNE";
		dir = "UL";
	} else if (from == kStage) {
		who = "VELMA";
		dir = "U";
	}
	if (who) {
		if (Character *c = script.getCharacter(who))
			c->walkIn(room, sound, dir);
	}
	_music = true;
	Character *artie = script.getCharacter("ARTIE");
	// Chained: Artie in his chains, OBJ_ARTIE_<n>, idle.
	if (_chained > 0 && artie) {
		artie->setSet((char)('0' + _chained));
		artie->setState(room, sound, Character::kStateIdle);
	}
	if (artie) {
		if (!_masked)
			room.setDepth(artie->getSprite(), -1);
		else
			artie->specialLoop(room, sound, "OBJ_ARTIE_MASKED");
	}
	// Free, and his clue not yet given: 360C_ (1 in 3) or 360C3_, held; his
	// clue (game vtable +0x44), Artie met (0x40A8B0), and he gives it when
	// idle.
	if (_chained == 0 && !_engine.isClueOwner("ARTIE")) {
		scene(_engine.getRandomNumber(2) == 0 ? "Saloon.360C_" : "Saloon.360C3_", true);
		_engine.giveClue("ARTIE");
		inventory.meetSuspect("ARTIE");
		_gives = true;
	}
	// The keys found and not yet used, shown at depth 1.
	for (uint k = 0; k < kKeys; k++) {
		if (_keyVisible[k] && _keys[k] >= 0) {
			room.setHidden(_keys[k], false);
			room.setDepth(_keys[k], 1);
		}
	}
	_bullTries = 0;
	_ungag = false;
	_velmaStarted = false;
	_velmaSpoke = false;
	// The bull ridden: the pump handle off the chandelier.
	if (inventory.hasItem(kNickel) && !held(inventory, kNickel) && !inventory.hasItem(kPumpHandle))
		inventory.addItem(kPumpHandle, "ANIM_TOOLBARPUMP_HANDLE", "ANIM_CURSORPUMP_HANDLE");
	script.setSnacksNeeded(_engine.getDifficulty() + 2);
}

void SaloonRoom::roomLeft() {
	// 0x414820, after the base leave: each of hotspots 3-8 open when its
	// state ends in B (ClickableOpen).
	const RoomScript::StateMap &states = _engine.getScript()->getClickableStates();
	for (uint k = 0; k < kKeys; k++) {
		const Common::String obj = Common::String::format("%s%u", kHotspotPrefix, k + 3);
		_open[k] = states.contains(obj) && !states[obj].empty() && states[obj].lastChar() == 'B';
	}
}

bool SaloonRoom::click(int sprite) {
	// 0x414910 reads Artie's and Shaggy's clicked flags (+8) with the
	// cursor: Artie with the arrow, Shaggy with the nickel.
	if (sprite < 0)
		return false;
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	Character *artie = script.getCharacter("ARTIE");
	Character *shaggy = script.getCharacter("SHAGGY");
	if (artie && sprite == artie->getSprite() && inventory.isDefaultCursor()) {
		_artieClicked = true;
		return true;
	}
	if (shaggy && sprite == shaggy->getSprite() && inventory.getCursor() == kNickelCursor) {
		_shaggyNickel = true;
		return true;
	}
	return false;
}

void SaloonRoom::leave(const char *destination, const char *sceneName) {
	// The room's exit handler (+0x24, 0x415C30) with these keys; for the
	// rooms it builds them for, it never keeps the gang here.
	RoomScript::Exit exit;
	exit.destination = destination;
	exit.scene = sceneName;
	exit.movie = -1;
	_lastExit = exit;
	if (!keepsHere(destination))
		_engine.getScript()->takeExit(exit);
}

bool SaloonRoom::leaveBy(const Common::String &exitObject) {
	// 0x415C30 for an exit clicked. The class's own exits pass no object.
	if (exitObject.empty())
		return false;
	return keepsHere(_engine.getScript()->readExit(exitObject).destination);
}

bool SaloonRoom::keepsHere(const Common::String &destination) {
	// 0x415C30. Anywhere but ROOM_Artie, while Artie is gagged, or chained
	// with no key showing: 360A1_, 360A2_, 360A3_ or Must_Free_Artie_
	// (rand() & 3). Chained with keys showing: 340A2_, G3_ or 360D3_ (rand()
	// & 1 with one key, rand() % 3 with more). Each held, and the gang stays.
	// To the stage without the level's snacks (+0x88 against level + 2): a
	// line by level, and the snack box flashes. Else the default handler.
	RoomScript &script = *_engine.getScript();
	if (destination != kArtieRoom) {
		uint visible = 0;
		for (uint k = 0; k < kKeys; k++)
			visible += _keyVisible[k] ? 1 : 0;
		if (_masked || (_chained >= 1 && visible == 0)) {
			static const char *const kLines[] = { "Saloon.360A1_", "Saloon.360A2_", "Saloon.360A3_", "Saloon.Must_Free_Artie_" };
			scene(kLines[_engine.getRandomNumber(3)], true);
			return true;
		}
		if (_chained >= 1) {
			static const char *const kLines[] = { "Saloon.340A2_", "Saloon.G3_", "Saloon.360D3_" };
			scene(kLines[_engine.getRandomNumber(visible < 2 ? 1 : 2)], true);
			return true;
		}
	}
	const int level = _engine.getDifficulty();
	if (destination == kStage && script.getMemory(kSaloon).snacksGiven < (uint)(level + 2)) {
		static const char *const kLines[] = { "Saloon.380A_", "Saloon.380A2_", "Saloon.380A3_" };
		if (level >= 0 && level <= 2)
			scene(kLines[level], true);
		_engine.getInventory()->flashSnackBox();
		return true;
	}
	return false;
}

void SaloonRoom::idle() {
	// 0x4148D0: Artie free, 1100C_ (its rand() & 1 always passes).
	if (_chained == 0)
		scene("Saloon.1100C_", false);
}

void SaloonRoom::event(const Common::String &object, bool success) {
	// 0x415660.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();

	// The piano: the first time, Piano_. Moved on to anything but its B (it
	// plays its tune), the music group stops until it does (+0xC0); on its
	// A, it goes to its B.
	if (object == kPiano) {
		if (!_shaggyPiano) {
			_shaggyPiano = true;
			scene("Saloon.Piano_", false);
		}
		if (_piano < 0)
			return;
		const Common::String state = script.getClickableState(_piano);
		if (state != kPianoB) {
			_engine.getMusic()->setGroup(-1, 100);
			_music = true;
		}
		if (state == kPianoA)
			script.setClickable(_piano, kPianoB);
		return;
	}

	// Hotspots 3-8, once Artie is ungagged: a key behind it shows once the
	// hotspot stops (+0xD8), and the hotspot goes to its B.
	int k = keyIndex(object, kHotspotPrefix);
	if (k >= 0) {
		if (_masked || !_keyHidden[k])
			return;
		_keyFound[k] = true;
		_keyHidden[k] = false;
		if (_hotspots[k] >= 0)
			script.setClickable(_hotspots[k], withLast(script.getClickableState(_hotspots[k]), 'B'));
		return;
	}

	// A key: hidden and used, a chain fewer, and to ROOM_Artie.
	k = keyIndex(object, kKeyPrefix);
	if (k >= 0) {
		if (_keys[k] >= 0)
			room.setHidden(_keys[k], true);
		_keyVisible[k] = false;
		_chained--;
		leave(kArtieRoom, "");
		return;
	}

	if (object != kBull)
		return;
	// The bull with the nickel: movie 5 and the saloon again (game vtable
	// +0x08), the nickel used, the arrow, BullRidden.
	if (success) {
		_engine.requestRoom(kSaloon, 5);
		inventory.removeItem(kNickel);
		inventory.setCursor(kArrow);
		_bullRidden = true;
		return;
	}
	// Without it: once it is used, 120E2_; else 390A_, 390A2_, 390A3_ in
	// turn. Held.
	if (inventory.hasItem(kNickel) && !held(inventory, kNickel)) {
		scene("Saloon.120E2_", true);
		return;
	}
	const int tries = _bullTries++;
	if (tries == 0) {
		scene("Saloon.390A_", true);
	} else if (tries == 1) {
		scene("Saloon.390A2_", true);
	} else if (tries == 2) {
		scene("Saloon.390A3_", true);
		_bullTries = 0;
	}
}

void SaloonRoom::tick() {
	// 0x414910, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();

	// While the story has moans (MoaningActive, game +0x254), one of four
	// every 5 to 14 seconds.
	const int moan = _moan--;
	if (moan < 1 && _engine.getGameFlag("MoaningActive")) {
		playSound(Common::String::format("Saloon.Moan.%u", _engine.getRandomNumber(3) + 1));
		_moan = ((int)_engine.getRandomNumber(9) + 5) * 36;
	}

	// The piano stopped: music group 3 again.
	if (_piano >= 0 && room.getMode(_piano) == 0 && _music) {
		_music = false;
		_engine.getMusic()->setGroup(3, 100);
	}

	// Artie clicked with the arrow: gagged, Velma takes it off; chained,
	// 360D2_, held; free, 1601_, 1701_ or 1801_.
	Character *artie = script.getCharacter("ARTIE");
	if (_artieClicked) {
		_artieClicked = false;
		if (_masked) {
			_ungag = true;
		} else if (_chained >= 1) {
			scene("Saloon.360D2_", true);
		} else {
			static const char *const kLines[] = { "Saloon.1601_", "Saloon.1701_", "Saloon.1801_" };
			scene(kLines[_engine.getRandomNumber(2)], false);
		}
	}

	// Artie idle with his clue given: he gives (GIVES), input held; idle
	// again, the thanks and the suspects.
	if (artie && artie->getState() == Character::kStateIdle) {
		if (_gives) {
			artie->act(room, sound, "GIVES");
			_gives = false;
			_thanks = true;
		}
		if (artie->getState() == Character::kStateIdle && _thanks) {
			_thanks = false;
			leave("ROOM_Suspect_Chooser", "Global.Thank_You_");
		}
	}

	// A key found: shown at depth 1 once its hotspot stops, with 360G_,
	// 360G2_ or 360G3_.
	for (uint k = 0; k < kKeys; k++) {
		if (!_keyFound[k] || _hotspots[k] < 0 || room.getMode(_hotspots[k]) != 0)
			continue;
		_keyFound[k] = false;
		_keyVisible[k] = true;
		if (_keys[k] >= 0) {
			room.setHidden(_keys[k], false);
			room.setDepth(_keys[k], 1);
		}
		static const char *const kLines[] = { "Saloon.360G_", "Saloon.360G2_", "Saloon.360G3_" };
		scene(kLines[_engine.getRandomNumber(2)], false);
	}

	// The gag: with no scene playing and the opening done, Velma, idle,
	// plays OBJ_VELMA_P12_UNGAG_ARTIE. On its step 6 her line plays and the
	// idle timer starts again; on its step 33 Artie is idle (ungagged) and
	// the moans end; Velma idle again, hotspots 3-8 get the open hand, and
	// GAG_GONE_, held.
	if (_ungag) {
		Character *velma = script.getCharacter("VELMA");
		if (!_velmaStarted && velma && velma->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
			velma->special(room, sound, "OBJ_VELMA_P12_UNGAG_ARTIE");
			_velmaStarted = true;
		}
		if (_velmaStarted && velma) {
			const int step = velma->getSprite() >= 0 ? (int)room.getStep(velma->getSprite()) : -1;
			if (step == 6 && !_velmaSpoke) {
				playSound("Saloon.360D_1_VELMA");
				_velmaSpoke = true;
				script.restartIdle();
			}
			if (step == 33 && artie && artie->getState() != Character::kStateIdle) {
				artie->setState(room, sound, Character::kStateIdle);
				_engine.setGameFlag("MoaningActive", false);
			}
			if (velma->getState() == Character::kStateIdle) {
				_masked = false;
				for (uint k = 0; k < kKeys; k++) {
					if (_hotspots[k] >= 0)
						room.setRollover(_hotspots[k], kOpenHand);
				}
				_velmaStarted = false;
				_ungag = false;
				scene("Saloon.GAG_GONE_", true);
			}
		}
	}

	// Shaggy clicked with the nickel: the bull with it, and the arrow.
	if (_shaggyNickel) {
		_shaggyNickel = false;
		event(kBull, true);
		inventory.setCursor(kArrow);
	}
}

void SaloonRoom::saveKeys(Common::INIFile &profile) {
	// 0x416000: the base keys, then these, in the room's section; the keys'
	// only once Artie is ungagged.
	profile.setKey("ArtieMasked", kSaloon, _masked ? "Yes" : "No");
	profile.setKey("ArtieChained", kSaloon, Common::String::format("%i", _chained));
	profile.setKey("ShaggyPiano", kSaloon, _shaggyPiano ? "Yes" : "No");
	profile.setKey("BullRidden", kSaloon, _bullRidden ? "Yes" : "No");
	for (uint k = 0; k < kKeys; k++)
		profile.setKey(Common::String::format("ClickableOpen%u", k), kSaloon, _open[k] ? "Yes" : "No");
	if (_masked)
		return;
	for (uint k = 0; k < kKeys; k++) {
		profile.setKey(Common::String::format("KeyHidden%u", k), kSaloon, _keyHidden[k] ? "Yes" : "No");
		profile.setKey(Common::String::format("KeyVisible%u", k), kSaloon, _keyVisible[k] ? "Yes" : "No");
	}
}

void SaloonRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4161D0 -> 0x40C560: a missing flag reads as false; ArtieChained
	// through atoi (0x40C670). The keys' only once Artie is ungagged.
	Common::String v;
	auto flag = [&](const Common::String &key) { return profile.getKey(key, kSaloon, v) && v == "Yes"; };
	_masked = flag("ArtieMasked");
	_chained = profile.getKey("ArtieChained", kSaloon, v) ? atoi(v.c_str()) : 0;
	_shaggyPiano = flag("ShaggyPiano");
	_bullRidden = flag("BullRidden");
	for (uint k = 0; k < kKeys; k++)
		_open[k] = flag(Common::String::format("ClickableOpen%u", k));
	if (_masked)
		return;
	for (uint k = 0; k < kKeys; k++) {
		_keyHidden[k] = flag(Common::String::format("KeyHidden%u", k));
		_keyVisible[k] = flag(Common::String::format("KeyVisible%u", k));
	}
}

// ---------------------------------------------------------------------------
// Artie's chains

static const char *chainsAnim(int locked) {
	if (locked == 3)
		return "ANIM_CHAINSA";
	if (locked == 2)
		return "ANIM_CHAINSB";
	if (locked == 1)
		return "ANIM_CHAINSC";
	return nullptr;
}

void ArtieRoom::reset() {
	// 0x40D540, after the base reset.
	_chainsLocked = 3;
}

void ArtieRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40D6D0: the chains a sprite under layer 0 (game +0x58) at depth 0,
	// on CHAINSA, B or C by ChainsLocked (3, 2, 1), stopped on step 0.
	if (object != "OBJ_CHAINS")
		return;
	claim.build = Room::Claim::kSprite;
	claim.layer = 0;
	claim.depth = 0;
	if (const char *anim = chainsAnim(_chainsLocked))
		claim.anim = anim;
}

void ArtieRoom::enter(const Common::String &from) {
	// 0x40D680, after the base entry: input held and the cursor faded, the
	// toolbar off (0x407CC0, in the toolbar's hidden list), and a second to
	// wait.
	Room &room = *_engine.getRoom();
	_lastSound.clear();
	_countdown = 36;
	_chains = findSprite(room, "OBJ_CHAINS");
	if (_chains >= 0) {
		if (const char *anim = chainsAnim(_chainsLocked))
			room.startAnim(_chains, anim, 0, -1);
	}
}

void ArtieRoom::tick() {
	// 0x40D560, after the base tick. The chains stopped, a second on: on
	// step 0 they play once (graphics_AnimSprite_Start 3, 3, -1) with
	// Artie.Chains_A, B or C, and another second; played, a chain fewer, and
	// the saloon again, through movie 6 once the last is off.
	Room &room = *_engine.getRoom();
	if (_chains < 0 || room.getMode(_chains) != 0 || --_countdown != 0)
		return;
	if (room.getStep(_chains) == 0) {
		room.setPlayer(_chains, 3, -1);
		static const char *const kSounds[] = { "Artie.Chains_C", "Artie.Chains_B", "Artie.Chains_A" };
		if (_chainsLocked >= 1 && _chainsLocked <= 3) {
			const char *name = kSounds[_chainsLocked - 1];
			const bool found = _engine.getSound()->play(name);
			_lastSound = Common::String::format("%s found %d", name, found ? 1 : 0);
		}
		_countdown = 36;
		return;
	}
	if (--_chainsLocked == 0)
		_engine.requestRoom(kSaloon, 6);
	else
		_engine.requestRoom(kSaloon);
}

void ArtieRoom::saveKeys(Common::INIFile &profile) {
	// 0x40D820: the base keys, then ChainsLocked, in the room's section.
	profile.setKey("ChainsLocked", kArtieRoom, Common::String::format("%i", _chainsLocked));
}

void ArtieRoom::loadKeys(const Common::INIFile &profile) {
	// 0x40D860 -> 0x40C670: atoi.
	Common::String v;
	_chainsLocked = profile.getKey("ChainsLocked", kArtieRoom, v) ? atoi(v.c_str()) : 0;
}

RoomClass *createSaloonRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kSaloon)
		return new SaloonRoom(engine);
	if (room == kArtieRoom)
		return new ArtieRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
