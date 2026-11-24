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

#ifndef TERRAGLYPH_WIZARDS_BEDROOM_ROOMS_H
#define TERRAGLYPH_WIZARDS_BEDROOM_ROOMS_H

#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Phantom's wizard's bedroom and veranda (WP-081), the third of the audit's
 * group-8 (Upstairs) rooms. The addresses are Phantom's; Showdown has neither
 * room. The last group-8 room, ROOM_P30_WFR (the Wizard's Rule Room, the game's
 * largest class), is its own packet (WP-083).
 */

/**
 * ROOM_P24_Wizards_Bedroom (class 0x42CCF0, vtable 0x45F444): the wizard's
 * bedroom. The wizard is met here (OBJ_INV_MAZE_MAP is the scroll he gives); on
 * the return from the Wizard's Rule Room (ROOM_P30_WFR) his clue is given and he
 * ACTs GIVES, which arms the accusation: the tick, once he is idle, takes the
 * gang to ROOM_Suspect_Chooser with Global.Thank_You_ (as the dining room and the
 * arenas do). The maze-map scroll (OBJ_INV_MAZE_MAP, an inventory placement) shows
 * only while his clue is given and the gang hasn't the map, when
 * Wizards_Bedroom.Scroll plays; otherwise it is hidden. Music "1", no opening.
 */
class WizardsBedroomRoom : public RoomClass {
public:
	explicit WizardsBedroomRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void tick() override;                                                            // 0x42CF70
	void enter(const Common::String &from) override;                                 // 0x42CD70
	void reset() override;                                                            // 0x42CD50
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x42D140

	// For the check.
	bool armed() const { return _armed; }

private:
	bool _armed = false; // +0xB8: the wizard GIVES, the tick to send the gang to accuse
	int _mazeMap = -1;   // the placed maze-map scroll sprite, found on entry
};

/**
 * ROOM_P28_Veranda (class 0x4183B0, vtable 0x45ECDC): the veranda, where the
 * well is looked down (OBJ_P28_VERANDA_HOTSPOT1). Each click cycles a line
 * (Veranda.290B_ / 290X1_ / 290X2_, by WellClicks) and sends the gang to
 * ROOM_Big_Head (BIG_HEAD_BG_P44P22) to say it, back here. The first entry (not
 * back from Big Head) opens with Veranda.270D_; before the first well click,
 * Veranda.290_. Music "Princess_Bedroom.Music" (shared), no opening. Entrances
 * and WellClicks are saved.
 */
class VerandaRoom : public RoomClass {
public:
	explicit VerandaRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                    // 0x4185C0
	void event(const Common::String &object, bool success) override;         // 0x418410
	void arrive(const Common::String &from) override;                        // 0x4185E0 (+0x2C)
	void saveKeys(Common::INIFile &profile) override;                        // 0x418650
	void loadKeys(const Common::INIFile &profile) override;                  // 0x4186B0

	// For the check.
	int wellClicks() const { return _wellClicks; }

private:
	int _entrances = 0;  // +0xB8: saved "Entrances"; non-zero once the intro has played
	int _wellClicks = 0; // +0xBC: saved "WellClicks"; which well line is next (1..3, wrapping)
};

/** The wizard's-bedroom / veranda class for @p room, or nullptr. */
RoomClass *createWizardsBedroomRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_WIZARDS_BEDROOM_ROOMS_H
