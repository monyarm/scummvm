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
// u32 size, u16 n, u16 flags, then n 12-byte steps of (u32 unread, u32 frame
// reference, u32 polygon offset or 0). The frame reference's low 18 bits
// index table 1.
static const uint32 kObjectSequenceCount = 4;
static const uint32 kObjectSequenceTable = 8;
static const uint32 kObjectFrameCount = 12;
static const uint32 kObjectFrameTable = 16;
static const uint32 kSequenceSteps = 4;
static const uint32 kSequenceHeaderSize = 8;
static const uint32 kStepSize = 12;
static const uint32 kStepFrameRef = 4;
static const uint32 kStepPolygon = 8;

// A polygon is u32 size, u32 n, i16 x, i16 y, u16 w, u16 h (its box), then
// n x (i16 x, i16 y).
static const uint32 kPolygonHeaderSize = 16;

// A frame record is u32 size, u16 n, u16 0, then n x (i16 x, i16 y, u32 cel).
static const uint32 kFrameHeaderSize = 8;
static const uint32 kFrameEntrySize = 8;

// Sprite depths set on room entry (0x435DC0 and the placement handlers).
static const int kRootDepth = -0x7FFFFFFF;
static const int kLayerDepthBase = -110000;
static const int kLayerDepthStep = 10000;

// Every placement constructor passes 3 ticks per step and start step 0.
static const int kPlacementTicksPerStep = 3;

// Characters placed at depth y - 200 (the 20-name table at 0x46A720 in
// Showdown's Scooby.exe, 0x469720 in Phantom's; the same in both).
static const char *const kCharacters[] = {
	"NARRATOR", "DAPHNE", "FRED", "SCOOBY", "SHAGGY", "VELMA", "ARTIE", "JENNY",
	"SHERIFF", "ICDOUBLE", "GUMMY", "DONKEY", "BRAD", "EWAN", "MARIA", "JANE",
	"WIZARD", "JESTER", "PRINCESS", "UNICYCLE"
};

// The cursor is an OBJ_CURSOR sprite (0x4089E0).
static const char *const kCursorObject = "OBJ_CURSOR";

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

Room::Room(const ArtArchive &art, const Common::INIFile &objectIni) :
	_art(art), _ini(objectIni), _cursorShown(false), _width(0), _height(0) {
	_cursor.depth = 0;
	_cursor.frame = 0;
	_cursor.animated = false;
	_cursor.x = _cursor.y = 0;
	_cursor.hidden = false;
	_cursor.hitType = 1;
	for (uint slot = 0; slot < kRoomLayerCount; slot++)
		_layerSprites[slot] = -1;
}

Room::~Room() {
}

uint Room::addSprite(uint parent, const Sprite &sprite) {
	const uint index = _sprites.size();
	_sprites.push_back(sprite);
	insertChild(parent, index);
	return index;
}

void Room::insertChild(uint parent, uint index) {
	// 0x430F40: children stay sorted by descending depth, and a new child
	// goes after every child whose depth is at least its own.
	_sprites[index].parent = parent;
	Common::Array<uint> &children = _sprites[parent].children;
	uint k = 0;
	while (k < children.size() && _sprites[index].depth <= _sprites[children[k]].depth)
		k++;
	children.insert_at(k, index);
}

