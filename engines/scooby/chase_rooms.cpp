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
#include "scooby/chase_rooms.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

// Sequence modes (0x430AA0).
static const int kModeOnce = 3;

static const char *const kOilCan = "OBJ_INV_OILCAN";
static const char *const kCorral = "ROOM_P30_Horseshoe_Corral";
static const char *const kCloseup = "ROOM_P25A_Chase_B2_Closeup";

// The movie the corral's run-off plays (game +0x08, push 8), and the one
// before the chase starts again from C5 and C6 (push 0x1F).
static const int kCorralMovie = 8;
static const int kRestartMovie = 31;

static int findPrefixed(const Room &room, const char *prefix) {
	// The placement claim keeps the last sprite it builds.
	int found = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s).hasPrefix(prefix))
			found = s;
	}
	return found;
}

// ---------------------------------------------------------------------------
// Chase B

void ChaseBRoom::reset() {
	// 0x41E780, after the base reset (0x40CD20).
	_countdown = 720;
}

void ChaseBRoom::arrive(const Common::String &from) {
	// 0x41FA80 (room vtable +0x2C): this visit's opening, not played yet.
	RoomScript &script = *_engine.getScript();
	const Common::String &room = _engine.getRoom()->getName();
	if (room == "ROOM_P26_Chase_B3") {
		script.setOpening("Chase_Branch_3.230_");
	} else if (room == "ROOM_P27_Chase_B4" || room == "ROOM_P24_Chase_B1" || from == kCorral) {
		script.setOpening("");
	} else {
		// rand() & 3
		static const char *const kOpenings[] = {
			"Chase_Branch_3.230_", "Chase_Branch_3.230B2_", "Chase_Branch_3.230C_", "Chase_Branch_3.230D_"
		};
		script.setOpening(kOpenings[_engine.getRandomNumber(3)]);
	}
	debugC(1, kDebugScript, "%s: opening '%s'", room.c_str(), script.getOpening().c_str());
}

void ChaseBRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41ED40
	if (object.hasPrefix("OBJ_BRANCH")) {
		// The run-off's sprite, under layer 0 at depth 0, looping.
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == kOilCan) {
		// A requires object under layer 0 (0x408C80 with -1), unless the
		// oil can was ever taken.
		if (_engine.getInventory()->hasItem(kOilCan)) {
			claim.build = Room::Claim::kSkip;
		} else {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		}
	} else if (object == "OBJ_P25_OVERLAY") {
		// Under layer 0 at depth -1, while the safe's combination is still
		// to get and ICDouble has given his clue (0x40A870, game +0xD8).
		if (!_engine.getInventory()->hasItem("OBJ_INV_SAFE_COMBO") && _engine.isClueOwner("ICDOUBLE")) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = -1;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	}
}

void ChaseBRoom::enter(const Common::String &from) {
	// 0x41F650, after the base entry.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	_countdown = 720;
	_pending.clear();
	_lastSound.clear();
	_branch = findPrefixed(room, "OBJ_BRANCH");
	_oilCanSprite = findPrefixed(room, kOilCan);

	// B4 and B1 run straight on.
	if (room.getName() == "ROOM_P27_Chase_B4")
		leave("ROOM_P35_Chase_C6");
	if (room.getName() == "ROOM_P24_Chase_B1")
		leave("ROOM_P39_Chase_C4");

	// Back from the close-up or the corral: the rest of that run-off.
	if (from == kCloseup || from == kCorral) {
		const bool closeup = from == kCloseup;
		_lastSound = closeup ? "Chase.Branch_2" : "Chase.Branch_5_from_HC";
		sound.play(_lastSound);
		if (_branch >= 0)
			room.startAnim(_branch, closeup ? "ANIM_BRANCH2A_FIN" : "ANIM_BRANCH5A_FIN", kModeOnce, -1);
		_pending = closeup ? "ROOM_P38_Chase_C3" : "ROOM_P35_Chase_C6";
		debugC(1, kDebugScript, "%s: back from %s; on to %s", room.getName().c_str(), from.c_str(), _pending.c_str());
	}
	_oilCan = false;

	// ICDouble is never hit here (+0x4 = 0).
	Character *icdouble = _engine.getScript()->getCharacter("ICDOUBLE");
	if (icdouble)
		room.setHitType(icdouble->getSprite(), 0);
}

