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
// (sequences) at 4 and table 1 (frame records) at 12. A sequence record is
// u32 size, u16 n, u16 flags, then n 12-byte steps; a step's second u32 is
// its frame reference, whose low 18 bits index table 1.
static const uint32 kObjectSequenceCount = 4;
static const uint32 kObjectSequenceTable = 8;
static const uint32 kObjectFrameCount = 12;
static const uint32 kObjectFrameTable = 16;
static const uint32 kSequenceSteps = 4;
static const uint32 kSequenceHeaderSize = 8;
static const uint32 kStepSize = 12;
static const uint32 kStepFrameRef = 4;

// A frame record is u32 size, u16 n, u16 0, then n x (i16 x, i16 y, u32 cel).
static const uint32 kFrameHeaderSize = 8;
static const uint32 kFrameEntrySize = 8;

// Sprite depths set on room entry (0x435DC0 and the placement handlers).
static const int kRootDepth = -0x7FFFFFFF;
static const int kLayerDepthBase = -110000;
static const int kLayerDepthStep = 10000;

// The default placement path starts each placed sprite in mode 1 (loop
// forward) at 3 ticks per step, from step 0.
static const int kPlacementMode = 1;
static const int kPlacementTicksPerStep = 3;

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

// A frame record whose cel entries all lie inside the block.
static bool isFrameRecord(const Common::Array<byte> &block, uint32 frame) {
	if (!inBlock(block, frame, kFrameHeaderSize))
		return false;
	return inBlock(block, frame + kFrameHeaderSize, kFrameEntrySize * READ_LE_UINT16(&block[frame + 4]));
}

Room::Room(const ArtArchive &art, const Common::INIFile &objectIni) : _art(art), _ini(objectIni), _width(0), _height(0) {
}

Room::~Room() {
}

