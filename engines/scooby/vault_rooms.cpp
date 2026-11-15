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
#include "common/util.h"

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

// ---------------------------------------------------------------------------
// The bank

static const char *const kBank = "ROOM_P13_Bank";
static const char *const kClockRoom = "ROOM_Vault_Clock";
static const char *const kClock = "OBJ_INV_CLOCK";
static const char *const kBankDoor = "OBJ_P13_DOOR";
static const char *const kBankDoorOpen = "ANIM_P13_DOOROPEN";
static const int kModeOnce = 3;

// The times the scene can name, by level (rand() % 10), in 0x428710.
static const int8 kTimes[3][10][2] = {
	{ { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 6, 0 }, { 7, 0 }, { 8, 0 }, { 9, 0 }, { 10, 0 }, { 11, 0 } },
	{ { 1, 40 }, { 2, 50 }, { 3, 10 }, { 4, 20 }, { 5, 30 }, { 6, 40 }, { 7, 50 }, { 10, 10 }, { 11, 20 }, { 12, 30 } },
	{ { 1, 14 }, { 2, 26 }, { 3, 18 }, { 4, 8 }, { 5, 16 }, { 7, 49 }, { 8, 2 }, { 10, 23 }, { 11, 57 }, { 12, 31 } }
};
static const char *const kLevelNames[3] = { "Spooky", "Spookier", "Spooktacular" };

static bool clockUsed(const Inventory &inventory) {
	if (!inventory.hasItem(kClock))
		return false;
	for (uint i = 0; i < 4; i++) {
		if (inventory.getSlot(i) == kClock)
			return false;
	}
	return true;
}

void BankRoom::getTime(int level, uint r, int &hour, int &minute) {
	if (level < 0 || level > 2 || r > 9) {
		hour = minute = 0;
		return;
	}
	hour = kTimes[level][r][0];
	minute = kTimes[level][r][1];
}

void BankRoom::reset() {
	// 0x4286C0, after the base reset: the first moan a second in.
	_moanDelay = 36;
}

void BankRoom::prepare() {
	// 0x4286E0 (+0x30): Bank.330A_ on every visit while the story has moans.
	_engine.getScript()->setOpening(_engine.getGameFlag("MoaningActive") ? "Bank.330A_" : "");
}

void BankRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x428EC0: the vault door, a requires object (0x408C80 with -1: layer 0)
	// at depth 1. enter() shows it.
	if (object == kBankDoor) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 1;
	}
}

void BankRoom::enter(const Common::String &from) {
	// 0x428FE0, after the base entry, with the claim's door.
	Room &room = *_engine.getRoom();
	_lastScene.clear();
	_lastSound.clear();
	_opening = 0;
	_door = findObject(room, kBankDoor);
	// The vault open: its door on its last step. Not yet, with the clock on
	// it: the door's first step.
	if (_door >= 0) {
		if (_engine.getGameFlag("VaultOpen"))
			room.startAnim(_door, kBankDoorOpen, kModeLast, -1);
		else if (clockUsed(*_engine.getInventory()))
			room.startAnim(_door, kBankDoorOpen, kModeStopped, -1);
	}
	// Back from setting the clock: input held, and a second later the door
	// swings open.
	if (_engine.getGameFlag("VaultOpen") && from == kClockRoom) {
		_opening = 36;
		if (_door >= 0)
			room.startAnim(_door, kBankDoorOpen, kModeStopped, -1);
		debugC(1, kDebugScript, "bank: the vault opens in %d ticks", _opening);
	}
	_doorLine = 0;
	// The gang walking in: Fred from Street 3 (0x405F20), Daphne from the
	// vault (0x405E60).
	Character *c = nullptr;
	const char *direction = nullptr;
	if (from == "ROOM_P11_Street3") {
		c = _engine.getScript()->getCharacter("FRED");
		direction = "DL";
	} else if (from == kVault) {
		c = _engine.getScript()->getCharacter("DAPHNE");
		direction = "UR";
	}
	if (c)
		c->walkIn(room, *_engine.getSound(), direction);
}

