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
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/vault_rooms.h"

namespace Scooby {

static const char *const kVault = "ROOM_P16_Bank_Vault";
static const char *const kOpenSafe = "ROOM_Open_Safe";
static const char *const kBackAlley = "ROOM_P19_Back_Alley";
static const char *const kCombo = "OBJ_INV_SAFE_COMBO";
static const char *const kKey = "OBJ_INV_STRONGBOXKEY";
static const char *const kDoor = "OBJ_P16_DOOR";
static const char *const kDust = "OBJ_P16_BANK_VAULT_HOTSPOT6";
static const char *const kDustDone = "ANIM_P16_BANK_VAULT_HOTSPOT6C";
static const char *const kDoorOpen = "ANIM_P16_DOOROPEN";
static const char *const kBoxPrefix = "OBJ_P16_BANK_VAULT_HOTSPOT";

// Sequence modes (0x430AA0): 0 stopped, 5 stopped on the last step.
static const int kModeStopped = 0;
static const int kModeLast = 5;

static int findObject(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

// Hotspots 1-5, the strong boxes.
static bool isBox(const Common::String &object) {
	return object.size() == strlen(kBoxPrefix) + 1 && object.hasPrefix(kBoxPrefix) && object.lastChar() >= '1' && object.lastChar() <= '5';
}

// ---------------------------------------------------------------------------
// The vault

bool BankVaultRoom::used(const char *item) const {
	const Inventory &inventory = *_engine.getInventory();
	if (!inventory.hasItem(item))
		return false;
	for (uint i = 0; i < 4; i++) {
		if (inventory.getSlot(i) == item)
			return false;
	}
	return true;
}

void BankVaultRoom::scene(const char *name, bool block) {
	// 0x40B400: only if no scene is playing.
	if (_engine.getScript()->startScene(name, block))
		_lastScene = name;
}

void BankVaultRoom::reset() {
	// 0x429280, after the base reset.
	_dustVisible = false;
	_boxLine = 0;
}

void BankVaultRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4295E0. The safe's door is a requires object under layer 0
	// (0x408C80 with -1) at depth 1; enter() shows it open once the
	// combination is used. The strong boxes aren't built once the key is
	// used. The dust is the default clickable; enter() sets it.
	if (object == kDoor) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 1;
	} else if (isBox(object) && used(kKey)) {
		claim.build = Room::Claim::kSkip;
	}
}

void BankVaultRoom::enter(const Common::String &from) {
	// 0x4292A0, after the base entry; with the claims' finishing touches.
	Room &room = *_engine.getRoom();
	Inventory &inventory = *_engine.getInventory();
	_doorClicked = false;
	_daphneDone = false;
	_lastScene.clear();
	_lastSound.clear();
	_door = findObject(room, kDoor);
	_dust = findObject(room, kDust);
	if (_door >= 0 && used(kCombo))
		room.startAnim(_door, kDoorOpen, kModeLast, -1);
	if (_dust >= 0 && _dustVisible) {
		room.startAnim(_dust, kDustDone, kModeStopped, -1);
		_engine.getScript()->setClickable(_dust, kDustDone);
	}
	// The key used (in the strong box), the dentures, once.
	if (used(kKey) && !inventory.hasItem("OBJ_INV_DENTURES"))
		inventory.addItem("OBJ_INV_DENTURES", "ANIM_TOOLBARDENTURES", "ANIM_CURSORDENTURES");
}

