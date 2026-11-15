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

#include "scooby/detection.h"
#include "scooby/menu_rooms.h"
#include "scooby/options_rooms.h"
#include "scooby/accusation_rooms.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/scooby.h"
#include "scooby/signin_room.h"
#include "scooby/sound.h"
#include "scooby/story_rooms.h"

namespace Scooby {

// The button sound playing (0x475F50): a button doesn't play its sound
// again while it is still the one playing.
static Common::String g_buttonSound;

int ButtonRoom::addButton(int sprite, const Common::String &sound) {
	Room &room = *_engine.getRoom();
	Button b;
	b.sprite = sprite;
	b.sound = sound;
	b.state = 0;
	// 0x436C50 starts the sequence stopped on step 0; 0x436FC0 picks the
	// hit test.
	room.setPlayer(sprite, 0, 0);
	room.setHitType(sprite, room.hasStepPolygon(sprite) ? 2 : 1);
	// Every menu button of both titles has a polygon on step 0 only. Taken
	// literally, 0x430D10 would lose the button the tick it shows its hover
	// step, and it would flicker; the button keeps step 0's shape instead.
	room.setFirstStepHit(sprite);
	_buttons.push_back(b);
	return _buttons.size() - 1;
}

int ButtonRoom::findButton(int sprite) const {
	for (uint i = 0; i < _buttons.size(); i++) {
		if (sprite >= 0 && _buttons[i].sprite == sprite)
			return i;
	}
	return -1;
}

int ButtonRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

bool ButtonRoom::click(int sprite) {
	const int b = findButton(sprite);
	if (b < 0 || isBusy())
		return false;
	// The click callback (+0x78) runs on the button's update; it shows
	// the pressed step.
	debugC(1, kDebugScript, "button %s clicked", _engine.getRoom()->getLabel(sprite).c_str());
	_buttons[b].state = 2;
	_engine.getRoom()->setPlayer(sprite, 0, 2);
	onClick(b);
	return true;
}

void ButtonRoom::tick() {
	// 0x436D70, each button's update.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	if (!g_buttonSound.empty() && !sound.isClipPlaying(g_buttonSound))
		g_buttonSound.clear();
	const int hover = isBusy() ? -1 : _engine.getHover();
	for (uint i = 0; i < _buttons.size(); i++) {
		Button &b = _buttons[i];
		int state = 0;
		if (b.sprite == hover) {
			if (_engine.isMouseDown()) {
				state = 2;
			} else {
				state = 1;
				if (b.state == 0) {
					// Coming under the mouse: its sound, unless it's the one
					// playing (a silent button stops that one), and the
					// rollover callback.
					if (b.sound != g_buttonSound) {
						if (!g_buttonSound.empty())
							sound.stopClip(g_buttonSound);
						if (!b.sound.empty())
							sound.play(b.sound);
						g_buttonSound = b.sound;
					}
					onRollover(i, true);
				}
			}
		}
		if (b.state == 1 && state != 1)
			onRollover(i, false);
		b.state = state;
		room.setPlayer(b.sprite, 0, state);
	}
}

RoomClass::~RoomClass() {
	RoomScript *script = _engine.getScript();
	if (script && script->getHooks() == this)
		script->setHooks(nullptr);
}

static RoomClass *buildRoomClass(ScoobyEngine &engine, const Common::String &room) {
	// The factory's classes for the menus, in both Gen 1 titles.
	if (room == "ROOM_Title_Screen")
		return new TitleRoom(engine);
	if (room == "ROOM_Main_Menu")
		return new MainMenuRoom(engine);
	if (room == "ROOM_Difficulty")
		return new DifficultyRoom(engine);
	if (room == "ROOM_Quit")
		return new QuitRoom(engine);
	if (room == "ROOM_Sign_In")
		return new SignInRoom(engine);
	if (room == "ROOM_Options")
		return new OptionsRoom(engine);
	if (room == "ROOM_Credits")
		return new CreditsRoom(engine);
	if (room == "ROOM_Suspect_Chooser")
		return new SuspectChooserRoom(engine);
	if (room == "ROOM_Big_Head")
		return new BigHeadRoom(engine);
	// The story rooms (story_rooms.cpp); every other room gets the base
	// class for now.
	return createStoryRoom(engine, room);
}

RoomClass *createRoomClass(ScoobyEngine &engine, const Common::String &room) {
	// The game resets each room it builds (vtable +0x08).
	RoomClass *c = buildRoomClass(engine, room);
	if (c)
		c->reset();
	return c;
}

} // End of namespace Scooby