void ChaseBRoom::leave(const Common::String &destination) {
	// 0x41E7A0: once only. The run-off for the room it leads to, and for B4
	// and C5 and C6, the branch the gang is at; input stops until it has
	// played out. A destination with no run-off still holds the input.
	if (!_pending.empty())
		return;
	_pending = destination;

	struct RunOff {
		const char *destination;
		const char *branch; // the branch sprite's object, if it matters
		int oilCan;         // the oil-can flag, if it matters (-1)
		const char *anim;
		const char *sound;
	};
	static const RunOff kRunOffs[] = {
		{ "ROOM_P25_Chase_B2", nullptr, -1, "ANIM_BRANCH3B", "Chase.Branch_3b" },
		{ "ROOM_P38_Chase_C3", nullptr, -1, "ANIM_BRANCH2B", "Chase.Branch_2" },
		{ kCloseup, nullptr, -1, "ANIM_BRANCH2A", "Chase.Branch_2" },
		{ "ROOM_P37_Chase_C2", nullptr, -1, "ANIM_BRANCH3A", "Chase.Branch_3a" },
		{ "ROOM_P39_Chase_C4", nullptr, -1, "ANIM_BRANCH1A", "Chase.Branch_1a" },
		{ kCorral, nullptr, -1, "ANIM_BRANCH5A", "Chase.Branch_5a" },
		{ "ROOM_P35_Chase_C6", "OBJ_BRANCH5", -1, "ANIM_BRANCH5B", "Chase.Branch_5b" },
		{ "ROOM_P34_Chase_C5", nullptr, 1, "ANIM_BRANCH6C", "Chase.Branch_6c" },
		{ "ROOM_P34_Chase_C5", nullptr, 0, "ANIM_BRANCH6A", "Chase.Branch_6a" },
		{ "ROOM_P27_Chase_B4", "OBJ_BRANCH6", -1, "ANIM_BRANCH6B", "Chase.Branch_6b" },
		{ "ROOM_P35_Chase_C6", "OBJ_BRANCH4", -1, "ANIM_BRANCH4A", "Chase.Branch_4" }
	};
	Room &room = *_engine.getRoom();
	const Common::String branch = _branch >= 0 ? room.getObjectName(_branch) : Common::String();
	for (uint i = 0; i < ARRAYSIZE(kRunOffs); i++) {
		const RunOff &r = kRunOffs[i];
		if (destination != r.destination || (r.branch && branch != r.branch) || (r.oilCan >= 0 && (int)_oilCan != r.oilCan))
			continue;
		if (_branch >= 0)
			room.startAnim(_branch, r.anim, kModeOnce, -1);
		_lastSound = r.sound;
		_engine.getSound()->play(_lastSound);
		debugC(1, kDebugScript, "%s: run-off to %s: %s, %s", room.getName().c_str(), destination.c_str(), r.anim, r.sound);
		return;
	}
	debugC(1, kDebugScript, "%s: to %s, with no run-off for %s", room.getName().c_str(), destination.c_str(), branch.c_str());
}

bool ChaseBRoom::leaveBy(const Common::String &exitObject) {
	// 0x41E7A0 replaces the default handler: the exit's scene and walk-off
	// never play.
	leave(_engine.getScript()->readExit(exitObject).destination);
	return true;
}

void ChaseBRoom::event(const Common::String &object, bool success) {
	// 0x41EBC0: taking the oil can sends the gang on to C5, the long way.
	if (object == kOilCan) {
		_oilCan = true;
		leave("ROOM_P34_Chase_C5");
	}
}

void ChaseBRoom::tick() {
	// 0x41EFC0, after the base tick (0x40B490).
	Room &room = *_engine.getRoom();
	const int old = _countdown--;
	if (old < 0 && _pending.empty()) {
		// The gang runs on by itself, from the rooms that have a way to.
		_countdown = 2160;
		static const char *const kTimeouts[][2] = {
			{ "ROOM_P25_Chase_B2", "ROOM_P38_Chase_C3" },
			{ "ROOM_P26_Chase_B3", "ROOM_P37_Chase_C2" },
			{ "ROOM_P28_Chase_B5", "ROOM_P35_Chase_C6" },
			{ "ROOM_P29_Chase_B6", "ROOM_P34_Chase_C5" }
		};
		for (uint i = 0; i < ARRAYSIZE(kTimeouts); i++) {
			if (room.getName() == kTimeouts[i][0]) {
				debugC(1, kDebugScript, "%s: time is up at tick %u", room.getName().c_str(), _engine.getScript()->getTick());
				leave(kTimeouts[i][1]);
				break;
			}
		}
	}
	if (_pending.empty())
		return;

	// Step 22 of the oil can's run-off: the oil can goes in the toolbar.
	Inventory &inventory = *_engine.getInventory();
	if (_oilCan && _branch >= 0 && room.getStep(_branch) == 22 && !inventory.hasItem(kOilCan)) {
		if (_oilCanSprite >= 0)
			room.setHidden(_oilCanSprite, true);
		inventory.addItem(kOilCan, "ANIM_TOOLBAROILCAN", "ANIM_CURSOROILCAN");
		debugC(1, kDebugScript, "%s: the oil can is in the toolbar at tick %u", room.getName().c_str(), _engine.getScript()->getTick());
	}
	// The run-off has played out: the next room, through a movie to the corral.
	if (_branch >= 0 && room.getMode(_branch) == 0)
		_engine.requestRoom(_pending, _pending == kCorral ? kCorralMovie : -1);
}