void BankVaultRoom::event(const Common::String &object, bool success) {
	// 0x429350
	Inventory &inventory = *_engine.getInventory();
	RoomScript &script = *_engine.getScript();
	if (object == kDoor) {
		// The safe's door: with the combination it is used. Either way,
		// Daphne has a go at it, input held.
		if (success) {
			inventory.removeItem(kCombo);
			inventory.setCursor("ANIM_CURSORARROW");
		}
		_doorClicked = true;
		debugC(1, kDebugScript, "vault: the safe clicked%s at tick %u", success ? " with the combination" : "", script.getTick());
		return;
	}
	if (isBox(object)) {
		if (!success) {
			// With the arrow: one line, then the other.
			if (_boxLine == 0) {
				_boxLine = 1;
				scene("Bank_Vault.340A_", true);
			} else {
				_boxLine = 0;
				scene("Bank_Vault.340A2_", true);
			}
			return;
		}
		// With the key: the right box (StrongBox, game +0x28C) plays its
		// movie (38 for box 1) and the vault comes back; the key is used.
		const int box = object.lastChar() - '1';
		if (box == _engine.getGameInt("StrongBox")) {
			const int movie = object.lastChar() - 0xB;
			debugC(1, kDebugScript, "vault: strong box %d opened, movie %d", box + 1, movie);
			_engine.requestRoom(kVault, movie);
			inventory.removeItem(kKey);
			inventory.setCursor("ANIM_CURSORARROW");
			return;
		}
		static const char *const kWrong[] = { "Bank_Vault.340B_", "Bank_Vault.340B2_", "Bank_Vault.340B3_" };
		scene(kWrong[_engine.getRandomNumber(2)], true);
		return;
	}
	if (object == kDust && _dust >= 0) {
		// The dust: once its next state is C, it shows; clicked then, its
		// scene, and it stays on C.
		if (!_dustVisible) {
			if (script.getClickableState(_dust) == kDustDone)
				_dustVisible = true;
			return;
		}
		scene("Bank_Vault.340X_", false);
		_engine.getRoom()->startAnim(_dust, kDustDone, kModeStopped, -1);
		script.setClickable(_dust, kDustDone);
	}
}

void BankVaultRoom::tick() {
	// 0x429870, after the base tick: the safe clicked, Daphne tries it when
	// idle and nothing plays, input held meanwhile.
	if (!_doorClicked)
		return;
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Character *daphne = script.getCharacter("DAPHNE");
	const bool open = used(kCombo);
	if (!_daphneDone && daphne && daphne->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		if (!open) {
			daphne->special(room, sound, "OBJ_DAPHNE_P16_NO_UNLOCK_SAFE");
			_lastSound = "Daphne.No_Open_Safe";
		} else {
			daphne->special(room, sound, "OBJ_DAPHNE_P16_UNLOCK_SAFE");
			_lastSound = "Daphne.Opens_Safe";
			if (_door >= 0)
				room.setHidden(_door, true);
		}
		sound.play(_lastSound);
		_daphneDone = true;
		debugC(1, kDebugScript, "vault: Daphne tries the safe at tick %u: %s", script.getTick(), _lastSound.c_str());
	}
	if (_daphneDone && daphne && daphne->getState() == Character::kStateIdle) {
		if (!open) {
			// Input stays held until the scene is over.
			scene("Bank_Vault.340E_", true);
		} else {
			// The door open, and through it to the open safe.
			if (_door >= 0) {
				room.startAnim(_door, kDoorOpen, kModeStopped, -1);
				room.setHidden(_door, false);
			}
			debugC(1, kDebugScript, "vault: the safe open at tick %u; to %s", script.getTick(), kOpenSafe);
			_engine.exitTo(kOpenSafe);
		}
		_doorClicked = false;
		_daphneDone = false;
	}
}

void BankVaultRoom::saveKeys(Common::INIFile &profile) {
	// 0x4297F0: the base keys, then DustVisible, in the room's section.
	profile.setKey("DustVisible", kVault, _dustVisible ? "Yes" : "No");
}

void BankVaultRoom::loadKeys(const Common::INIFile &profile) {
	// 0x429830 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_dustVisible = profile.getKey("DustVisible", kVault, v) && v == "Yes";
}

// ---------------------------------------------------------------------------
// The open safe

// The speakers a line can have (0x46A720, 20 names): the first with a clip
// <scene><line>_<NAME> speaks it.
static const char *const kSpeakers[] = {
	"NARRATOR", "DAPHNE", "FRED", "SCOOBY", "SHAGGY", "VELMA", "ARTIE", "JENNY",
	"SHERIFF", "ICDOUBLE", "GUMMY", "DONKEY", "BRAD", "EWAN", "MARIA", "JANE",
	"WIZARD", "JESTER", "PRINCESS", "UNICYCLE"
};
// The gang's sprites (+0xC0 to +0xD0), by name.
static const char *const kGangNames[OpenSafeRoom::kGang] = { "DAPHNE", "FRED", "VELMA", "SHAGGY", "SCOOBY" };

void OpenSafeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42A040: each of the gang a sprite under layer 1 (game +0x5C), depth
	// 1, looping at 3 ticks a step from step 0, hit by its cels.
	for (uint i = 0; i < kGang; i++) {
		if (object == Common::String("OBJ_GANG_VAULT_") + kGangNames[i]) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 1;
			claim.depth = 1;
			claim.hitType = 1;
			return;
		}
	}
}

void OpenSafeRoom::enter(const Common::String &from) {
	// 0x429C40: seen before, straight on to the back alley. Then, after the
	// base entry, the toolbar slides off (0x407CC0, in the toolbar's rooms),
	// and the scene starts after 72 ticks, input held (0x40B400).
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	if (script.getMemory(room.getName()).openingPlayed) {
		debugC(1, kDebugScript, "open safe: seen; to %s", kBackAlley);
		_engine.requestRoom(kBackAlley);
	}
	for (uint i = 0; i < kGang; i++)
		_gang[i] = findObject(room, (Common::String("OBJ_GANG_VAULT_") + kGangNames[i]).c_str());
	_clip.clear();
	_mouth = -1;
	_countdown = 72;
	_line = 0;
	_skipped = false;
	_scene = "Open_Safe.340F_";
	_engine.getSound()->fadeMusic(90, 2, false);
}

void OpenSafeRoom::nextLine() {
	// 0x429CB0: once the countdown is out, with the scene on and no line
	// playing, the next line: the first speaker with a clip. With none, the
	// scene is over.
	const int old = _countdown--;
	if (old >= 1 || _scene.empty() || !_clip.empty())
		return;
	_countdown = 0;
	_line++;
	Sound &sound = *_engine.getSound();
	for (uint i = 0; i < ARRAYSIZE(kSpeakers); i++) {
		const Common::String clip = Common::String::format("%s%d_%s", _scene.c_str(), _line, kSpeakers[i]);
		if (!sound.findClip(clip))
			continue;
		// The mouth: Daphne, Fred, Scooby, Velma, or else Shaggy's.
		const Common::String who = kSpeakers[i];
		_mouth = who == "DAPHNE" ? _gang[0] : who == "FRED" ? _gang[1] : who == "SCOOBY" ? _gang[4] : who == "VELMA" ? _gang[2] : _gang[3];
		_clip = clip;
		sound.play(_clip);
		debugC(1, kDebugScript, "open safe: line %d, %s at tick %u", _line, _clip.c_str(), _engine.getScript()->getTick());
		return;
	}
	debugC(1, kDebugScript, "open safe: over after %d lines at tick %u", _line - 1, _engine.getScript()->getTick());
	_scene.clear();
}

bool OpenSafeRoom::skip() {
	// Input slot 5 in 0x429CB0: the line stops and the scene is over.
	if (_scene.empty())
		return false;
	if (!_clip.empty())
		_engine.getSound()->stopClip(_clip);
	_scene.clear();
	_skipped = true;
	return true;
}

void OpenSafeRoom::tick() {
	// 0x429F10: the base tick (and with it +0x38, nextLine()), then the
	// mouths: each of the gang on step 0, the speaker's on the lip sync's
	// shape - 1. A line over frees the next. The scene over: played, and the
	// back alley.
	nextLine();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	for (uint i = 0; i < kGang; i++) {
		if (_gang[i] >= 0)
			room.setPlayer(_gang[i], kModeStopped, 0);
	}
	uint shape = 0;
	if (!_clip.empty() && sound.getLipShape(_clip, shape)) {
		if (shape != 0 && _mouth >= 0)
			room.setPlayer(_mouth, kModeStopped, shape - 1);
	} else if (!_clip.empty() && !sound.isClipPlaying(_clip)) {
		_clip.clear();
	}
	if (_scene.empty()) {
		RoomScript &script = *_engine.getScript();
		script.getMemory(room.getName()).openingPlayed = true;
		_engine.requestRoom(kBackAlley);
	}
}

RoomClass *createVaultRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kVault)
		return new BankVaultRoom(engine);
	if (room == kOpenSafe)
		return new OpenSafeRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
