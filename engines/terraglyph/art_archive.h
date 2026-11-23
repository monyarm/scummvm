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

#ifndef TERRAGLYPH_ART_ARCHIVE_H
#define TERRAGLYPH_ART_ARCHIVE_H

#include "common/archive.h"
#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/path.h"
#include "common/str.h"

namespace Terraglyph {

/**
 * Type tag in the top nibble of a TGIFILE.ART name id.
 */
enum ArtNameType {
	kArtRoom = 1,   // id & 0x0FFFFFFF = room index
	kArtObject = 2, // id & 0x0FFFFFFF = object index
	kArtAnim = 3    // (owner object index & 0xFF) << 20 | sequence index
};

struct ArtName {
	uint32 id;
	Common::String name;

	ArtNameType getType() const { return (ArtNameType)(id >> 28); }
	uint32 getIndex() const { return id & 0x0FFFFFFF; }
};

/**
 * The Gen 1 art file, TGIFILE.ART (Showdown in Ghost Town, Phantom of the
 * Knight). It holds one block per room and one per object, each found
 * through a sector descriptor and named in the file's name table.
 *
 * Room and object names are the archive's members; each member's stream is
 * the block's whole sector span, as the original engine reads it. ANIM names
 * select a sequence inside an object and are not members, but can be looked
 * up with findName().
 */
class ArtArchive : public Common::Archive {
public:
	ArtArchive();
	~ArtArchive() override;

	/**
	 * Read and check the header and name table. Returns false, after a
	 * warning, if the file is missing or malformed.
	 */
	bool open(const Common::Path &filename);
	void close();
	bool isOpen() const { return !_filename.empty(); }

	// Common::Archive API
	bool hasFile(const Common::Path &path) const override;
	int listMembers(Common::ArchiveMemberList &list) const override;
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override;
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override;

	uint getRoomCount() const { return _rooms.size(); }
	uint getObjectCount() const { return _objects.size(); }
	const Common::Array<ArtName> &getNames() const { return _names; }

	/** Look up any name (ROOM, OBJ or ANIM), ignoring case. */
	bool findName(const Common::String &name, uint32 &id) const;

	/**
	 * File offset and size of a room or object block, by name. Returns false
	 * for unknown names and for ANIM names.
	 */
	bool getSpan(const Common::String &name, uint32 &offset, uint32 &size) const;

	/** Read a whole room or object block into memory. */
	bool readBlock(const Common::String &name, Common::Array<byte> &data) const;

	/** Name of a room or object block by index, or an empty string. */
	Common::String getRoomName(uint index) const;
	Common::String getObjectName(uint index) const;

private:
	struct Block {
		uint32 offset;
		uint32 size;
		Common::String name;
	};

	bool readBlocks(Common::SeekableReadStream &file, uint32 fileSize, const char *kind,
	                Common::Array<Block> &blocks);
	const Block *findBlock(const Common::String &name) const;
	void checkBlocks() const;

	Common::Path _filename;
	Common::Array<Block> _rooms;
	Common::Array<Block> _objects;
	Common::Array<ArtName> _names;
	Common::HashMap<Common::String, uint32, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _ids;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_ART_ARCHIVE_H