void BankRoom::event(const Common::String &object, bool success) {
	// 0x428710: the vault door.
	if (object != kBankDoor)
		return;
	Inventory &inventory = *_engine.getInventory();
	RoomScript &script = *_engine.getScript();
	if (!success) {
		// With the arrow, the clock on the door: to its dial.
		if (clockUsed(inventory) && inventory.isDefaultCursor()) {
			debugC(1, kDebugScript, "bank: the door clicked; to %s", kClockRoom);
			_engine.exitTo(kClockRoom);
			return;
		}
		// Else three lines in turn; the third is 330C_ with the clock in
		// the toolbar.
		const char *scene;
		if (_doorLine == 0) {
			_doorLine = 1;
			scene = "Bank.330B_";
		} else if (_doorLine == 1) {
			_doorLine = 2;
			scene = "Bank.330B2_";
		} else {
			_doorLine = 0;
			bool held = false;
			for (uint i = 0; i < 4; i++)
				held = held || inventory.getSlot(i) == kClock;
			scene = held ? "Bank.330C_" : "Bank.330B3_";
		}
		if (script.startScene(scene, true))
			_lastScene = scene;
		return;
	}
	// The clock put on the door: a time by level (rand() % 10), stored as
	// ClockHour and ClockMinute (game +0x284, +0x288), and the scene that
	// names it on the way to the dial.
	inventory.removeItem(kClock);
	inventory.setCursor("ANIM_CURSORARROW");
	const uint r = _engine.getRandomNumber(9);
	int level = _engine.getDifficulty();
	if (level < 0 || level > 2)
		level = 0;
	int hour, minute;
	getTime(level, r, hour, minute);
	_engine.setGameInt("ClockHour", hour);
	_engine.setGameInt("ClockMinute", minute);
	RoomScript::Exit exit;
	exit.destination = kClockRoom;
	exit.scene = Common::String::format("Bank_Vault.Clock_%s_%.2d%.2d_", kLevelNames[level], hour, minute);
	exit.movie = -1;
	_lastScene = exit.scene;
	debugC(1, kDebugScript, "bank: the clock on the door; %d:%.2d, %s", hour, minute, exit.scene.c_str());
	script.takeExit(exit);
}

void BankRoom::tick() {
	// 0x4290D0, after the base tick: the door's countdown, then the moans.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	if (_opening != 0 && --_opening == 0) {
		if (script.startScene("Bank.330D_", true))
			_lastScene = "Bank.330D_";
		_lastSound = "Bank.Vault_Opens";
		sound.play(_lastSound);
		if (_door >= 0)
			room.startAnim(_door, kBankDoorOpen, kModeOnce, -1);
		debugC(1, kDebugScript, "bank: the vault door opens at tick %u", script.getTick());
	}
	// A moan every 5-14 seconds while the story has them (rand() & 3, then
	// (rand() % 10 + 5) seconds).
	const int old = _moanDelay--;
	if (old > 0 || !_engine.getGameFlag("MoaningActive"))
		return;
	_lastSound = Common::String::format("Town_Center.Moan.%u", _engine.getRandomNumber(3) + 1);
	sound.play(_lastSound);
	_moanDelay = (_engine.getRandomNumber(9) + 5) * Room::kTicksPerSecond;
}

// ---------------------------------------------------------------------------
// The vault door's clock

// The dial's center, the hands' turning point (0x45F238, 0x45F234).
static const float kDialX = 319.0f;
static const float kDialY = 285.0f;

uint VaultClockRoom::getTarget(const Common::Point &p, uint steps) {
	// atan2 from straight up, clockwise, as a turn; then the nearest step.
	double turn = atan2((double)p.x - kDialX, -((double)p.y - kDialY)) * 0.15915493667125702;
	if (turn < 0.0)
		turn += 1.0;
	return (uint)(int)(turn * steps + 0.5) % steps;
}

void VaultClockRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42A830: the hands and the time, sprites at depth 0, hit by their
	// cels, stopped on step 0 (enter() stops them); the time shows the one
	// to set, ANIM_VAULT_CLOCK_TIME<hh>_<mm>.
	if (object == "OBJ_VAULT_CLOCK_HOUR" || object == "OBJ_VAULT_CLOCK_MIN") {
		claim.build = Room::Claim::kSprite;
		claim.depth = 0;
		claim.hitType = 1;
	} else if (object == "OBJ_VAULT_CLOCK_TIME") {
		// Its sequence is set in enter(): a time without one (no time drawn
		// yet) leaves it showing nothing, as vtable +0x24 does with an
		// unknown name.
		claim.build = Room::Claim::kSprite;
		claim.depth = 0;
		claim.hitType = 1;
	}
}

