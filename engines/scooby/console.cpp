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

#include "scooby/console.h"
#include "scooby/art_archive.h"
#include "scooby/scooby.h"

namespace Scooby {

Console::Console() : GUI::Debugger() {
	registerCmd("art_list", WRAP_METHOD(Console, Cmd_artList));
	registerCmd("art_info", WRAP_METHOD(Console, Cmd_artInfo));
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

} // End of namespace Scooby