// ---------------------------------------------------------------------------
// Chase C

void ChaseCRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x420690: the canned run-off, under layer 0, moved to depth -1, and
	// started once through (graphics_AnimSprite_Start 3, 3, -1) in enter().
	if (object.hasPrefix("OBJ_CANNED")) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -1;
	}
}

void ChaseCRoom::enter(const Common::String &from) {
	// 0x420510, after the base entry: input held, the cursor hidden, and
	// the run-off's sound.
	Room &room = *_engine.getRoom();
	_leaving = false;
	_lastSound.clear();
	_canned = findPrefixed(room, "OBJ_CANNED");
	if (_canned >= 0)
		room.startAnim(_canned, room.getAnim(_canned), kModeOnce, -1);
	static const char *const kRooms[] = {
		"ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3",
		"ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6"
	};
	for (uint i = 0; i < ARRAYSIZE(kRooms); i++) {
		if (room.getName() == kRooms[i]) {
			_lastSound = Common::String::format("Chase.Canned_%u", i + 1);
			_engine.getSound()->play(_lastSound);
			debugC(1, kDebugScript, "%s: %s", room.getName().c_str(), _lastSound.c_str());
			break;
		}
	}
}

void ChaseCRoom::tick() {
	// 0x41FC10, after the base tick: when the run-off has played out, the
	// default exit handler (0x40C1E0) to the next branch, or back to the
	// chase's start through movie 31.
	Room &room = *_engine.getRoom();
	if (_leaving || _canned < 0 || room.getMode(_canned) != 0)
		return;
	static const struct {
		const char *room;
		const char *destination;
		int movie;
	} kExits[] = {
		{ "ROOM_P33_Chase_C1", "ROOM_P26_Chase_B3", -1 },
		{ "ROOM_P37_Chase_C2", "ROOM_P24_Chase_B1", -1 },
		{ "ROOM_P38_Chase_C3", "ROOM_P28_Chase_B5", -1 },
		{ "ROOM_P39_Chase_C4", "ROOM_P29_Chase_B6", -1 },
		{ "ROOM_P34_Chase_C5", "ROOM_P40_Chase_Start", kRestartMovie },
		{ "ROOM_P35_Chase_C6", "ROOM_P40_Chase_Start", kRestartMovie }
	};
	for (uint i = 0; i < ARRAYSIZE(kExits); i++) {
		if (room.getName() == kExits[i].room) {
			_leaving = true;
			debugC(1, kDebugScript, "%s: run-off over at tick %u; to %s, movie %d", room.getName().c_str(),
			       _engine.getScript()->getTick(), kExits[i].destination, kExits[i].movie);
			_engine.exitTo(kExits[i].destination, kExits[i].movie);
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// The close-up

static const char *const kIcDouble = "ICDOUBLE";
static const char *const kCombo = "OBJ_INV_SAFE_COMBO";
// The combination's run of the sprite: step 38 is where it passes Shaggy.
static const uint kComboPassStep = 38;
static const int kComboDepth = 500;
static const int kModeLast = 5;

void CloseupRoom::reset() {
	// 0x411A50, after the base reset (0x40CD20).
	_gives = false;
	_thanks = false;
	_blownIn = false;
}

Common::String CloseupRoom::getArtifactRoom() const {
	// 0x412490: ROOM_Artifact_<ArtifactType>_, then the level's size.
	const int level = _engine.getDifficulty();
	return Common::String::format("ROOM_Artifact_%s_%s", _engine.getGameString("ArtifactType").c_str(),
	                              level == 0 ? "09" : level == 1 ? "12" : "15");
}

void CloseupRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x412300: the combination, once ICDouble's clue is given and until it is
	// held, is a requires object under layer 0 at depth 500, stopped on its
	// last step (graphics_AnimSprite_Start 5, in enter()), and hidden until
	// it has blown in.
	if (object != kCombo)
		return;
	if (_engine.getInventory()->hasItem(kCombo)) {
		claim.build = Room::Claim::kSkip;
		return;
	}
	claim.build = Room::Claim::kRequires;
	claim.layer = 0;
	claim.depth = kComboDepth;
	claim.hidden = !_blownIn;
}

void CloseupRoom::enter(const Common::String &from) {
	// 0x412020, after the base entry.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	_lastScene.clear();
	_lastSound.clear();
	_comboScene = false;
	_held = false;
	_combo = findPrefixed(room, kCombo);
	if (_combo >= 0)
		room.startAnim(_combo, room.getAnim(_combo), kModeLast, -1);

	// With the artifact built and no clue from him yet: a 260C scene
	// (rand() % 3), and his clue; he gives it when next idle.
	_owner = _engine.isClueOwner(kIcDouble);
	if (!_owner && _engine.getGameInt("ArtifactsBuilt") == 1) {
		static const char *const kScenes[] = {
			"Chase_B2_Closeup.260C1_", "Chase_B2_Closeup.260C2_", "Chase_B2_Closeup.260C3_"
		};
		_lastScene = kScenes[_engine.getRandomNumber(2)];
		script.startScene(_lastScene, true);
		_engine.giveClue(kIcDouble);
		_gives = true;
		debugC(1, kDebugScript, "close-up: %s, and ICDouble's clue", _lastScene.c_str());
	}
	_comboSceneDone = false;
	_taking = false;
	_countdown = 24;

	// The first visit: he introduces himself on Big Head, then the artifact.
	if (!inventory.hasMetSuspect(kIcDouble)) {
		inventory.meetSuspect(kIcDouble);
		const Common::String artifact = getArtifactRoom();
		_engine.startBigHead("Chase.Branch_2.260A_", "BIG_HEAD_BG_P25A", artifact);
		debugC(1, kDebugScript, "close-up: ICDouble's introduction, then %s", artifact.c_str());
		return;
	}
	// The combination is about to blow in: input waits for it.
	if (_combo >= 0 && !_blownIn && _owner)
		_held = true;
}

void CloseupRoom::roomLeft() {
	// 0x411A70, after the base leave: the combination's scene was over but
	// Shaggy hadn't taken it yet.
	Inventory &inventory = *_engine.getInventory();
	if (_comboSceneDone && !inventory.hasItem(kCombo))
		inventory.addItem(kCombo, "ANIM_TOOLBARSAFE_COMBO", "ANIM_CURSORSAFE_COMBO");
}

bool CloseupRoom::click(int sprite) {
	// 0x411AC0: a click on ICDouble with the arrow. Before the artifact is
	// built, back to it; after, one of his lines (rand() & 3).
	Character *icd = _engine.getScript()->getCharacter(kIcDouble);
	if (!icd || sprite < 0 || sprite != icd->getSprite() || !_engine.getInventory()->isDefaultCursor())
		return false;
	if (_engine.getGameInt("ArtifactsBuilt") < 1) {
		const Common::String artifact = getArtifactRoom();
		debugC(1, kDebugScript, "close-up: ICDouble clicked; to %s", artifact.c_str());
		_engine.requestRoom(artifact);
	} else {
		static const char *const kLines[] = {
			"Chase_B2_Closeup.1901_", "Chase_B2_Closeup.1902_", "Chase_B2_Closeup.1903_", "Chase_B2_Closeup.1904_"
		};
		const char *scene = kLines[_engine.getRandomNumber(3)];
		if (_engine.getScript()->startScene(scene, false))
			_lastScene = scene;
		debugC(1, kDebugScript, "close-up: ICDouble clicked; %s", scene);
	}
	return true;
}

void CloseupRoom::event(const Common::String &object, bool success) {
	// 0x411FC0: the combination clicked with the arrow: its scene. 0x40B400
	// is told to hold input, but the room's tick lets go of it that same
	// tick (see tick()), so the scene doesn't block.
	if (object != kCombo || !_engine.getInventory()->isDefaultCursor())
		return;
	if (_engine.getScript()->startScene("Chase.Branch_2.486_", false)) {
		_lastScene = "Chase.Branch_2.486_";
		_comboScene = true;
		_comboSceneDone = false;
		debugC(1, kDebugScript, "close-up: the combination clicked at tick %u", _engine.getScript()->getTick());
	}
}

void CloseupRoom::tick() {
	// 0x411AC0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	// The combination's scene's done flag (+0xC0, through +0x80).
	if (_comboScene && !script.isScenePlaying()) {
		_comboScene = false;
		_comboSceneDone = true;
	}

	// ICDouble gives when idle; then the gang thanks him and goes to the
	// suspects and clues.
	Character *icd = script.getCharacter(kIcDouble);
	if (icd && icd->getState() == Character::kStateIdle) {
		if (_gives) {
			icd->act(room, sound, "GIVES");
			_gives = false;
			_thanks = true;
			_held = true;
			debugC(1, kDebugScript, "close-up: ICDouble gives at tick %u", script.getTick());
		}
		if (icd->getState() == Character::kStateIdle && _thanks) {
			_thanks = false;
			RoomScript::Exit exit;
			exit.destination = "ROOM_Suspect_Chooser";
			exit.scene = "Global.Thank_You_";
			exit.movie = -1;
			script.takeExit(exit);
			debugC(1, kDebugScript, "close-up: thanks at tick %u, then the suspects and clues", script.getTick());
		}
	}

	// The combination's scene over: Shaggy takes it when free, and it goes
	// in the toolbar when he is idle again.
	Character *shaggy = script.getCharacter("SHAGGY");
	if (_comboSceneDone) {
		if (!_taking && shaggy && shaggy->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
			_taking = true;
			shaggy->special(room, sound, "OBJ_SHAGGY_P25A_TAKE_COMBO");
			if (_combo >= 0)
				room.setHidden(_combo, true);
			_held = true;
			debugC(1, kDebugScript, "close-up: Shaggy takes the combination at tick %u", script.getTick());
		}
		if (_taking && shaggy && shaggy->getState() == Character::kStateIdle) {
			_comboSceneDone = false;
			_taking = false;
			if (!inventory.hasItem(kCombo))
				inventory.addItem(kCombo, "ANIM_TOOLBARSAFE_COMBO", "ANIM_CURSORSAFE_COMBO");
			_held = false;
			debugC(1, kDebugScript, "close-up: the combination in the toolbar at tick %u", script.getTick());
		}
	}

	// The combination blows in, 25 ticks after entry, from behind Shaggy.
	if (_combo >= 0 && !_blownIn && _owner && _countdown-- < 1) {
		_blownIn = true;
		room.setHidden(_combo, false);
		room.startAnim(_combo, room.getAnim(_combo), kModeOnce, -1);
		if (shaggy && shaggy->getSprite() >= 0)
			room.setDepth(_combo, room.getDepth(shaggy->getSprite()) - 1);
		_lastSound = "ICDouble.Combo.Blows.In";
		sound.play(_lastSound);
		debugC(1, kDebugScript, "close-up: the combination blows in at tick %u", script.getTick());
	}
	// On step 38 it passes in front of him.
	if (_combo >= 0 && room.getMode(_combo) != 0 && room.getStep(_combo) == kComboPassStep && shaggy && shaggy->getSprite() >= 0)
		room.setDepth(_combo, room.getDepth(shaggy->getSprite()) + 1);
	// Once it is in, input is free, every tick: that ends the hold of the
	// combination's scene and of Shaggy taking it the tick they start.
	if (_combo >= 0 && _blownIn && room.getMode(_combo) == 0 && _owner)
		_held = false;
}

void CloseupRoom::saveKeys(Common::INIFile &profile) {
	// 0x412410: the base keys, then ComboBlewOn, in the room's section.
	profile.setKey("ComboBlewOn", kCloseup, _blownIn ? "Yes" : "No");
}

void CloseupRoom::loadKeys(const Common::INIFile &profile) {
	// 0x412450 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_blownIn = profile.getKey("ComboBlewOn", kCloseup, v) && v == "Yes";
}

RoomClass *createChaseRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0); Phantom has no chase.
	if (room == kCloseup)
		return new CloseupRoom(engine);
	static const char *const kBRooms[] = {
		"ROOM_P24_Chase_B1", "ROOM_P25_Chase_B2", "ROOM_P26_Chase_B3",
		"ROOM_P27_Chase_B4", "ROOM_P28_Chase_B5", "ROOM_P29_Chase_B6"
	};
	static const char *const kCRooms[] = {
		"ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3",
		"ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6"
	};
	for (uint i = 0; i < ARRAYSIZE(kBRooms); i++) {
		if (room == kBRooms[i])
			return new ChaseBRoom(engine);
	}
	for (uint i = 0; i < ARRAYSIZE(kCRooms); i++) {
		if (room == kCRooms[i])
			return new ChaseCRoom(engine);
	}
	return nullptr;
}

} // End of namespace Scooby
