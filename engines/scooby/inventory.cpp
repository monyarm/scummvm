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
#include "common/random.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/sound.h"

namespace Scooby {

// The cursors the game compares against (0x4089E0 sets the first two).
static const char *const kArrowCursor = "ANIM_CURSORARROW";
static const char *const kScrappyCursor = "ANIM_CURSORSCRAPPY";
static const char *const kOpenCursor = "ANIM_CURSOROPEN";

static const char *const kToolbarObject = "OBJ_TOOLBAR";
static const char *const kVanIdle = "ANIM_TOOLBARMYSTERY_MACHINE_IDLE";
static const char *const kVanDrive = "ANIM_TOOLBARMYSTERY_MACHINE_ANIM";
static const char *const kVanStop = "ANIM_TOOLBARMYSTERY_MACHINE_STOP";
static const char *const kSlotIdle = "ANIM_TOOLBARFINGERPRINT_IDLE";
static const char *const kSlotDim = "ANIM_TOOLBARFINGERPRINT_DIM";
static const char *const kToolbarSound = "Global.Toolbar";
static const char *const kQuitSound = "Sign_In.Quit_Button";

// The toolbar's geometry (0x406620, 0x407120): the van's foot sits on the
// screen's last row, it stops 5 pixels short of the right edge, and it
// drives 15 pixels a tick. The towed sprites keep 20 pixels apart.
static const int kScreenBottom = 479;      // 0x1DF
static const int kOpenRight = 635;         // 0x27B
static const int kScreenWidth = 640;       // 0x280
static const int kDriveStep = 15;
static const int kGap = 20;

// The toolbar indexes 0x406A70 switches on (+0x5C).
enum {
	kIndexVan = 4,
	kIndexScrappy = 5,
	kIndexQuit = 6,
	kIndexMagnifier = 7,
	kIndexSnacks = 8
};

// The wrong-item lines (0x4098B0): rand() % 4 picks a speaker until one is
// in the room. Fred speaks only where the room has no echo (+0x29), which
// no room of the default class sets.
static const char *const kWrongSpeakers[4][2] = {
	{ "FRED", "Global.WRONG_" },
	{ "SHAGGY", "Global.WRONGS_" },
	{ "VELMA", "Global.WRONGV_" },
	{ "SCOOBY", "Global.WRONGSC_" }
};
static const char *const kWrongNarrator = "Global.WRONGN_";

Inventory::Inventory(Room &room, Sound &sound, RoomScript &script, const Common::INIFile &objectIni) :
	_room(room), _sound(sound), _script(script), _ini(objectIni), _random(nullptr) {
	reset();
	_van = _vanWidth = _blockMoving = _blockOpen = _tens = _units = -1;
	_vanY = 0;
	_clickSprite = -1;
	_clicked = false;
	_movie = -1;
	_inputBlocked = false;
}

void Inventory::reset() {
	// 0x406530
	for (uint i = 0; i < kSlotCount; i++) {
		_slots[i].name.clear();
		_slots[i].toolbarAnim.clear();
		_slots[i].useCursor.clear();
		_slots[i].fingerprint = _slots[i].item = -1;
	}
	for (uint i = 0; i < kHistorySize; i++)
		_history[i].clear();
	_snacks = 1;
	_cursor = kArrowCursor;
	_busy = false;
	_x = 0;
	_total = 0;
	_heldTicks = 0;
	_gangHere = false;
	_openOnEntry = false;
	_quitting = false;
}

bool Inventory::isDefaultCursor() const {
	return _cursor == kArrowCursor;
}

Common::String Inventory::getIniString(const Common::String &section, const char *key) const {
	Common::String value;
	if (!_ini.getKey(key, section, value))
		return Common::String();
	value.trim();
	return value;
}

void Inventory::setRole(int sprite, Role role, int index) {
	_roles[sprite] = role;
	_indexes[sprite] = index;
}

int Inventory::addToolbarSprite(int parent, const char *anim, int index, Role role, int depth) {
	// 0x407E50: an OBJ_TOOLBAR sprite with its index, hit by its step
	// polygon (+4 = 2).
	const int sprite = _room.addEngineSprite(parent, kToolbarObject, anim, depth, 0, 0, 2,
	                                         Common::String::format("toolbar %d %s", index, anim));
	setRole(sprite, role, index);
	return sprite;
}

int Inventory::getOpenX() const {
	return kOpenRight - _vanWidth;
}

Inventory::State Inventory::getState() const {
	// 0x407AF0
	if (_van < 0 || _busy)
		return kStateBusy;
	if (_x == 0)
		return kStateParked;
	if (_x == getOpenX())
		return kStateOpen;
	if (_x < 0 || _x > getOpenX())
		return kStateMovingOut;
	return kStateMovingIn;
}

void Inventory::setVanX(int x) {
	_x = x;
	_room.setOffset(_van, x, _vanY);
}

void Inventory::startVan(const char *anim, int mode) {
	_room.startAnim(_van, anim, mode, -1);
}

void Inventory::enterRoom() {
	_roles.clear();
	_indexes.clear();
	_pickups.clear();
	_clickSprite = -1;
	_clicked = false;
	_nextRoom.clear();
	_movie = -1;
	_inputBlocked = false;

	// 0x406620: the van at the root, in front of the room, then the sprites
	// it tows, each 20 pixels left of the last.
	_van = addToolbarSprite(-1, kVanIdle, kIndexVan, kRoleVan);
	int w, h;
	_room.getSize(_van, w, h);
	_vanWidth = w;
	_vanY = kScreenBottom - h;
	_room.setOffset(_van, 0, _vanY);
	_total = 0;
	for (uint i = 0; i < kSlotCount; i++) {
		Slot &slot = _slots[i];
		slot.fingerprint = addToolbarSprite(_van, slot.name.empty() ? kSlotDim : kSlotIdle, i, kRoleFingerprint);
		_room.getSize(slot.fingerprint, w, h);
		_total += w + kGap;
		_room.setOffset(slot.fingerprint, -_total, 0);
		slot.item = slot.name.empty() ? -1 : addToolbarSprite(slot.fingerprint, slot.toolbarAnim.c_str(), i, kRoleItem);
	}
	const int magnifier = addToolbarSprite(_van, "ANIM_TOOLBARMAGGLASS_IDLE", kIndexMagnifier, kRoleButton);
	_room.getSize(magnifier, w, h);
	_total += w + kGap;
	_room.setOffset(magnifier, -_total, 0);

	// 0x40D020: the snack box and its two digits, the units after the tens.
	const int snackBox = addToolbarSprite(_van, "ANIM_TOOLBARSNACK_BOX_IDLE", kIndexSnacks, kRoleSnackBox);
	int boxWidth;
	_room.getSize(snackBox, boxWidth, h);
	_tens = addToolbarSprite(snackBox, "ANIM_TOOLBARSNACK_BOX_NUMBERS", kIndexSnacks, kRoleButton);
	_units = addToolbarSprite(snackBox, "ANIM_TOOLBARSNACK_BOX_NUMBERS", kIndexSnacks, kRoleButton);
	_room.getSize(_tens, w, h);
	_room.setOffset(_units, w, 0);
	_total += boxWidth + kGap;
	_room.setOffset(snackBox, -_total, 0);
	showSnacks();

	const int scrappy = addToolbarSprite(_van, "ANIM_TOOLBARSCRAPPY_IDLE", kIndexScrappy, kRoleScrappy);
	_room.getSize(scrappy, w, h);
	_total += w + kGap;
	_room.setOffset(scrappy, -_total, 0);
	const int quit = addToolbarSprite(_van, "ANIM_TOOLBARQUIT_GAME", kIndexQuit, kRoleButton);
	_room.getSize(quit, w, h);
	_total += w + kGap;
	_room.setOffset(quit, -_total, 0);

	// Two hit blockers at the root, hidden for now: one in front of the van
	// while it drives, one behind the open toolbar.
	_blockMoving = _room.addEngineSprite(-1, "OBJ_INVENTORY_BLOCK", "ANIM_INVENTORY_BLOCKHOTSPOT", kToolbarDepth + 1, 0, 0, 2, "toolbar block (moving)");
	_blockOpen = _room.addEngineSprite(-1, "OBJ_INVENTORY_BLOCK", "ANIM_INVENTORY_BLOCKHOTSPOT", kToolbarDepth - 1, 0, 0, 2, "toolbar block (open)");
	_room.setHidden(_blockMoving, true);
	_room.setHidden(_blockOpen, true);
	setRole(_blockMoving, kRoleBlockMoving, -1);
	setRole(_blockOpen, kRoleBlockOpen, -1);

	// 0x407D50: parked.
	_busy = false;
	setVanX(0);

	// The room's inventory objects (0x4026B0) and requires objects; an item
	// already taken is hidden (0x402AD0).
	for (uint s = 0; s < _room.getSpriteCount(); s++) {
		if (_room.isRequiresObject(s)) {
			setRole(s, kRoleRequires, -1);
		} else if (_room.isInventoryObject(s)) {
			const Common::String obj = _room.getObjectName(s);
			Pickup p;
			p.name = obj;
			p.toolbarAnim = getIniString(obj, "toolbaranim");
			p.useCursor = getIniString(obj, "usecursor");
			p.sound = getIniString(obj, "pickupsfx");
			// 0x40AC60: atoi, -1 for a missing or non-numeric value.
			const Common::String movie = getIniString(obj, "pickupmovie");
			p.movie = movie.empty() ? -1 : (Common::isDigit(movie[0]) || movie[0] == '-' ? atoi(movie.c_str()) : -1);
			p.pending = false;
			_pickups[s] = p;
			setRole(s, kRolePickup, -1);
			if (hasItem(obj)) {
				_room.setHidden(s, true);
				debugC(1, kDebugScript, "inventory: %s is taken already; hidden", obj.c_str());
			}
		}
	}

	// 0x409510: the arrow, whether the gang is here, and the toolbar opens
	// after a pick-up movie.
	_cursor = kArrowCursor;
	_gangHere = _script.hasCharacter("FRED") || _script.hasCharacter("SCOOBY") ||
	            _script.hasCharacter("SHAGGY") || _script.hasCharacter("VELMA");
	if (_openOnEntry && getState() == kStateParked)
		openToolbar();
	_openOnEntry = false;
}

void Inventory::showSnacks() {
	// 0x40D3B0: digit n shows on step n - 1, and 0 on step 10, which is
	// also the tens digit below 10.
	_room.setPlayer(_tens, 0, _snacks < 10 ? 10 : _snacks / 10 - 1);
	_room.setPlayer(_units, 0, _snacks % 10 == 0 ? 10 : _snacks % 10 - 1);
}

void Inventory::playSound(const Common::String &name) {
	// A name with no clip (pickupsfx=<none>) looks up as id 0 and plays
	// nothing.
	if (name.empty() || !_sound.findClip(name)) {
		_log += Common::String::format("nosound %s; ", name.c_str());
		return;
	}
	_sound.play(name);
	_log += Common::String::format("sound %s; ", name.c_str());
}

void Inventory::openToolbar() {
	// 0x407C00: no longer hidden; a parked toolbar drives in.
	_busy = false;
	if (getState() == kStateParked) {
		setVanX(kDriveStep);
		playSound(kToolbarSound);
		startVan(kVanDrive, 1);
		debugC(1, kDebugScript, "toolbar opens");
	}
}

bool Inventory::addItem(const Common::String &name) {
	return addItem(name, getIniString(name, "toolbaranim"), getIniString(name, "usecursor"));
}

bool Inventory::addItem(const Common::String &name, const Common::String &toolbarAnim, const Common::String &useCursor) {
	// 0x406DD0
	uint i = 0;
	while (i < kSlotCount && !_slots[i].name.empty())
		i++;
	if (i == kSlotCount) {
		debugC(1, kDebugScript, "inventory: Out of Inventory space for %s", name.c_str());
		_log += Common::String::format("full %s; ", name.c_str());
		return false;
	}
	Slot &slot = _slots[i];
	slot.name = name;
	slot.toolbarAnim = toolbarAnim;
	slot.useCursor = useCursor;
	// Only with no room change pending (game +0x8): the slot lights up, and
	// a parked toolbar drives in.
	if (_nextRoom.empty() && _van >= 0) {
		if (slot.fingerprint >= 0)
			_room.startAnim(slot.fingerprint, kSlotIdle, 1, -1);
		_busy = false;
		if (getState() == kStateParked) {
			setVanX(kDriveStep);
			playSound(kToolbarSound);
			startVan(kVanDrive, 1);
		}
		if (slot.fingerprint >= 0)
			slot.item = addToolbarSprite(slot.fingerprint, toolbarAnim.c_str(), i, kRoleItem);
	}

	// The history keeps every item ever taken.
	uint h = 0;
	while (h < kHistorySize && _history[h] != name)
		h++;
	if (h == kHistorySize) {
		h = 0;
		while (h < kHistorySize && !_history[h].empty())
			h++;
		if (h == kHistorySize) {
			warning("Out of Inventory space");
			slot.name.clear();
			if (slot.item >= 0)
				_room.setHidden(slot.item, true);
			slot.item = -1;
			return false;
		}
		_history[h] = name;
	}
	debugC(1, kDebugScript, "inventory: %s in slot %u (%s, cursor %s)", name.c_str(), i, toolbarAnim.c_str(), useCursor.c_str());
	_log += Common::String::format("add %s slot %u; ", name.c_str(), i);
	return true;
}

bool Inventory::removeItem(const Common::String &name) {
	// 0x406C80
	uint i = 0;
	while (i < kSlotCount && _slots[i].name != name)
		i++;
	if (i == kSlotCount) {
		debugC(1, kDebugScript, "inventory: Failed to located inventory item %s", name.c_str());
		return false;
	}
	Slot &slot = _slots[i];
	slot.name.clear();
	slot.toolbarAnim.clear();
	slot.useCursor.clear();
	if (slot.item >= 0)
		_room.setHidden(slot.item, true);
	slot.item = -1;
	if (slot.fingerprint >= 0)
		_room.startAnim(slot.fingerprint, kSlotDim, 1, -1);
	// An open toolbar drives off.
	if (_nextRoom.empty() && getState() == kStateOpen) {
		setVanX(_x + kDriveStep);
		playSound(kToolbarSound);
		startVan(kVanDrive, 1);
	}
	debugC(1, kDebugScript, "inventory: %s out of slot %u", name.c_str(), i);
	return true;
}

bool Inventory::hasItem(const Common::String &name) const {
	// 0x407060: the history, not the slots.
	for (uint i = 0; i < kHistorySize; i++) {
		if (_history[i] == name)
			return true;
	}
	return false;
}

void Inventory::click(int sprite) {
	_clicked = true;
	_clickSprite = sprite;
}

bool Inventory::handles(int sprite) const {
	return sprite >= 0 && _roles.contains(sprite);
}

Common::String Inventory::getRollover(int sprite) const {
	// Every rollover here is the open hand for the arrow and nothing for an
	// item in use: 0x407F80 (the toolbar's, but not over a fingerprint),
	// 0x40CF50 (Scrappy), 0x40D410 (the snack box), 0x402D20 (an item
	// waiting to be picked up has none), 0x403010 (requires).
	if (!handles(sprite) || !isDefaultCursor())
		return Common::String();
	switch (_roles.getVal(sprite)) {
	case kRoleVan:
	case kRoleItem:
	case kRoleButton:
	case kRoleSnackBox:
	case kRoleScrappy:
	case kRoleRequires:
		return kOpenCursor;
	case kRolePickup:
		return _pickups.getVal(sprite).pending ? Common::String() : Common::String(kOpenCursor);
	default:
		return Common::String();
	}
}

void Inventory::clickToolbar(int index) {
	// 0x406A70: only with the toolbar parked or open, and the arrow.
	if (_x != 0 && _x != getOpenX())
		return;
	if (!isDefaultCursor())
		return;
	if (index >= 0 && index < (int)kSlotCount && !_slots[index].name.empty()) {
		// The item becomes the cursor, and its slot dims.
		Slot &slot = _slots[index];
		_cursor = slot.useCursor;
		if (slot.item >= 0)
			_room.setHidden(slot.item, true);
		_room.startAnim(slot.fingerprint, kSlotDim, 1, -1);
		debugC(1, kDebugScript, "inventory: %s in use (cursor %s)", slot.name.c_str(), _cursor.c_str());
		_log += Common::String::format("use %s; ", slot.name.c_str());
	}
	if (index == kIndexVan) {
		setVanX(_x == 0 ? kDriveStep : getOpenX() + kDriveStep);
		playSound(kToolbarSound);
		startVan(kVanDrive, 1);
		debugC(1, kDebugScript, "toolbar %s", _x == kDriveStep ? "opens" : "closes");
	} else if (index == kIndexQuit) {
		// The quit button's sound plays; the main menu follows it.
		_quitting = true;
		playSound(kQuitSound);
	} else if (index == kIndexMagnifier) {
		_quitting = false;
		_nextRoom = "ROOM_Suspect_Chooser";
		_movie = -1;
		debugC(1, kDebugScript, "toolbar: the magnifying glass goes to %s", _nextRoom.c_str());
	}
}

void Inventory::updateToolbar() {
	// 0x407120
	if (_quitting && !_sound.isClipPlaying(kQuitSound)) {
		_quitting = false;
		if (_sound.findClip(kQuitSound)) {
			_nextRoom = "ROOM_Main_Menu";
			_movie = -1;
		}
	}
	// An item no longer the cursor goes back to its slot.
	for (uint i = 0; i < kSlotCount; i++) {
		Slot &slot = _slots[i];
		if (!slot.name.empty() && slot.item >= 0 && _room.isHidden(slot.item) && _cursor != slot.useCursor) {
			_room.setHidden(slot.item, false);
			_room.startAnim(slot.fingerprint, kSlotIdle, 1, -1);
			_log += Common::String::format("back %s; ", slot.name.c_str());
		}
	}

	_room.setHidden(_blockMoving, false);
	const State state = getState();
	_room.setHidden(_blockOpen, !(state == kStateOpen || _x == getOpenX()));
	if (_x != 0 && _x != getOpenX() && state != kStateBusy) {
		const int open = getOpenX();
		int stop;
		if (_x >= 0) {
			if (_x <= open) {
				// Driving in from the left edge.
				setVanX(_x + kDriveStep);
				if (_x <= open)
					return;
				stop = open;
			} else {
				// Driving off the right edge, then back in from the left.
				setVanX(_x + kDriveStep);
				if (_x < _total + kScreenWidth)
					return;
				stop = -(_vanWidth + 1);
			}
		} else {
			setVanX(_x + kDriveStep);
			if (_x <= 0)
				return;
			stop = 0;
		}
		setVanX(stop);
		startVan(kVanStop, 3);
		return;
	}
	_room.setHidden(_blockMoving, true);
}

void Inventory::roomEvent(const Common::String &object, bool success) {
	// Game vtable +0x40 -> the room's vtable +0x14, which the base room
	// class leaves empty (0x404440). Rooms with classes of their own act on
	// it.
	debugC(1, kDebugScript, "room event %s %d", object.c_str(), success ? 1 : 0);
	_log += Common::String::format("event %s %d; ", object.c_str(), success ? 1 : 0);
}

void Inventory::updatePickup(int sprite, Pickup &p, bool clicked) {
	// 0x402B00
	if (_room.isHidden(sprite))
		return;
	if (clicked && isDefaultCursor()) {
		if (p.movie == -1) {
			p.pending = true;
		} else {
			// The movie plays and the room loads again; the toolbar opens
			// after it (game +0x248).
			_inputBlocked = true;
			_nextRoom = _room.getName();
			_movie = p.movie;
			if (!hasItem(p.name))
				addItem(p.name, p.toolbarAnim, p.useCursor);
			_openOnEntry = true;
			_log += Common::String::format("movie %d; ", p.movie);
		}
	}
	if (!p.pending)
		return;
	const State state = getState();
	if (state == kStateOpen) {
		if (!hasItem(p.name))
			addItem(p.name, p.toolbarAnim, p.useCursor);
		if (hasItem(p.name)) {
			roomEvent(p.name, true);
			_room.setHidden(sprite, true);
			playSound(p.sound);
		}
		p.pending = false;
		return;
	}
	if (state != kStateMovingIn)
		openToolbar();
}

void Inventory::clickRequires(int sprite) {
	// 0x402F20: the right item, or the arrow, tells the room; either way
	// the cursor goes back to the arrow. Another item does nothing here.
	const Common::String obj = _room.getObjectName(sprite);
	const Common::String required = getIniString(obj, "requiredanim");
	if (_cursor == required)
		roomEvent(obj, true);
	else if (isDefaultCursor())
		roomEvent(obj, false);
	else
		return;
	_cursor = kArrowCursor;
}

void Inventory::tickObjects() {
	if (_van < 0)
		return;
	const int target = _clicked ? _clickSprite : -1;
	const Role role = (target >= 0 && _roles.contains(target)) ? _roles[target] : kRoleNone;

	// The toolbar's children update before the van (0x431130, front to
	// back): a click on a slot, an item or a button (0x407F30).
	if (role == kRoleFingerprint || role == kRoleItem || role == kRoleButton)
		clickToolbar(_indexes[target]);
	else if (role == kRoleSnackBox || role == kRoleScrappy)
		debugC(1, kDebugScript, "toolbar: %s is not implemented", role == kRoleSnackBox ? "the snack box" : "Scrappy");

	// The van (0x408060): a click drives it; STOP plays out into IDLE.
	if (role == kRoleVan && isDefaultCursor())
		clickToolbar(kIndexVan);
	if (_room.getAnim(_van) != kVanIdle && _room.getMode(_van) == 0)
		startVan(kVanIdle, 1);
	updateToolbar();

	// The room's objects, in sprite order.
	for (uint s = 0; s < _room.getSpriteCount(); s++) {
		if (_pickups.contains(s))
			updatePickup(s, _pickups[s], (int)s == target);
	}
	if (role == kRoleRequires)
		clickRequires(target);
}

void Inventory::tickGame() {
	// 0x4098B0, once a tick after the room's update: a click with an item
	// as the cursor that nothing took puts the item back, and a gang member
	// says so. The tick the item became the cursor doesn't count.
	const bool clicked = _clicked;
	_clicked = false;
	_clickSprite = -1;
	if (!_gangHere && getState() == kStateBusy)
		return;
	if (isDefaultCursor()) {
		_heldTicks = 0;
		return;
	}
	const int before = _heldTicks++;
	if (before < 1 || !clicked)
		return;
	if (_cursor != kScrappyCursor) {
		Common::String scene = kWrongNarrator;
		if (_gangHere) {
			for (uint tries = 0; tries < 1000; tries++) {
				const uint r = _random ? _random->getRandomNumber(3) : tries % 4;
				if (_script.hasCharacter(kWrongSpeakers[r][0])) {
					scene = kWrongSpeakers[r][1];
					break;
				}
			}
		}
		const bool started = _script.startScene(scene, false);
		debugC(1, kDebugScript, "inventory: %s used on nothing: scene %s%s", _cursor.c_str(), scene.c_str(), started ? "" : " (a scene is playing)");
		_log += Common::String::format("wrong %s; ", scene.c_str());
	}
	_cursor = kArrowCursor;
}

bool Inventory::getNextRoom(Common::String &room, int &movie) const {
	if (_nextRoom.empty())
		return false;
	room = _nextRoom;
	movie = _movie;
	return true;
}

Common::String Inventory::takeLog() {
	Common::String log = _log;
	_log.clear();
	return log;
}

Common::String Inventory::describeToolbar() const {
	Common::String out;
	for (uint s = 0; s < _room.getSpriteCount(); s++) {
		if (!_roles.contains(s) || _roles.getVal(s) == kRolePickup || _roles.getVal(s) == kRoleRequires)
			continue;
		int w, h;
		_room.getSize(s, w, h);
		out += Common::String::format("[%s %s %d,%d %dx%d%s] ", _room.getLabel(s).c_str(), _room.getAnim(s).c_str(),
		                              _room.getX(s), _room.getY(s), w, h, _room.isHidden(s) ? " hidden" : "");
	}
	return out;
}

} // End of namespace Scooby
