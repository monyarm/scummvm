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
#include "common/file.h"
#include "common/substream.h"
#include "common/textconsole.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"

namespace Terraglyph {

// Layout, as read by the original engine's art-file constructor:
//   u32 room count,   then that many u32 room descriptors
//   u32 object count, then that many u32 object descriptors
//   u32 name count,   then that many 68-byte records (u32 id, char name[64])
// A descriptor is (start sector << 14) | sector count, in 2048-byte sectors.
static const uint32 kSectorSize = 2048;
static const uint32 kNameRecordSize = 68;
static const uint32 kNameSize = 64;

ArtArchive::ArtArchive() {
}

ArtArchive::~ArtArchive() {
	close();
}

void ArtArchive::close() {
	_filename.clear();
	_rooms.clear();
	_objects.clear();
	_names.clear();
	_ids.clear();
}

bool ArtArchive::readBlocks(Common::SeekableReadStream &file, uint32 fileSize, const char *kind,
                            Common::Array<Block> &blocks) {
	const uint32 count = file.readUint32LE();
	if (file.err() || file.eos() || count > (fileSize - (uint32)file.pos()) / 4) {
		warning("TGIFILE.ART: bad %s count %u", kind, count);
		return false;
	}

	blocks.resize(count);
	for (uint32 i = 0; i < count; i++) {
		const uint32 d = file.readUint32LE();
		Block &b = blocks[i];
		b.offset = (d >> 14) * kSectorSize;
		b.size = (d & 0x3FFF) * kSectorSize;
		if (b.size == 0 || b.offset > fileSize || b.size > fileSize - b.offset) {
			warning("TGIFILE.ART: %s %u descriptor 0x%08x is outside the file", kind, i, d);
			return false;
		}
	}
	return true;
}

bool ArtArchive::open(const Common::Path &filename) {
	close();

	Common::File file;
	if (!file.open(filename)) {
		warning("TGIFILE.ART: cannot open %s", filename.toString().c_str());
		return false;
	}
	const uint32 fileSize = file.size();

	if (!readBlocks(file, fileSize, "room", _rooms) || !readBlocks(file, fileSize, "object", _objects)) {
		close();
		return false;
	}

	const uint32 nameCount = file.readUint32LE();
	if (file.err() || file.eos() || nameCount > (fileSize - (uint32)file.pos()) / kNameRecordSize) {
		warning("TGIFILE.ART: bad name count %u", nameCount);
		close();
		return false;
	}

	_names.reserve(nameCount);
	char buf[kNameSize];
	for (uint32 i = 0; i < nameCount; i++) {
		ArtName n;
		n.id = file.readUint32LE();
		file.read(buf, kNameSize);
		if (file.err() || file.eos()) {
			warning("TGIFILE.ART: name table is truncated at record %u", i);
			close();
			return false;
		}

		// The name is a C string; the bytes after its NUL are never read.
		const char *nul = (const char *)memchr(buf, 0, kNameSize);
		if (!nul || nul == buf) {
			warning("TGIFILE.ART: name record %u has no valid name", i);
			close();
			return false;
		}
		n.name = Common::String(buf, nul - buf);

		Common::Array<Block> *blocks = nullptr;
		switch (n.getType()) {
		case kArtRoom:
			blocks = &_rooms;
			break;
		case kArtObject:
			blocks = &_objects;
			break;
		case kArtAnim:
			break;
		default:
			warning("TGIFILE.ART: name %s has unknown id 0x%08x", n.name.c_str(), n.id);
			close();
			return false;
		}

		if (blocks) {
			if (n.getIndex() >= blocks->size()) {
				warning("TGIFILE.ART: name %s has id 0x%08x beyond the descriptors", n.name.c_str(), n.id);
				close();
				return false;
			}
			(*blocks)[n.getIndex()].name = n.name;
		}

		if (_ids.contains(n.name))
			warning("TGIFILE.ART: duplicate name %s; keeping the first", n.name.c_str());
		else
			_ids[n.name] = n.id;
		_names.push_back(n);
	}

	_filename = filename;

	debugC(1, kDebugResource, "TGIFILE.ART: %u rooms, %u objects, %u names",
	       _rooms.size(), _objects.size(), _names.size());
	for (uint i = 0; i < _rooms.size(); i++)
		debugC(2, kDebugResource, "art room %u %s 0x%x %u",
		       i, _rooms[i].name.c_str(), _rooms[i].offset, _rooms[i].size);
	for (uint i = 0; i < _objects.size(); i++)
		debugC(2, kDebugResource, "art object %u %s 0x%x %u",
		       i, _objects[i].name.c_str(), _objects[i].offset, _objects[i].size);

	if (debugChannelSet(3, kDebugResource))
		checkBlocks();

	return true;
}

void ArtArchive::checkBlocks() const {
	// Each block starts with its own size, which must fit its sector span:
	// rooms fill the span exactly, objects end in its last sector. This
	// reads every member through the archive interface.
	Common::ArchiveMemberList members;
	listMembers(members);

	uint bad = 0;
	for (const Common::ArchiveMemberPtr &m : members) {
		Common::SeekableReadStream *s = m->createReadStream();
		const uint32 blockSize = s ? s->readUint32LE() : 0;
		const bool ok = s && !s->err() && blockSize <= s->size() && blockSize > s->size() - kSectorSize;
		debugC(3, kDebugResource, "art check %s %u of %u%s", m->getName().c_str(), blockSize,
		       s ? (uint32)s->size() : 0, ok ? "" : " BAD");
		if (!ok)
			bad++;
		delete s;
	}
	debugC(3, kDebugResource, "art check: %u blocks, %u bad", members.size(), bad);
}

bool ArtArchive::findName(const Common::String &name, uint32 &id) const {
	return _ids.tryGetVal(name, id);
}

const ArtArchive::Block *ArtArchive::findBlock(const Common::String &name) const {
	ArtName n;
	if (!findName(name, n.id))
		return nullptr;

	if (n.getType() == kArtRoom)
		return &_rooms[n.getIndex()];
	if (n.getType() == kArtObject)
		return &_objects[n.getIndex()];
	return nullptr;
}

bool ArtArchive::getSpan(const Common::String &name, uint32 &offset, uint32 &size) const {
	const Block *b = findBlock(name);
	if (!b)
		return false;
	offset = b->offset;
	size = b->size;
	return true;
}

bool ArtArchive::readBlock(const Common::String &name, Common::Array<byte> &data) const {
	Common::SeekableReadStream *s = createReadStreamForMember(Common::Path(name));
	if (!s)
		return false;

	data.resize(s->size());
	const bool ok = s->read(data.data(), data.size()) == data.size() && !s->err();
	delete s;
	if (!ok)
		warning("TGIFILE.ART: cannot read block %s", name.c_str());
	return ok;
}

Common::String ArtArchive::getRoomName(uint index) const {
	return index < _rooms.size() ? _rooms[index].name : Common::String();
}

Common::String ArtArchive::getObjectName(uint index) const {
	return index < _objects.size() ? _objects[index].name : Common::String();
}

bool ArtArchive::hasFile(const Common::Path &path) const {
	return findBlock(path.toString()) != nullptr;
}

int ArtArchive::listMembers(Common::ArchiveMemberList &list) const {
	int count = 0;
	for (const Block &b : _rooms) {
		if (!b.name.empty()) {
			list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(b.name, *this)));
			count++;
		}
	}
	for (const Block &b : _objects) {
		if (!b.name.empty()) {
			list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(b.name, *this)));
			count++;
		}
	}
	return count;
}

const Common::ArchiveMemberPtr ArtArchive::getMember(const Common::Path &path) const {
	if (!hasFile(path))
		return Common::ArchiveMemberPtr();
	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
}

Common::SeekableReadStream *ArtArchive::createReadStreamForMember(const Common::Path &path) const {
	const Block *b = findBlock(path.toString());
	if (!b)
		return nullptr;

	Common::File *file = new Common::File();
	if (!file->open(_filename)) {
		delete file;
		return nullptr;
	}
	return new Common::SeekableSubReadStream(file, b->offset, b->offset + b->size, DisposeAfterUse::YES);
}

} // End of namespace Terraglyph
