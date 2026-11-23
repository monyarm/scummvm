
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

#ifndef TERRAGLYPH_CONSOLE_H
#define TERRAGLYPH_CONSOLE_H

#include "gui/debugger.h"

namespace Terraglyph {

class Console : public GUI::Debugger {
private:
	bool Cmd_artList(int argc, const char **argv);
	bool Cmd_artInfo(int argc, const char **argv);
	bool Cmd_room(int argc, const char **argv);
	bool Cmd_soundList(int argc, const char **argv);
	bool Cmd_soundPlay(int argc, const char **argv);
	bool Cmd_soundStop(int argc, const char **argv);
	bool Cmd_movie(int argc, const char **argv);
	bool Cmd_inventory(int argc, const char **argv);
	bool Cmd_inventoryAdd(int argc, const char **argv);
	bool Cmd_inventoryRemove(int argc, const char **argv);
	bool Cmd_profile(int argc, const char **argv);
public:
	Console();
	~Console() override;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_CONSOLE_H
