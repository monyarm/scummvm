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

#include "terraglyph/console.h"
#include "terraglyph/formats/art_archive.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

Console::Console() : GUI::Debugger() {
	registerCmd("art_list", WRAP_METHOD(Console, Cmd_artList));
	registerCmd("art_info", WRAP_METHOD(Console, Cmd_artInfo));
	registerCmd("room", WRAP_METHOD(Console, Cmd_room));
	registerCmd("sound_list", WRAP_METHOD(Console, Cmd_soundList));
	registerCmd("sound_play", WRAP_METHOD(Console, Cmd_soundPlay));
	registerCmd("sound_stop", WRAP_METHOD(Console, Cmd_soundStop));
	registerCmd("movie", WRAP_METHOD(Console, Cmd_movie));
	registerCmd("inv", WRAP_METHOD(Console, Cmd_inventory));
	registerCmd("inv_add", WRAP_METHOD(Console, Cmd_inventoryAdd));
	registerCmd("inv_remove", WRAP_METHOD(Console, Cmd_inventoryRemove));
	registerCmd("profile", WRAP_METHOD(Console, Cmd_profile));
}

bool Console::Cmd_profile(int argc, const char **argv) {
	// The player profile as a save would hold it (the original's <name>.sav).
	debugPrintf("%s", g_engine->getProfileText().c_str());
	return true;
}

bool Console::Cmd_inventory(int argc, const char **argv) {
	const Inventory *inv = g_engine->getInventory();
	if (!inv) {
		debugPrintf("No inventory yet\n");
		return true;
	}
	for (uint i = 0; i < Inventory::kSlotCount; i++)
		debugPrintf("slot %u: %s\n", i, inv->getSlot(i).empty() ? "-" : inv->getSlot(i).c_str());
	debugPrintf("cursor %s, toolbar state %d at x %d\n", inv->getCursor().c_str(), inv->getState(), inv->getVanX());
	return true;
}

bool Console::Cmd_inventoryAdd(int argc, const char **argv) {
	Inventory *inv = g_engine->getInventory();
	if (argc < 2 || !inv) {
		debugPrintf("Usage: %s <OBJ name>  (puts an inventory object in the first free slot)\n", argv[0]);
		return true;
	}
	if (!inv->addItem(argv[1]))
		debugPrintf("No free slot\n");
	return true;
}

bool Console::Cmd_inventoryRemove(int argc, const char **argv) {
	Inventory *inv = g_engine->getInventory();
	if (argc < 2 || !inv) {
		debugPrintf("Usage: %s <OBJ name>  (takes an item out of its slot)\n", argv[0]);
		return true;
	}
	if (!inv->removeItem(argv[1]))
		debugPrintf("%s is not in a slot\n", argv[1]);
	return true;
}

bool Console::Cmd_soundList(int argc, const char **argv) {
	const Sound *sound = g_engine->getSound();
	if (!sound) {
		debugPrintf("No sound files are open\n");
		return true;
	}
	static const char *const kFiles[] = { "sfx", "voice", "music" };
	const Common::String prefix = argc > 1 ? argv[1] : "";
	uint shown = 0;
	for (uint f = 0; f < Sound::kFileCount; f++) {
		const Common::Array<SoundArchive::Clip> &clips = sound->getArchive((Sound::File)f).getClips();
		for (uint i = 0; i < clips.size(); i++) {
			const SoundArchive::Clip &c = clips[i];
			if (!c.name.hasPrefixIgnoreCase(prefix))
				continue;
			const uint32 ms = (uint32)((uint64)c.dataSize * 1000 / (c.rate * c.channels * (c.bits / 8)));
			debugPrintf("%-5s %-48s %5u Hz %u ch %2u bit %6u ms%s\n", kFiles[f], c.name.c_str(), c.rate, c.channels,
			            c.bits, ms, c.loop ? " loop" : "");
			shown++;
		}
	}
	debugPrintf("%u clips\n", shown);
	return true;
}

bool Console::Cmd_soundPlay(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <Section.Clip>; sound_list lists them\n", argv[0]);
		return true;
	}
	Sound *sound = g_engine->getSound();
	if (!sound || !sound->play(argv[1]))
		debugPrintf("%s is not in Sfx.dat, Voice.dat or Music.dat\n", argv[1]);
	return true;
}

bool Console::Cmd_movie(int argc, const char **argv) {
	if (argc < 2) {
		debugPrintf("Usage: %s <id>  (plays BK.<id>, then returns to the game)\n", argv[0]);
		return true;
	}
	if (!g_engine->playMovie(atoi(argv[1])))
		debugPrintf("Movie %s could not be played\n", argv[1]);
	return false;
}

bool Console::Cmd_soundStop(int argc, const char **argv) {
	Sound *sound = g_engine->getSound();
	if (sound)
		sound->stop();
	return true;
}

Console::~Console() {
}

bool Console::Cmd_artList(int argc, const char **argv) {
	const ArtArchive *art = g_engine->getArt();
	if (!art) {
		debugPrintf("No art file is open\n");
		return true;
	}

	const Common::String prefix = argc > 1 ? argv[1] : "";
	uint shown = 0;
	for (const ArtName &n : art->getNames()) {
		if (n.getType() == kArtAnim || !n.name.hasPrefixIgnoreCase(prefix))
			continue;

		uint32 offset = 0, size = 0;
		art->getSpan(n.name, offset, size);
		debugPrintf("%-6s %4u  %-40s 0x%08x %8u\n", n.getType() == kArtRoom ? "room" : "object",
		            n.getIndex(), n.name.c_str(), offset, size);
		shown++;
	}
	debugPrintf("%u of %u rooms and objects\n", shown, art->getRoomCount() + art->getObjectCount());
	return true;
}

bool Console::Cmd_artInfo(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <ROOM_, OBJ_ or ANIM_ name>\n", argv[0]);
		return true;
	}

	const ArtArchive *art = g_engine->getArt();
	if (!art) {
		debugPrintf("No art file is open\n");
		return true;
	}

	ArtName n;
	n.name = argv[1];
	if (!art->findName(n.name, n.id)) {
		debugPrintf("%s is not in the art file\n", argv[1]);
		return true;
	}

	if (n.getType() == kArtAnim) {
		// The owner field keeps only the low 8 bits of the object index, so
		// the object comes from context, not from the id.
		debugPrintf("ANIM id 0x%08x: owner object index & 0xFF = %u, sequence %u\n",
		            n.id, (n.id >> 20) & 0xFF, n.id & 0x3FFFF);
		return true;
	}

	uint32 offset = 0, size = 0;
	art->getSpan(n.name, offset, size);
	debugPrintf("%s id 0x%08x: index %u, offset 0x%08x, %u bytes\n",
	            n.getType() == kArtRoom ? "ROOM" : "OBJ", n.id, n.getIndex(), offset, size);
	return true;
}

bool Console::Cmd_room(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <ROOM_ name>; art_list ROOM_ lists them\n", argv[0]);
		return true;
	}

	const ArtArchive *art = g_engine->getArt();
	uint32 id;
	if (!art || !art->findName(argv[1], id) || (ArtNameType)(id >> 28) != kArtRoom) {
		debugPrintf("%s is not a room\n", argv[1]);
		return true;
	}

	if (!g_engine->showRoom(argv[1]))
		debugPrintf("%s could not be drawn\n", argv[1]);
	return true;
}

} // End of namespace Terraglyph