void VaultClockRoom::enter(const Common::String &from) {
	// 0x42AA70, after the base entry: the toolbar slides off (0x407CC0, in
	// the toolbar's rooms); nothing dragged.
	Room &room = *_engine.getRoom();
	_hour = findObject(room, "OBJ_VAULT_CLOCK_HOUR");
	_minute = findObject(room, "OBJ_VAULT_CLOCK_MIN");
	const int time = findObject(room, "OBJ_VAULT_CLOCK_TIME");
	if (_hour >= 0)
		room.setPlayer(_hour, kModeStopped, 0);
	if (_minute >= 0)
		room.setPlayer(_minute, kModeStopped, 0);
	if (time >= 0) {
		const Common::String anim = Common::String::format("ANIM_VAULT_CLOCK_TIME%.2d_%.2d", _engine.getGameInt("ClockHour"), _engine.getGameInt("ClockMinute"));
		if (!room.startAnim(time, anim, kModeStopped, -1))
			debugC(1, kDebugScript, "vault clock: no %s; the time shows nothing", anim.c_str());
	}
	_lastHour = _lastMinute = 0;
	_drag = 0;
	_pressed = false;
	_pressedOn = -1;
	_wasDown = _engine.isMouseDown();
	_leaving = false;
	_clicks = 0;
}

bool VaultClockRoom::click(int sprite) {
	// The button going down (game +0x20 bit 2) on a sprite (+0x08); the tick
	// reads it. The engine still handles the click (the exit).
	_pressed = true;
	_pressedOn = sprite;
	return false;
}

void VaultClockRoom::tick() {
	// 0x42AAA0, after the base tick.
	Room &room = *_engine.getRoom();
	const bool pressed = _pressed;
	const int pressedOn = _pressedOn;
	_pressed = false;
	_pressedOn = -1;
	const bool down = _engine.isMouseDown();
	const bool released = _wasDown && !down;
	_wasDown = down;
	if (_hour < 0 || _minute < 0)
		return;

	if (_drag == 0) {
		// A hand caught by the button going down on it.
		if (pressed && pressedOn == _hour)
			_drag = 1;
		else if (pressed && pressedOn == _minute)
			_drag = 2;
	} else {
		// The hand turns a step a tick toward the mouse, the shorter way.
		const int hand = _drag == 1 ? _hour : _minute;
		const uint steps = _drag == 1 ? 12 : 60;
		const Common::Point mouse = _engine.getMousePos();
		if (mouse.x != (int)kDialX || mouse.y != (int)kDialY) {
			const int step = room.getStep(hand);
			const uint diff = (getTarget(mouse, steps) + steps - step) % steps;
			if (diff == 0)
				room.setPlayer(hand, kModeStopped, -1);
			else if (diff < steps / 2)
				room.setPlayer(hand, 1, (uint)step + 1 >= steps ? 0 : step + 1);
			else
				room.setPlayer(hand, 2, step - 1);
		}
	}
	// Let go: both hands stop.
	if (_drag != 0 && released) {
		_drag = 0;
		room.setPlayer(_hour, kModeStopped, -1);
		room.setPlayer(_minute, kModeStopped, -1);
	}
	// Nothing dragged and the time set: the vault opens, back to the bank.
	if (_drag == 0 && !_leaving && (int)room.getStep(_minute) == _engine.getGameInt("ClockMinute") &&
	    (int)room.getStep(_hour) == _engine.getGameInt("ClockHour") % 12) {
		_leaving = true;
		_engine.setGameFlag("VaultOpen", true);
		debugC(1, kDebugScript, "vault clock: set at tick %u; the vault opens", _engine.getScript()->getTick());
		_engine.exitTo(kBank);
	}
	// A hand on a new step: the dial's click, again from its start.
	if (room.getStep(_minute) != _lastMinute || room.getStep(_hour) != _lastHour) {
		Sound &sound = *_engine.getSound();
		sound.stopClip("Vault_Clock.Dial.Click");
		sound.play("Vault_Clock.Dial.Click");
		_clicks++;
		_lastMinute = room.getStep(_minute);
		_lastHour = room.getStep(_hour);
	}
}

RoomClass *createVaultRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kBank)
		return new BankRoom(engine);
	if (room == kClockRoom)
		return new VaultClockRoom(engine);
	if (room == kVault)
		return new BankVaultRoom(engine);
	if (room == kOpenSafe)
		return new OpenSafeRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
