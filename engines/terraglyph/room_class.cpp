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

#include "terraglyph/alley_rooms.h"
#include "terraglyph/arena_rooms.h"
#include "terraglyph/below_balcony_rooms.h"
#include "terraglyph/armory_rooms.h"
#include "terraglyph/brad_rooms.h"
#include "terraglyph/detection.h"
#include "terraglyph/passage_rooms.h"
#include "terraglyph/dining_rooms.h"
#include "terraglyph/doors_rooms.h"
#include "terraglyph/drawbridge_rooms.h"
#include "terraglyph/ewan_rooms.h"
#include "terraglyph/gallery_rooms.h"
#include "terraglyph/kst_rooms.h"
#include "terraglyph/maze_rooms.h"
#include "terraglyph/maze_interior_rooms.h"
#include "terraglyph/garden_rooms.h"
#include "terraglyph/ghost_spot_room.h"
#include "terraglyph/gummy_rooms.h"
#include "terraglyph/hotel_rooms.h"
#include "terraglyph/jail_rooms.h"
#include "terraglyph/kitchen_rooms.h"
#include "terraglyph/maria_rooms.h"
#include "terraglyph/office_rooms.h"
#include "terraglyph/post_rooms.h"
#include "terraglyph/saloon_rooms.h"
#include "terraglyph/sheriff_rooms.h"
#include "terraglyph/stage_rooms.h"
#include "terraglyph/station_rooms.h"
#include "terraglyph/storage_rooms.h"
#include "terraglyph/store_rooms.h"
#include "terraglyph/strongbox_rooms.h"
#include "terraglyph/menu_rooms.h"
#include "terraglyph/options_rooms.h"
#include "terraglyph/accusation_rooms.h"
#include "terraglyph/boot_hill_rooms.h"
#include "terraglyph/chase_rooms.h"
#include "terraglyph/corral_rooms.h"
#include "terraglyph/pienoon_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/signin_room.h"
#include "terraglyph/souvenir_rooms.h"
#include "terraglyph/sound.h"
#include "terraglyph/story_rooms.h"
#include "terraglyph/throne_rooms.h"
#include "terraglyph/tunnel_rooms.h"
#include "terraglyph/princess_rooms.h"
#include "terraglyph/vault_rooms.h"
#include "terraglyph/wizards_bedroom_rooms.h"
#include "terraglyph/wizards_rooms.h"

namespace Terraglyph {

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

static RoomClass *buildRoomClass(TerraglyphEngine &engine, const Common::String &room) {
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
	if (room == "ROOM_Ghost_Spot")
		return new GhostSpotRoom(engine);
	// Phantom's drawbridge and town (drawbridge_rooms.cpp).
	if (RoomClass *drawbridge = createDrawbridgeRoom(engine, room))
		return drawbridge;
	if (RoomClass *souvenir = createSouvenirRoom(engine, room))
		return souvenir;
	if (RoomClass *garden = createGardenRoom(engine, room))
		return garden;
	if (RoomClass *doors = createDoorsRoom(engine, room))
		return doors;
	if (RoomClass *throne = createThroneRoom(engine, room))
		return throne;
	if (RoomClass *dining = createDiningRoom(engine, room))
		return dining;
	if (RoomClass *maria = createMariaRoom(engine, room))
		return maria;
	if (RoomClass *kitchen = createKitchenRoom(engine, room))
		return kitchen;
	if (RoomClass *ewan = createEwanRoom(engine, room))
		return ewan;
	if (RoomClass *storage = createStorageRoom(engine, room))
		return storage;
	if (RoomClass *armory = createArmoryRoom(engine, room))
		return armory;
	if (RoomClass *arena = createArenaRoom(engine, room))
		return arena;
	if (RoomClass *brad = createBradRoom(engine, room))
		return brad;
	if (RoomClass *passage = createPassageRoom(engine, room))
		return passage;
	if (RoomClass *gallery = createGalleryRoom(engine, room))
		return gallery;
	if (RoomClass *kst = createKstRoom(engine, room))
		return kst;
	if (RoomClass *maze = createMazeRoom(engine, room))
		return maze;
	if (RoomClass *mazeInterior = createMazeInteriorRoom(engine, room))
		return mazeInterior;
	if (RoomClass *belowBalcony = createBelowBalconyRoom(engine, room))
		return belowBalcony;
	if (RoomClass *wizards = createWizardsRoom(engine, room))
		return wizards;
	if (RoomClass *princess = createPrincessRoom(engine, room))
		return princess;
	if (RoomClass *wizardsBedroom = createWizardsBedroomRoom(engine, room))
		return wizardsBedroom;
	// Showdown's chase (chase_rooms.cpp).
	if (RoomClass *chase = createChaseRoom(engine, room))
		return chase;
	if (RoomClass *vault = createVaultRoom(engine, room))
		return vault;
	if (RoomClass *hill = createBootHillRoom(engine, room))
		return hill;
	if (RoomClass *tunnel = createTunnelRoom(engine, room))
		return tunnel;
	if (RoomClass *hotel = createHotelRoom(engine, room))
		return hotel;
	if (RoomClass *alley = createAlleyRoom(engine, room))
		return alley;
	if (RoomClass *pie = createPieNoonRoom(engine, room))
		return pie;
	if (RoomClass *corral = createCorralRoom(engine, room))
		return corral;
	if (RoomClass *box = createStrongBoxRoom(engine, room))
		return box;
	if (RoomClass *sheriff = createSheriffRoom(engine, room))
		return sheriff;
	if (RoomClass *saloon = createSaloonRoom(engine, room))
		return saloon;
	if (RoomClass *gummy = createGummyRoom(engine, room))
		return gummy;
	if (RoomClass *station = createStationRoom(engine, room))
		return station;
	if (RoomClass *post = createPostRoom(engine, room))
		return post;
	if (RoomClass *office = createOfficeRoom(engine, room))
		return office;
	if (RoomClass *jail = createJailRoom(engine, room))
		return jail;
	if (RoomClass *store = createStoreRoom(engine, room))
		return store;
	if (RoomClass *stage = createStageRoom(engine, room))
		return stage;
	// The story rooms (story_rooms.cpp); every other room gets the base
	// class for now.
	return createStoryRoom(engine, room);
}

RoomClass *createRoomClass(TerraglyphEngine &engine, const Common::String &room) {
	// The game resets each room it builds (vtable +0x08).
	RoomClass *c = buildRoomClass(engine, room);
	if (c)
		c->reset();
	return c;
}

} // End of namespace Terraglyph