uint Room::addSprite(uint parent, const Sprite &sprite) {
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

void Room::frontToBack(uint index, Common::Array<uint> &out) const {
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

int Room::getIniInt(const Common::String &section, const char *key) const {
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

void Room::getPlacementParent(const Common::String &obj, int y, int &slot, int &depth) const {
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

Room::BlockPtr Room::getObjectBlock(const Common::String &name) {
	BlockPtr block;
	if (_objects.tryGetVal(name, block))
		return block;

	block = BlockPtr(new Block());
	if (!_art.readBlock(name, block->data))
		block.reset();
	_objects[name] = block;
	return block;
}

bool Room::startSequence(Sprite &sprite, uint32 sequence, int mode, int ticksPerStep, int startStep) {
	// Check the whole sequence once, so that ticking and drawing can't
	// step outside the block.
	const Common::Array<byte> &object = sprite.block->data;
	if (!inBlock(object, 0, kObjectFrameTable + 4))
		return false;
	const uint32 sequenceCount = READ_LE_UINT32(&object[kObjectSequenceCount]);
	const uint32 sequenceTable = READ_LE_UINT32(&object[kObjectSequenceTable]);
	const uint32 frameCount = READ_LE_UINT32(&object[kObjectFrameCount]);
	const uint32 frameTable = READ_LE_UINT32(&object[kObjectFrameTable]);
	if (sequence >= sequenceCount || !inBlock(object, sequenceTable, 4 * sequenceCount) ||
	    !inBlock(object, frameTable, 4 * frameCount))
		return false;

	Player &p = sprite.player;
	p.record = READ_LE_UINT32(&object[sequenceTable + 4 * sequence]);
	if (!inBlock(object, p.record, kSequenceHeaderSize))
		return false;
	p.steps = READ_LE_UINT16(&object[p.record + kSequenceSteps]);
	if (p.steps == 0 || !inBlock(object, p.record + kSequenceHeaderSize, kStepSize * p.steps))
		return false;
	for (uint32 k = 0; k < p.steps; k++) {
		const uint32 ref = READ_LE_UINT32(&object[p.record + kSequenceHeaderSize + kStepSize * k + kStepFrameRef]);
		if ((ref & 0x3FFFF) >= frameCount ||
		    !isFrameRecord(object, READ_LE_UINT32(&object[frameTable + 4 * (ref & 0x3FFFF)])))
			return false;
	}

	// 0x430AA0
	p.mode = mode;
	p.ticksPerStep = p.countdown = ticksPerStep;
	p.step = 0;
	if (mode == 5) {
		p.mode = 0;
		p.step = p.steps - 1;
	} else if (startStep == -1) {
		if (mode == 2 || mode == 4)
			p.step = p.steps - 1;
	} else {
		p.step = (uint32)startStep < p.steps ? startStep : p.steps - 1;
	}
	sprite.animated = true;
	return true;
}

bool Room::load(const Common::String &name) {
	_name = name;
	_sprites.clear();
	_drawList.clear();
	_order.clear();
	_objects.clear();

	BlockPtr room(new Block());
	if (!_art.readBlock(name, room->data) || !inBlock(room->data, 0, kRoomHeaderSize)) {
		warning("Room %s: cannot read its block", name.c_str());
		return false;
	}
	const Common::Array<byte> &b = room->data;
	_width = READ_LE_UINT16(&b[kRoomWidth]);
	_height = READ_LE_UINT16(&b[kRoomHeight]);

	Sprite root;
	root.depth = kRootDepth;
	root.frame = 0;
	root.animated = false;
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
		if (!isFrameRecord(b, frame)) {
			warning("Room %s: layer %u is outside its block", name.c_str(), slot);
			return false;
		}
		Sprite s;
		s.depth = kLayerDepthBase + kLayerDepthStep * (int)slot;
		s.block = room;
		s.frame = frame;
		s.animated = false;
		s.x = s.y = 0;
		s.label = Common::String::format("layer %u", slot);
		layers[slot] = addSprite(0, s);
	}

	// One sprite per placement, playing its ANIM's sequence.
	const uint32 count = READ_LE_UINT32(&b[kRoomPlacementCount]);
	const uint32 first = READ_LE_UINT32(&b[kRoomPlacementFirst]);
	if (first > b.size() || (b.size() - first) / kPlacementSize < count) {
		warning("Room %s: placements are outside its block", name.c_str());
		return false;
	}
	for (uint32 k = 0; k < count; k++) {
		const byte *p = &b[first + kPlacementSize * k];
		const Common::String obj = readName(p);
		const Common::String anim = readName(p + kPlacementName);
		uint32 objId, animId;
		Sprite s;
		s.frame = 0;
		s.animated = false;
		if (!_art.findName(obj, objId) || (ArtNameType)(objId >> 28) != kArtObject ||
		    !_art.findName(anim, animId) || (ArtNameType)(animId >> 28) != kArtAnim ||
		    !(s.block = getObjectBlock(obj)) ||
		    !startSequence(s, animId & 0x3FFFF, kPlacementMode, kPlacementTicksPerStep, 0)) {
			warning("Room %s: placement %u (%s, %s) is not drawable", name.c_str(), k, obj.c_str(), anim.c_str());
			return false;
		}

		s.x = (int16)READ_LE_UINT16(p + kPlacementX);
		s.y = (int16)READ_LE_UINT16(p + kPlacementY);
		s.label = Common::String::format("%u %s", k, obj.c_str());
		int slot;
		getPlacementParent(obj, s.y, slot, s.depth);
		// A sprite whose layer slot is empty attaches to the root.
		const uint parent = (slot >= 0 && slot < (int)kRoomLayerCount && layers[slot] >= 0) ? layers[slot] : 0;
		addSprite(parent, s);
	}

	frontToBack(0, _drawList);
	for (uint i = 0; i < _drawList.size(); i++) {
		if (_drawList[i] == 0)
			continue;
		if (!_order.empty())
			_order += ", ";
		_order += _sprites[_drawList[i]].label;
	}
	debugC(2, kDebugGraphics, "room %s draw order, front to back: %s", name.c_str(), _order.c_str());
	return true;
}

bool Room::tick() {
	// 0x431130 ticks the tree front to back; each sprite's player is
	// independent, so the order doesn't change the result.
	bool changed = false;
	for (uint i = 0; i < _sprites.size(); i++) {
		Sprite &s = _sprites[i];
		Player &p = s.player;
		if (!s.animated || p.mode == 0 || --p.countdown != 0)
			continue;

		// 0x430890
		p.countdown = p.ticksPerStep;
		const uint32 before = p.step;
		switch (p.mode) {
		case 1:
			if (++p.step >= p.steps)
				p.step = 0;
			break;
		case 2:
			p.step = p.step != 0 ? p.step - 1 : p.steps - 1;
			break;
		case 3:
			if (++p.step >= p.steps) {
				p.mode = 0;
				p.step = p.steps - 1;
			}
			break;
		case 4:
			if (p.step == 0)
				p.mode = 0;
			else
				p.step--;
			break;
		default:
			break;
		}
		changed = changed || p.step != before;
	}
	return changed;
}

uint32 Room::getFrame(const Sprite &s) const {
	if (!s.animated)
		return s.frame;
	const Common::Array<byte> &object = s.block->data;
	const uint32 ref = READ_LE_UINT32(&object[s.player.record + kSequenceHeaderSize + kStepSize * s.player.step + kStepFrameRef]);
	return READ_LE_UINT32(&object[READ_LE_UINT32(&object[kObjectFrameTable]) + 4 * (ref & 0x3FFFF)]);
}

const Cel *Room::getCel(Block &block, uint32 offset) {
	Common::SharedPtr<Cel> cel;
	if (!block.cels.tryGetVal(offset, cel)) {
		cel = Common::SharedPtr<Cel>(new Cel());
		if (!cel->load(block.data.data(), block.data.size(), offset))
			cel.reset();
		block.cels[offset] = cel;
	}
	return cel.get();
}

void Room::paint(const Sprite &s, Graphics::Surface &out) {
	Block &block = *s.block;
	const uint32 frame = getFrame(s);
	const uint n = READ_LE_UINT16(&block.data[frame + 4]);
	for (uint k = 0; k < n; k++) {
		const byte *entry = &block.data[frame + kFrameHeaderSize + kFrameEntrySize * k];
		const Cel *cel = getCel(block, READ_LE_UINT32(entry + 4));
		if (cel)
			cel->draw(out, s.x + (int16)READ_LE_UINT16(entry), s.y + (int16)READ_LE_UINT16(entry + 2));
	}
}

void Room::draw(Graphics::Surface &out) {
	if (out.w != _width || out.h != _height) {
		out.free();
		out.create(_width, _height, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	}
	out.fillRect(Common::Rect(out.w, out.h), 0);

	// The original draws the list front to back with occlusion and blends
	// translucent pixels back to front; painting it back to front gives the
	// same pixels.
	for (int i = (int)_drawList.size() - 1; i >= 0; i--) {
		const Sprite &s = _sprites[_drawList[i]];
		if (s.block)
			paint(s, out);
	}
}

} // End of namespace Scooby
