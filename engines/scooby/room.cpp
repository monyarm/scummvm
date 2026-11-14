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
#include "common/endian.h"
#include "common/textconsole.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"

#include "scooby/art_archive.h"
#include "scooby/cel.h"
#include "scooby/detection.h"
#include "scooby/room.h"

namespace Scooby {

// Room block (offsets from the block start):
//   0x0C u16 width, 0x0E u16 height
//   0x80 ten u32 layer slots: frame record offset, or 0
//   0xC0 u32 placement count, 0xC4 u32 offset of the first placement
// A placement is 148 bytes: char obj[64], char anim[64], 4 bytes, i16 x,
// i16 y, then 12 bytes the default placement path does not read.
static const uint32 kRoomWidth = 0x0C;
static const uint32 kRoomHeight = 0x0E;
static const uint32 kRoomLayerSlots = 0x80;
static const uint kRoomLayerCount = 10;
static const uint32 kRoomPlacementCount = 0xC0;
static const uint32 kRoomPlacementFirst = 0xC4;
static const uint32 kRoomHeaderSize = 0xC8;
static const uint32 kPlacementSize = 148;
static const uint32 kPlacementName = 64;
static const uint32 kPlacementX = 0x84;
static const uint32 kPlacementY = 0x86;

// Object resource: u32 size, then (count, offset) pairs for table 0
// (sequences) at 4 and table 1 (frame records) at 12. A sequence is u32 size,
// u16 n, u16 flags, then n 12-byte steps; a step's second u32 is its frame
// reference, whose low 18 bits index table 1.
static const uint32 kObjectSequenceCount = 4;
static const uint32 kObjectSequenceTable = 8;
static const uint32 kObjectFrameCount = 12;
static const uint32 kObjectFrameTable = 16;
static const uint32 kStepFrameRef = 8 + 4;

// A frame record is u32 size, u16 n, u16 0, then n x (i16 x, i16 y, u32 cel).
static const uint32 kFrameHeaderSize = 8;
static const uint32 kFrameEntrySize = 8;

// Sprite depths set on room entry (0x435DC0 and the placement handlers).
static const int kRootDepth = -0x7FFFFFFF;
static const int kLayerDepthBase = -110000;
static const int kLayerDepthStep = 10000;

// Characters placed at depth y - 200 (the 20-name table at 0x46A720 in
// Showdown's Scooby.exe, 0x469720 in Phantom's; the same in both).
static const char *const kCharacters[] = {
	"NARRATOR", "DAPHNE", "FRED", "SCOOBY", "SHAGGY", "VELMA", "ARTIE", "JENNY",
	"SHERIFF", "ICDOUBLE", "GUMMY", "DONKEY", "BRAD", "EWAN", "MARIA", "JANE",
	"WIZARD", "JESTER", "PRINCESS", "UNICYCLE"
};

static bool inBlock(const Common::Array<byte> &block, uint32 offset, uint32 size) {
	return offset <= block.size() && size <= block.size() - offset;
}

static Common::String readName(const byte *p) {
	const char *s = (const char *)p;
	const char *nul = (const char *)memchr(s, 0, kPlacementName);
	return Common::String(s, nul ? nul - s : kPlacementName);
}

RoomRenderer::RoomRenderer(const ArtArchive &art, const Common::INIFile &objectIni) : _art(art), _ini(objectIni) {
}

uint RoomRenderer::addSprite(uint parent, const Sprite &sprite) {
	// 0x430F40: children stay sorted by descending depth, and a new child
	// goes after every child whose depth is at least its own.
	const uint index = _sprites.size();
	_sprites.push_back(sprite);
	Common::Array<uint> &children = _sprites[parent].children;
	uint k = 0;
	while (k < children.size() && sprite.depth <= _sprites[children[k]].depth)
		k++;
	children.insert_at(k, index);
	return index;
}

void RoomRenderer::frontToBack(uint index, Common::Array<uint> &out) const {
	// 0x431000: the children at or above this sprite's depth, then the
	// sprite, then the rest.
	const Sprite &s = _sprites[index];
	uint k = 0;
	while (k < s.children.size() && s.depth <= _sprites[s.children[k]].depth)
		frontToBack(s.children[k++], out);
	out.push_back(index);
	while (k < s.children.size())
		frontToBack(s.children[k++], out);
}

int RoomRenderer::getIniInt(const Common::String &section, const char *key) const {
	// 0x40AD50: atoi of the value, but a missing or non-numeric value is -1.
	Common::String value;
	if (!_ini.getKey(key, section, value))
		return -1;
	value.trim();

	uint start = (!value.empty() && (value[0] == '-' || value[0] == '+')) ? 1 : 0;
	bool digits = start < value.size();
	for (uint i = start; i < value.size(); i++)
		digits = digits && Common::isDigit(value[i]);
	if (digits)
		return atoi(value.c_str());
	return value.hasPrefix("0") ? 0 : -1;
}

void RoomRenderer::getPlacementParent(const Common::String &obj, int y, int &slot, int &depth) const {
	// The default placement path (0x4091FF, 0x408F90, 0x408DC0, 0x401000).
	Common::String kind;
	if (!_ini.getKey("ID", obj, kind))
		kind = "simpleobject";
	kind.trim();
	kind.toLowercase();

	if (kind == "inventoryobject") {
		slot = 0;
		depth = -100;
	} else if (kind == "requiresobject") {
		slot = 0;
		depth = 0;
	} else if (kind == "exitobject") {
		slot = 0;
		depth = -1000;
	} else if (kind == "clickableobject") {
		const int layer = getIniInt(obj, "layer");
		slot = layer == -1 ? 0 : layer;
		depth = getIniInt(obj, "priority");
	} else {
		// A character, as OBJ_<NAME> or OBJ_<NAME>_<x>, goes in front of
		// layer 0 by its feet; anything else goes under layer 1.
		Common::String stem = obj;
		if (stem.size() > 2 && stem[stem.size() - 2] == '_')
			stem = Common::String(stem.c_str(), stem.size() - 2);
		bool character = obj == "OBJ_GHOST";
		for (uint i = 0; i < ARRAYSIZE(kCharacters) && !character; i++)
			character = stem == Common::String("OBJ_") + kCharacters[i];
		slot = character ? 0 : 1;
		depth = character ? y - 200 : 0;
	}
}

RoomRenderer::BlockPtr RoomRenderer::getObjectBlock(const Common::String &name) {
	BlockPtr block;
	if (_objects.tryGetVal(name, block))
		return block;

	block = BlockPtr(new Common::Array<byte>());
	if (!_art.readBlock(name, *block))
		block.reset();
	_objects[name] = block;
	return block;
}

bool RoomRenderer::findStepFrame(const Common::Array<byte> &object, uint32 sequence, uint32 &frame) const {
	if (!inBlock(object, 0, kObjectFrameTable + 4))
		return false;
	const uint32 sequenceCount = READ_LE_UINT32(&object[kObjectSequenceCount]);
	const uint32 sequenceTable = READ_LE_UINT32(&object[kObjectSequenceTable]);
	const uint32 frameCount = READ_LE_UINT32(&object[kObjectFrameCount]);
	const uint32 frameTable = READ_LE_UINT32(&object[kObjectFrameTable]);
	if (sequence >= sequenceCount || !inBlock(object, sequenceTable, 4 * sequenceCount) ||
	    !inBlock(object, frameTable, 4 * frameCount))
		return false;

	// Step 0: the default placement path starts the sequence there.
	const uint32 record = READ_LE_UINT32(&object[sequenceTable + 4 * sequence]);
	if (!inBlock(object, record, kStepFrameRef + 4) || READ_LE_UINT16(&object[record + 4]) == 0)
		return false;
	const uint32 index = READ_LE_UINT32(&object[record + kStepFrameRef]) & 0x3FFFF;
	if (index >= frameCount)
		return false;

	frame = READ_LE_UINT32(&object[frameTable + 4 * index]);
	return inBlock(object, frame, kFrameHeaderSize);
}

bool RoomRenderer::paint(const Sprite &sprite, Graphics::Surface &out) const {
	const Common::Array<byte> &block = *sprite.block;
	const uint n = READ_LE_UINT16(&block[sprite.frame + 4]);
	if (!inBlock(block, sprite.frame + kFrameHeaderSize, kFrameEntrySize * n))
		return false;

	for (uint k = 0; k < n; k++) {
		const byte *entry = &block[sprite.frame + kFrameHeaderSize + kFrameEntrySize * k];
		Cel cel;
		if (!cel.load(block.data(), block.size(), READ_LE_UINT32(entry + 4)))
			return false;
		cel.draw(out, sprite.x + (int16)READ_LE_UINT16(entry), sprite.y + (int16)READ_LE_UINT16(entry + 2));
	}
	return true;
}

bool RoomRenderer::render(const Common::String &room, Graphics::Surface &out, Common::String &order) {
	_sprites.clear();
	_objects.clear();
	order.clear();

	BlockPtr block(new Common::Array<byte>());
	if (!_art.readBlock(room, *block) || !inBlock(*block, 0, kRoomHeaderSize)) {
		warning("Room %s: cannot read its block", room.c_str());
		return false;
	}
	const Common::Array<byte> &b = *block;

	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	out.free();
	out.create(READ_LE_UINT16(&b[kRoomWidth]), READ_LE_UINT16(&b[kRoomHeight]), format);
	out.fillRect(Common::Rect(out.w, out.h), 0);

	Sprite root;
	root.depth = kRootDepth;
	root.frame = 0;
	root.x = root.y = 0;
	root.label = "root";
	_sprites.push_back(root);

	// One sprite per used layer slot, slot 0 furthest back.
	int layers[kRoomLayerCount];
	for (uint slot = 0; slot < kRoomLayerCount; slot++) {
		const uint32 frame = READ_LE_UINT32(&b[kRoomLayerSlots + 4 * slot]);
		layers[slot] = -1;
		if (!frame)
			continue;
		if (!inBlock(b, frame, kFrameHeaderSize)) {
			warning("Room %s: layer %u is outside its block", room.c_str(), slot);
			return false;
		}
		Sprite s;
		s.depth = kLayerDepthBase + kLayerDepthStep * (int)slot;
		s.block = block;
		s.frame = frame;
		s.x = s.y = 0;
		s.label = Common::String::format("layer %u", slot);
		layers[slot] = addSprite(0, s);
	}

	// One sprite per placement, at step 0 of its ANIM's sequence.
	const uint32 count = READ_LE_UINT32(&b[kRoomPlacementCount]);
	const uint32 first = READ_LE_UINT32(&b[kRoomPlacementFirst]);
	if (first > b.size() || (b.size() - first) / kPlacementSize < count) {
		warning("Room %s: placements are outside its block", room.c_str());
		return false;
	}
	for (uint32 k = 0; k < count; k++) {
		const byte *p = &b[first + kPlacementSize * k];
		const Common::String obj = readName(p);
		const Common::String anim = readName(p + kPlacementName);
		uint32 objId, animId, frame;
		BlockPtr object;
		if (!_art.findName(obj, objId) || (ArtNameType)(objId >> 28) != kArtObject ||
		    !_art.findName(anim, animId) || (ArtNameType)(animId >> 28) != kArtAnim ||
		    !(object = getObjectBlock(obj)) || !findStepFrame(*object, animId & 0x3FFFF, frame)) {
			warning("Room %s: placement %u (%s, %s) is not drawable", room.c_str(), k, obj.c_str(), anim.c_str());
			return false;
		}

		Sprite s;
		s.block = object;
		s.frame = frame;
		s.x = (int16)READ_LE_UINT16(p + kPlacementX);
		s.y = (int16)READ_LE_UINT16(p + kPlacementY);
		s.label = Common::String::format("%u %s", k, obj.c_str());
		int slot;
		getPlacementParent(obj, s.y, slot, s.depth);
		// A sprite whose layer slot is empty attaches to the root.
		const uint parent = (slot >= 0 && slot < (int)kRoomLayerCount && layers[slot] >= 0) ? layers[slot] : 0;
		addSprite(parent, s);
	}

	// The original draws this list front to back with occlusion and blends
	// translucent pixels back to front; painting it back to front gives the
	// same pixels.
	Common::Array<uint> list;
	frontToBack(0, list);
	for (uint i = 0; i < list.size(); i++) {
		if (list[i] == 0)
			continue;
		if (!order.empty())
			order += ", ";
		order += _sprites[list[i]].label;
	}
	for (int i = (int)list.size() - 1; i >= 0; i--) {
		const Sprite &s = _sprites[list[i]];
		if (s.block && !paint(s, out)) {
			warning("Room %s: sprite %s has bad frame data", room.c_str(), s.label.c_str());
			return false;
		}
	}

	debugC(2, kDebugGraphics, "room %s draw order, front to back: %s", room.c_str(), order.c_str());
	return true;
}

} // End of namespace Scooby
