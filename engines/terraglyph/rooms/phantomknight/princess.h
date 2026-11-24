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

#ifndef TERRAGLYPH_PRINCESS_ROOMS_H
#define TERRAGLYPH_PRINCESS_ROOMS_H

#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Phantom's Princess's bedroom and Mermaid's cavern (WP-078), the second of the
 * audit's group-8 (Upstairs) rooms. The addresses are Phantom's; Showdown has
 * neither room.
 */

/**
 * ROOM_P29_Princess_Bedroom (class 0x415350, vtable 0x45EA5C): the bedroom where
 * the spell book (OBJ_INV_SPELL_BOOK, which the wizard's library consumes) is
 * taken. The book sits as a requires object until taken; clicking it has Daphne
 * walk over and take it (OBJ_DAPHNE_P24_TAKE_BOOK, with Princess_Bedroom.
 * Daphne_Takes_Book), after which it goes in the toolbar and the got line plays.
 * The one hotspot gives a line. Music "Princess_Bedroom.Music", opening
 * "Princess_Bedroom.280_". No own save keys.
 */
class PrincessBedroomRoom : public RoomClass {
public:
	explicit PrincessBedroomRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void tick() override;                                                            // 0x4153B0
	void enter(const Common::String &from) override;                                 // 0x415700
	void roomLeft() override;                                                         // 0x415520
	void event(const Common::String &object, bool success) override;                 // 0x415570
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x4155E0
	bool isBusy() const override { return _takePending; } // input held (game +0x24) through the take

	static const char *kHotspotLine; // Princess_Bedroom.280XA_ (the hotspot clicked)
	static const char *kBookGot;     // Princess_Bedroom.280A_ (the book taken)
	static const char *kTakeScene;   // Princess_Bedroom.Daphne_Takes_Book (Daphne taking it)
	static const char *kTakeAnim;    // OBJ_DAPHNE_P24_TAKE_BOOK (her take sequence)

	// For the check.
	bool takePending() const { return _takePending; }

private:
	bool _takePending = false; // +0xB8: the spell book clicked, Daphne to take it
	bool _taking = false;      // +0xB9: Daphne mid-take
	int _spellBook = -1;       // the placed spell-book sprite, found on entry
};

/**
 * ROOM_P21_Mermaids_Cavern (class 0x415280, vtable 0x45EA1C): the cavern. Its
 * one hotspot (OBJ_P21_MERMAID_HOTSPOT1) frightens the gang — Daphne, Shaggy and
 * Scooby each play their SCARED reaction. Music "1", opening
 * "Mermaids_Cavern.340_". The class overrides event only.
 */
class MermaidsCavernRoom : public RoomClass {
public:
	explicit MermaidsCavernRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void event(const Common::String &object, bool success) override; // 0x4152E0
	bool isEcho() const override { return true; } // ctor sets room +0x29: an echo room

	static const char *kHotspot; // OBJ_P21_MERMAID_HOTSPOT1 (the hotspot that scares the gang)
};

/** The Princess's-bedroom / Mermaid's-cavern class for @p room, or nullptr. */
RoomClass *createPrincessRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_PRINCESS_ROOMS_H