void Room::buildDrawList() {
	_drawList.clear();
	_order.clear();
	frontToBack(0, _drawList);
	for (uint i = 0; i < _drawList.size(); i++) {
		if (_drawList[i] == 0)
			continue;
		if (!_order.empty())
			_order += ", ";
		_order += _sprites[_drawList[i]].label;
	}
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

void Room::setActor(uint sprite, Actor *actor) {
	if (!_sprites[sprite].actor && actor)
		_actors++;
	else if (_sprites[sprite].actor && !actor)
		_actors--;
	_sprites[sprite].actor = actor;
}

int Room::nextSibling(uint child) const {
	const Common::Array<uint> &siblings = _sprites[_sprites[child].parent].children;
	for (uint k = 0; k + 1 < siblings.size(); k++) {
		if (siblings[k] == child)
			return siblings[k + 1];
	}
	return -1;
}

void Room::walkTick(uint index, bool &changed) {
	// 0x431130 in 0x431000's order: the children at or above this sprite's
	// depth, then the sprite (its player +0x08, its update +0x0C), then the
	// rest; a paused subtree is skipped. As in the original's list, each
	// child's next sibling is read before the child is walked: one added
	// right after it is not reached this tick, one added further on is.
	if (_sprites[index].paused)
		return;
	int cur = _sprites[index].children.empty() ? -1 : (int)_sprites[index].children[0];
	while (cur >= 0 && _sprites[index].depth <= _sprites[cur].depth) {
		const int next = nextSibling(cur);
		walkTick(cur, changed);
		cur = next;
	}
	changed = tickPlayer(_sprites[index]) || changed;
	if (_sprites[index].actor)
		_sprites[index].actor->actorUpdate(index);
	while (cur >= 0) {
		const int next = nextSibling(cur);
		walkTick(cur, changed);
		cur = next;
	}
}

void Room::detachSprite(uint sprite) {
	Sprite &s = _sprites[sprite];
	if (s.detached || sprite == 0)
		return;
	Common::Array<uint> &siblings = _sprites[s.parent].children;
	for (uint k = 0; k < siblings.size(); k++) {
		if (siblings[k] == sprite) {
			siblings.remove_at(k);
			break;
		}
	}
	if (s.actor) {
		s.actor = nullptr;
		_actors--;
	}
	s.detached = true;
	s.hidden = true;
	buildDrawList();
}

bool Room::hitsPoint(uint sprite, int x, int y) {
	Sprite &s = _sprites[sprite];
	if (s.detached || s.hidden || s.hitType == 0)
		return false;
	return s.hitType == 2 ? hitPolygon(s, x, y) : hitCels(s, x, y);
}

bool Room::getFrameRect(uint sprite, int &x, int &y, int &width, int &height) {
	x = y = width = height = 0;
	Sprite &s = _sprites[sprite];
	if (!s.block)
		return false;
	const uint32 frame = getFrame(s);
	if (!frame || !READ_LE_UINT16(&s.block->data[frame + 4]))
		return false;
	const byte *entry = &s.block->data[frame + kFrameHeaderSize];
	const Cel *cel = getCel(*s.block, READ_LE_UINT32(entry + 4));
	if (!cel)
		return false;
	x = s.x + (int16)READ_LE_UINT16(entry);
	y = s.y + (int16)READ_LE_UINT16(entry + 2);
	width = cel->getWidth();
	height = cel->getHeight();
	return true;
}

int Room::getStepCelWidth(uint sprite, uint step) {
	Sprite &s = _sprites[sprite];
	if (!s.animated || !s.block)
		return 0;
	const uint32 saved = s.player.step;
	s.player.step = step;
	int width, height;
	getFirstCelSize(sprite, width, height);
	s.player.step = saved;
	return width;
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

Common::String Room::getIniString(const Common::String &section, const char *key) const {
	// 0x40AB80, with an empty default.
	Common::String value;
	if (!_ini.getKey(key, section, value))
		return Common::String();
	value.trim();
	return value;
}

Room::Kind Room::getPlacementKind(const Common::String &obj, int y, int &slot, int &depth) const {
	// The default placement path (0x4091FF, 0x408F90, 0x408DC0, 0x401000).
	Common::String kind;
	if (!_ini.getKey("ID", obj, kind))
		kind = "simpleobject";
	kind.trim();
	kind.toLowercase();

	if (kind == "inventoryobject") {
		slot = 0;
		depth = -100;
		return kKindInventory;
	}
	if (kind == "requiresobject") {
		slot = 0;
		depth = 0;
		return kKindRequires;
	}
	if (kind == "exitobject") {
		slot = 0;
		depth = -1000;
		return kKindExit;
	}
	if (kind == "clickableobject") {
		const int layer = getIniInt(obj, "layer");
		slot = layer == -1 ? 0 : layer;
		depth = getIniInt(obj, "priority");
		return kKindClickable;
	}

	// A character, as OBJ_<NAME> or OBJ_<NAME>_<x>, or the ghost, goes in
	// front of layer 0 by its feet; anything else goes under layer 1.
	Common::String stem = obj;
	if (stem.size() > 2 && stem[stem.size() - 2] == '_')
		stem = Common::String(stem.c_str(), stem.size() - 2);
	for (uint i = 0; i < ARRAYSIZE(kCharacters); i++) {
		if (stem == Common::String("OBJ_") + kCharacters[i]) {
			slot = 0;
			depth = y - 200;
			return kKindCharacter;
		}
	}
	if (obj == "OBJ_GHOST") {
		slot = 0;
		depth = y - 200;
		return kKindGhost;
	}
	slot = 1;
	depth = 0;
	return kKindSimple;
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
	// Check the whole sequence once, so that ticking, drawing and hit tests
	// can't step outside the block.
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
		const uint32 step = p.record + kSequenceHeaderSize + kStepSize * k;
		const uint32 ref = READ_LE_UINT32(&object[step + kStepFrameRef]);
		if ((ref & 0x3FFFF) >= frameCount ||
		    !isFrameRecord(object, READ_LE_UINT32(&object[frameTable + 4 * (ref & 0x3FFFF)])))
			return false;
		const uint32 polygon = READ_LE_UINT32(&object[step + kStepPolygon]);
		if (polygon && (!inBlock(object, polygon, kPolygonHeaderSize) ||
		                !inBlock(object, polygon + kPolygonHeaderSize, 4 * READ_LE_UINT16(&object[polygon + 4]))))
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
	_actors = 0;
	_drawList.clear();
	_order.clear();
	_objects.clear();
	_characters.clear();

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
	root.hidden = false;
	root.hitType = 1;
	root.parent = 0;
	root.label = "root";
	_sprites.push_back(root);

	// One sprite per used layer slot, slot 0 furthest back. Layers are the
	// base sprite class, so their hit test is type 1.
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
		s.hidden = false;
		s.hitType = 1;
		s.label = Common::String::format("layer %u", slot);
		layers[slot] = addSprite(0, s);
	}
	for (uint slot = 0; slot < kRoomLayerCount; slot++)
		_layerSprites[slot] = layers[slot];

	// The class's own buffer sprite, made at its first claim (0x4370D0, 640
	// by 480 at (0, 0)), before any placement is built.
	int group = -1;
	int groupDepth = 0;
	if (_hook && _hook->wantsGroup(groupDepth) && layers[0] >= 0) {
		Sprite g;
		g.depth = groupDepth;
		g.frame = 0;
		g.animated = false;
		g.x = g.y = g.offsetX = g.offsetY = 0;
		g.hidden = false;
		g.hitType = 1;
		g.kind = kKindEngine;
		g.label = "render entity";
		g.player.mode = 0;
		g.player.step = g.player.steps = 0;
		group = addSprite(layers[0], g);
	}

	// One sprite per placement, started as its kind's constructor starts it.
	const uint32 count = READ_LE_UINT32(&b[kRoomPlacementCount]);
	const uint32 first = READ_LE_UINT32(&b[kRoomPlacementFirst]);
	if (first > b.size() || (b.size() - first) / kPlacementSize < count) {
		warning("Room %s: placements are outside its block", name.c_str());
		return false;
	}
	for (uint32 k = 0; k < count; k++) {
		const byte *p = &b[first + kPlacementSize * k];
		const Common::String obj = readName(p);
		Common::String anim = readName(p + kPlacementName);

		Sprite s;
		s.frame = 0;
		s.animated = false;
		s.x = (int16)READ_LE_UINT16(p + kPlacementX);
		s.y = (int16)READ_LE_UINT16(p + kPlacementY);
		s.offsetX = s.x; // layers sit at (0, 0)
		s.offsetY = s.y;
		s.label = Common::String::format("%u %s", k, obj.c_str());
		s.object = obj;
		s.anim = anim;
		int slot;
		Kind kind = getPlacementKind(obj, s.y, slot, s.depth);
		// The room's class sees it first (room vtable +0x18): it may build it
		// itself, as another kind, under another layer, or not at all.
		Claim claim;
		claim.placedAnim = anim;
		claim.x = s.x;
		claim.y = s.y;
		if (_hook)
			_hook->claimPlacement(obj, claim);
		if (claim.build == Claim::kSkip) {
			debugC(1, kDebugScript, "room %s: placement %u (%s) claimed, not built", name.c_str(), k, obj.c_str());
			continue;
		}
		if (claim.build == Claim::kRequires || claim.build == Claim::kClickable) {
			const Kind claimed = claim.build == Claim::kRequires ? kKindRequires : kKindClickable;
			// 0x408C80 builds a requires object under game +0x58 (layer 0)
			// unless told; 0x408DC0 a clickable under layer= unless told.
			if (claim.layer >= 0)
				slot = claim.layer;
			else if (claimed != kind)
				slot = 0;
			kind = claimed;
			s.depth = claim.depth;
			s.required = claim.required;
			if (!claim.anim.empty())
				anim = s.anim = claim.anim;
			debugC(1, kDebugScript, "room %s: placement %u (%s) claimed as a %s in layer %d, depth %d%s", name.c_str(), k,
			       obj.c_str(), claimed == kKindRequires ? "requires object" : "clickable", slot, s.depth,
			       claim.hidden ? ", hidden" : "");
		}
		if (claim.build == Claim::kSprite) {
			// graphics_AnimSprite_ctor, as the class calls it: its parent layer,
			// its depth, looping at 3 ticks a step, its hit test.
			kind = kKindSimple;
			if (claim.layer >= 0)
				slot = claim.layer;
			s.depth = claim.depth;
			if (!claim.anim.empty())
				anim = s.anim = claim.anim;
			debugC(1, kDebugScript, "room %s: placement %u (%s) claimed as a sprite playing %s in layer %d, depth %d, hit %d%s", name.c_str(), k,
			       obj.c_str(), anim.c_str(), slot, s.depth, claim.hitType, claim.hidden ? ", hidden" : "");
		}
		s.kind = kind;

		// Start modes: the clickable constructor (0x401000) and the ghost
		// (0x4091FF) start stopped; the others loop. Exits, requires,
		// inventory, clickables and simple sprites are hit by their step
		// polygon (type 2); characters and the ghost by their cels.
		const int mode = (kind == kKindClickable || kind == kKindGhost) ? 0 : 1;
		s.hidden = kind == kKindGhost || claim.hidden;
		s.hitType = (kind == kKindCharacter || kind == kKindGhost) ? 1 : 2;
		if (claim.build == Claim::kSprite)
			s.hitType = claim.hitType;
		if (kind == kKindExit) {
			s.rollover = getIniString(obj, "rolloveranim");
			s.destination = getIniString(obj, "destinationroom");
		} else if (kind == kKindClickable) {
			s.rollover = "ANIM_CURSOROPEN";
		}

		uint32 objId, animId;
		if (!_art.findName(obj, objId) || (ArtNameType)(objId >> 28) != kArtObject ||
		    !_art.findName(anim, animId) || (ArtNameType)(animId >> 28) != kArtAnim ||
		    !(s.block = getObjectBlock(obj)) ||
		    !startSequence(s, animId & 0x3FFFF, mode, kPlacementTicksPerStep, 0)) {
			warning("Room %s: placement %u (%s, %s) is not drawable", name.c_str(), k, obj.c_str(), anim.c_str());
			return false;
		}

		// A sprite whose layer slot is empty attaches to the root.
		uint parent = (slot >= 0 && slot < (int)kRoomLayerCount && layers[slot] >= 0) ? layers[slot] : 0;
		if (claim.group && group >= 0)
			parent = group;
		const uint index = addSprite(parent, s);
		if (kind == kKindCharacter)
			_characters.push_back(index);
	}

	buildDrawList();
	debugC(2, kDebugGraphics, "room %s draw order, front to back: %s", name.c_str(), _order.c_str());

	// Restart the cursor's sequence with the room's fresh block cache.
	if (_cursorShown) {
		const Common::String anim = _cursorAnim;
		_cursorAnim.clear();
		setCursor(anim);
	}
	return true;
}

bool Room::setSequence(uint sprite, const Common::String &obj, const Common::String &anim, int mode, int startStep) {
	Sprite &s = _sprites[sprite];
	uint32 id;
	if (!obj.empty() && !obj.equalsIgnoreCase(s.object) && _art.findName(obj, id) && (ArtNameType)(id >> 28) == kArtObject) {
		BlockPtr block = getObjectBlock(obj);
		if (block) {
			debugC(2, kDebugScript, "Switching to object %s from object %s", obj.c_str(), s.object.c_str());
			s.block = block;
			s.object = obj;
			s.animated = false;
		}
	}
	if (s.block && _art.findName(anim, id) && (ArtNameType)(id >> 28) == kArtAnim &&
	    startSequence(s, id & 0x3FFFF, mode, kPlacementTicksPerStep, startStep)) {
		debugC(2, kDebugScript, "Switching to ANIM %s", anim.c_str());
		s.anim = anim;
		return true;
	}

	// 0x430AA0 with mode 0 and start step -1: stopped on step 0.
	debugC(1, kDebugScript, "Setting %s to %s failed", s.object.c_str(), anim.c_str());
	s.player.mode = 0;
	s.player.countdown = kPlacementTicksPerStep;
	s.player.step = 0;
	return false;
}

int Room::findGhost() const {
	for (uint i = 0; i < _sprites.size(); i++) {
		if (_sprites[i].kind == kKindGhost)
			return i;
	}
	return -1;
}

bool Room::hasAnim(const Common::String &anim) const {
	uint32 id;
	return _art.findName(anim, id) && (ArtNameType)(id >> 28) == kArtAnim;
}

bool Room::startAnim(uint sprite, const Common::String &anim, int mode, int startStep) {
	Sprite &s = _sprites[sprite];
	uint32 id;
	if (!_art.findName(anim, id) || (ArtNameType)(id >> 28) != kArtAnim) {
		// 0x430A00 stores id 0 (+0x50) and starts nothing. SelectFrame
		// (0x4309C0) then picks no frame and the hit test (0x430D10) fails:
		// the sprite is neither drawn nor hit until another ANIM starts.
		debugC(1, kDebugScript, "Room %s: %s has no ANIM %s; it shows nothing", _name.c_str(), s.object.c_str(), anim.c_str());
		s.animated = false;
		s.frame = 0;
		s.anim = anim;
		return false;
	}
	if (!s.block || !startSequence(s, id & 0x3FFFF, mode, kPlacementTicksPerStep, startStep)) {
		warning("Room %s: cannot start %s on %s", _name.c_str(), anim.c_str(), s.object.c_str());
		return false;
	}
	s.anim = anim;
	return true;
}

int Room::getSequenceSteps(const Common::String &obj, const Common::String &anim) {
	uint32 objId, animId;
	Sprite s;
	s.animated = false;
	if (!_art.findName(obj, objId) || (ArtNameType)(objId >> 28) != kArtObject ||
	    !_art.findName(anim, animId) || (ArtNameType)(animId >> 28) != kArtAnim ||
	    !(s.block = getObjectBlock(obj)) || !startSequence(s, animId & 0x3FFFF, 0, kPlacementTicksPerStep, 0))
		return -1;
	return s.player.steps;
}

void Room::setDepth(uint sprite, int depth) {
	// 0x430EE0: unlink from the parent's children, then insert again.
	Sprite &s = _sprites[sprite];
	Common::Array<uint> &siblings = _sprites[s.parent].children;
	for (uint k = 0; k < siblings.size(); k++) {
		if (siblings[k] == sprite) {
			siblings.remove_at(k);
			break;
		}
	}
	s.depth = depth;
	insertChild(s.parent, sprite);
	buildDrawList();
}

void Room::setStep(uint sprite, uint32 step) {
	Player &p = _sprites[sprite].player;
	if (_sprites[sprite].animated)
		p.step = MIN<uint32>(step, p.steps - 1);
}

void Room::setStepPast(uint sprite, uint32 step) {
	if (_sprites[sprite].animated)
		_sprites[sprite].player.step = step;
}

bool Room::tickPlayer(Sprite &s) {
	Player &p = s.player;
	if (!s.animated || p.mode == 0 || --p.countdown != 0)
		return false;

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
	return p.step != before;
}

bool Room::tick() {
	// 0x431130 ticks the tree front to back; each sprite's player is
	// independent, so the order doesn't change the result. Hidden sprites
	// still tick: only a subtree marked done (+0x30) is skipped. Then
	// 0x431190 deletes each one marked done, before the draw.
	bool changed = false;
	if (_actors) {
		walkTick(0, changed);
		for (uint i = 1; i < _sprites.size(); i++) {
			if (_sprites[i].paused && !_sprites[i].detached) {
				detachSprite(i);
				changed = true;
			}
		}
	} else {
		for (uint i = 0; i < _sprites.size(); i++)
			changed = tickPlayer(_sprites[i]) || changed;
	}
	if (_cursorShown)
		changed = tickPlayer(_cursor) || changed;
	return changed;
}

uint32 Room::getFrame(const Sprite &s) const {
	if (!s.animated)
		return s.frame;
	const Common::Array<byte> &object = s.block->data;
	const uint32 at = s.player.record + kSequenceHeaderSize + kStepSize * s.player.step + kStepFrameRef;
	if (s.player.step >= s.player.steps) {
		// 0x4309C0 reads a step past the last as any other: the frame
		// reference is whatever follows the sequence (Pie Noon's pies step one
		// past their last). Here only a frame that is really there is shown.
		if (!inBlock(object, at, 4))
			return 0;
		const uint32 index = READ_LE_UINT32(&object[at]) & 0x3FFFF;
		if (index >= READ_LE_UINT32(&object[kObjectFrameCount]))
			return 0;
		const uint32 frame = READ_LE_UINT32(&object[READ_LE_UINT32(&object[kObjectFrameTable]) + 4 * index]);
		return isFrameRecord(object, frame) ? frame : 0;
	}
	const uint32 ref = READ_LE_UINT32(&object[at]);
	return READ_LE_UINT32(&object[READ_LE_UINT32(&object[kObjectFrameTable]) + 4 * (ref & 0x3FFFF)]);
}

uint32 Room::getPolygon(const Sprite &s) const {
	if (!s.animated || s.player.step >= s.player.steps)
		return 0;
	const uint32 polygon = READ_LE_UINT32(&s.block->data[s.player.record + kSequenceHeaderSize + kStepSize * s.player.step + kStepPolygon]);
	if (!polygon && s.firstStepHit)
		return READ_LE_UINT32(&s.block->data[s.player.record + kSequenceHeaderSize + kStepPolygon]);
	return polygon;
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
	if (!frame)
		return; // no frame selected (an unknown ANIM)
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
	// same pixels. A hidden sprite (+0x14) isn't drawn (0x431000).
	for (int i = (int)_drawList.size() - 1; i >= 0; i--) {
		const Sprite &s = _sprites[_drawList[i]];
		if (s.block && !s.hidden)
			paint(s, out);
	}
	if (_cursorShown)
		paint(_cursor, out);
}

bool Room::hitPolygon(const Sprite &s, int px, int py) const {
	// 0x430D10 -> 0x431350 -> 0x4312A0, on the current step's polygon.
	const uint32 polygon = getPolygon(s);
	if (!polygon)
		return false;
	const byte *poly = &s.block->data[polygon];
	const int x = px - s.x;
	const int y = py - s.y;
	const int bx = (int16)READ_LE_UINT16(poly + 8);
	const int by = (int16)READ_LE_UINT16(poly + 10);
	if (!(x < bx + (int)READ_LE_UINT16(poly + 12) && bx < x && y < by + (int)READ_LE_UINT16(poly + 14) && by < y))
		return false;

	// Even-odd crossings, with C integer division as in the original.
	const uint n = READ_LE_UINT16(poly + 4);
	const byte *pts = poly + kPolygonHeaderSize;
	bool inside = false;
	for (uint i = 0, j = n - 1; i < n; j = i++) {
		const int xi = (int16)READ_LE_UINT16(pts + 4 * i), yi = (int16)READ_LE_UINT16(pts + 4 * i + 2);
		const int xj = (int16)READ_LE_UINT16(pts + 4 * j), yj = (int16)READ_LE_UINT16(pts + 4 * j + 2);
		if (((yi <= y && y < yj) || (yj <= y && y < yi)) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
			inside = !inside;
	}
	return inside;
}

bool Room::hitCels(const Sprite &s, int px, int py) {
	// 0x431400: any cel of the current frame whose pixel or masks cover the point.
	if (!s.block)
		return false;
	Block &block = *s.block;
	const uint32 frame = getFrame(s);
	if (!frame)
		return false;
	const uint n = READ_LE_UINT16(&block.data[frame + 4]);
	for (uint k = 0; k < n; k++) {
		const byte *entry = &block.data[frame + kFrameHeaderSize + kFrameEntrySize * k];
		const Cel *cel = getCel(block, READ_LE_UINT32(entry + 4));
		if (!cel)
			continue;
		const int cx = px - (s.x + (int16)READ_LE_UINT16(entry));
		const int cy = py - (s.y + (int16)READ_LE_UINT16(entry + 2));
		if (cx >= 0 && cy >= 0 && cx < cel->getWidth() && cy < cel->getHeight() && cel->hitTest(cx, cy))
			return true;
	}
	return false;
}

int Room::hitTest(int x, int y) {
	// 0x4311F0 walks the tree front to back, skipping hidden sprites; the
	// first hit wins.
	for (uint i = 0; i < _drawList.size(); i++) {
		const Sprite &s = _sprites[_drawList[i]];
		// Type 0 is never hit (0x4311F0 tests only types 1 and 2).
		if (s.hidden || s.hitType == 0)
			continue;
		if (s.hitType == 2 ? hitPolygon(s, x, y) : hitCels(s, x, y))
			return _drawList[i];
	}
	return -1;
}

uint Room::addEngineSprite(int parent, const Common::String &obj, const Common::String &anim, int depth,
                           int x, int y, int hitType, const Common::String &label) {
	Sprite s;
	s.depth = depth;
	s.frame = 0;
	s.animated = false;
	s.offsetX = x;
	s.offsetY = y;
	s.hidden = false;
	s.hitType = hitType;
	s.kind = kKindEngine;
	s.object = obj;
	s.label = label;
	s.block = getObjectBlock(obj);
	const uint p = parent < 0 ? 0 : parent;
	s.x = _sprites[p].x + x;
	s.y = _sprites[p].y + y;
	const uint index = addSprite(p, s);
	// 0x430A00: an unknown ANIM starts nothing and selects no frame.
	if (!s.block || !startAnim(index, anim, 1, 0))
		_sprites[index].anim = anim;
	buildDrawList();
	return index;
}

uint Room::addGroupSprite(int parent, int depth, int x, int y, const Common::String &label) {
	Sprite s;
	s.depth = depth;
	s.frame = 0;
	s.animated = false;
	s.offsetX = x;
	s.offsetY = y;
	s.hidden = false;
	s.hitType = 1;
	s.kind = kKindEngine;
	s.label = label;
	s.player.mode = 0;
	s.player.step = s.player.steps = 0;
	const uint p = parent < 0 ? 0 : parent;
	s.x = _sprites[p].x + x;
	s.y = _sprites[p].y + y;
	const uint index = addSprite(p, s);
	buildDrawList();
	return index;
}

void Room::setOffset(uint sprite, int x, int y) {
	_sprites[sprite].offsetX = x;
	_sprites[sprite].offsetY = y;
	placeSubtree(sprite);
}

void Room::placeSubtree(uint index) {
	// 0x431000 draws a child at its parent's position plus its own.
	Sprite &s = _sprites[index];
	if (index != 0) {
		s.x = _sprites[s.parent].x + s.offsetX;
		s.y = _sprites[s.parent].y + s.offsetY;
	}
	for (uint k = 0; k < s.children.size(); k++)
		placeSubtree(s.children[k]);
}

void Room::getSize(uint sprite, int &width, int &height) {
	// 0x430C00: every step's frame, every cel in it; the cel offsets are
	// not added.
	width = height = 0;
	Sprite &s = _sprites[sprite];
	if (!s.animated || !s.block)
		return;
	Block &block = *s.block;
	const Common::Array<byte> &object = block.data;
	const uint32 frameTable = READ_LE_UINT32(&object[kObjectFrameTable]);
	for (uint32 k = 0; k < s.player.steps; k++) {
		const uint32 ref = READ_LE_UINT32(&object[s.player.record + kSequenceHeaderSize + kStepSize * k + kStepFrameRef]);
		const uint32 frame = READ_LE_UINT32(&object[frameTable + 4 * (ref & 0x3FFFF)]);
		const uint n = READ_LE_UINT16(&object[frame + 4]);
		for (uint c = 0; c < n; c++) {
			const Cel *cel = getCel(block, READ_LE_UINT32(&object[frame + kFrameHeaderSize + kFrameEntrySize * c + 4]));
			if (!cel)
				continue;
			width = MAX<int>(width, cel->getWidth());
			height = MAX<int>(height, cel->getHeight());
		}
	}
}

int Room::getFirstCelWidth(uint sprite) {
	int width, height;
	getFirstCelSize(sprite, width, height);
	return width;
}

void Room::getFirstCelSize(uint sprite, int &width, int &height) {
	width = height = 0;
	Sprite &s = _sprites[sprite];
	if (!s.block)
		return;
	const uint32 frame = getFrame(s);
	if (!frame || !READ_LE_UINT16(&s.block->data[frame + 4]))
		return;
	const Cel *cel = getCel(*s.block, READ_LE_UINT32(&s.block->data[frame + kFrameHeaderSize + 4]));
	if (cel) {
		width = cel->getWidth();
		height = cel->getHeight();
	}
}

bool Room::hasStepPolygon(uint sprite) const {
	const Sprite &s = _sprites[sprite];
	if (!s.animated || !s.block)
		return false;
	for (uint32 k = 0; k < s.player.steps; k++) {
		if (READ_LE_UINT32(&s.block->data[s.player.record + kSequenceHeaderSize + kStepSize * k + kStepPolygon]))
			return true;
	}
	return false;
}

void Room::setPlayer(uint sprite, int mode, int startStep, int ticksPerStep) {
	// 0x430AA0 on the sequence the sprite has.
	Sprite &s = _sprites[sprite];
	if (!s.animated) {
		s.player.mode = 0;
		return;
	}
	Player &p = s.player;
	p.mode = mode;
	p.ticksPerStep = p.countdown = ticksPerStep;
	if (mode == 5) {
		p.mode = 0;
		p.step = p.steps - 1;
	} else if (startStep == -1) {
		if (mode == 1 || mode == 3)
			p.step = 0;
		else if (mode == 2 || mode == 4)
			p.step = p.steps - 1;
	} else {
		p.step = (uint32)startStep < p.steps ? startStep : p.steps - 1;
	}
}

bool Room::setCursor(const Common::String &anim) {
	if (anim.empty()) {
		_cursorShown = false;
		_cursorAnim.clear();
		return true;
	}
	if (_cursorShown && anim == _cursorAnim)
		return true;

	// The cursor plays its sequence in mode 1 at 3 ticks per step (an
	// inference: 0x4089E0's own start call is not traced).
	uint32 animId;
	Sprite c;
	c.frame = 0;
	c.animated = false;
	c.hidden = false;
	c.hitType = 1;
	c.depth = 0;
	c.x = _cursor.x;
	c.y = _cursor.y;
	c.label = "cursor";
	if (!_art.findName(anim, animId) || (ArtNameType)(animId >> 28) != kArtAnim ||
	    !(c.block = getObjectBlock(kCursorObject)) ||
	    !startSequence(c, animId & 0x3FFFF, 1, kPlacementTicksPerStep, 0)) {
		warning("Cursor %s is not drawable", anim.c_str());
		return false;
	}
	_cursor = c;
	_cursorAnim = anim;
	_cursorShown = true;
	return true;
}

void Room::setCursorPosition(int x, int y) {
	_cursor.x = x;
	_cursor.y = y;
}

} // End of namespace Scooby
