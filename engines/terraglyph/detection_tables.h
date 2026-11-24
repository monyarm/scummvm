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

namespace Terraglyph {

const PlainGameDescriptor terraglyphGames[] = {
	{ "showdown", "Scooby-Doo! Showdown in Ghost Town" },
	{ "phantomknight", "Scooby-Doo! Phantom of the Knight" },
	{ "jinxsphinx", "Scooby-Doo! Jinx at the Sphinx" },
	{ "casefile1", "Scooby-Doo! Case File #1: The Glowing Bug Man" },
	{ "activitychallenge", "Scooby-Doo! Activity Challenge" },
	{ nullptr, nullptr }
};

// Every known release is a Windows disc with no MIDI, so those fields live in
// the base macro; the per-game macros name only what varies between releases.
#define TG_GAME_ENTRY(gameid, extra, files, lang, flags, gen) \
	{ { gameid, extra, files, lang, Common::kPlatformWindows, flags, GUIO1(GUIO_NOMIDI) }, gen }

// Showdown: only the executable, the art archive and the language vary. Its
// object.ini is byte-identical in the English and Norwegian discs; a disc that
// ships its own calls TG_GAME_ENTRY with AD_ENTRY3s instead of this macro.
#define SHOWDOWN_ENTRY(exeMd5, exeSize, artMd5, artSize, lang, flags) \
	TG_GAME_ENTRY("showdown", nullptr, \
	              AD_ENTRY3s("Scooby.exe", exeMd5, exeSize, \
	                         "TGIFILE.ART", artMd5, artSize, \
	                         "object.ini", "42db419bd6be75a49552d9e9bdeed880", 25196), \
	              lang, flags, kGen1)

// MD5s cover the first 5000 bytes (the detector default). Sizes are of the
// whole file. Values are from the English Windows discs unless a localized
// release is noted on the entry.
//
// Only Gen 1 is being implemented, so Gen 2 and Gen 3 entries are detected
// but marked unsupported until their archive readers exist.
const TerraglyphGameDescription gameDescriptions[] = {
	// Scooby-Doo! Showdown in Ghost Town
	// English Windows
	SHOWDOWN_ENTRY("a7a84eb0e66863bb699bab9fc75fdf2b", 487473,
	               "ba57da019a38f63b1b85d8cf83457377", 144592896,
	               Common::EN_ANY, ADGF_TESTING),
	// Norwegian (Bokmal) CD
	SHOWDOWN_ENTRY("2e06d1de95b854288f5fdb5cc05c3a8b", 1693471,
	               "ffdf0dfefca235966b0620649d4b27d8", 144852992,
	               Common::NB_NOR, ADGF_TESTING),

	// Scooby-Doo! Phantom of the Knight
	TG_GAME_ENTRY("phantomknight", nullptr,
	              AD_ENTRY3s("Scooby.exe", "c36473c3204c10d75eebf0241e35aee8", 483377,
	                         "TGIFILE.ART", "a0c28dd3e75241794cb19481496bd919", 114386944,
	                         "object.ini", "aafe695ae3f6c2db91858661e28e1c6c", 28788),
	              Common::EN_ANY, ADGF_UNSTABLE, kGen1),

	// Scooby-Doo! Jinx at the Sphinx
	TG_GAME_ENTRY("jinxsphinx", nullptr,
	              AD_ENTRY3s("Scooby.exe", "2df7a8e242d525e589abbd2372a80824", 847872,
	                         "Mummy.mms", "17d996eb0ed078a90ce9ab76357e81b9", 887171,
	                         "Mummy.MMP", "acd69eec416b74a5f2aade1c06e441ef", 107393012),
	              Common::EN_ANY, ADGF_UNSUPPORTED, kGen2),

	// Scooby-Doo! Case File #1: The Glowing Bug Man
	TG_GAME_ENTRY("casefile1", nullptr,
	              AD_ENTRY3s("Case File #1.exe", "00a2fd4530103cecb0b2f2684ce59129", 786432,
	                         "Museum.MMS", "f92430e7de3242a4fb7b9d8ec2ec6116", 787946,
	                         "MuseumCD.MMP", "1a906c898fc25a41c812bafd85f66b5e", 91589634),
	              Common::EN_ANY, ADGF_UNSUPPORTED, kGen3),

	// Scooby-Doo! Activity Challenge
	// The disc keeps Scooby.exe in INSTALL\HDINSTALL, so detection uses
	// the three archives at the disc root.
	TG_GAME_ENTRY("activitychallenge", nullptr,
	              AD_ENTRY3s("Bonus.mms", "b6708dba6c936d5df3355668bca45aa9", 289844,
	                         "BONUS.MMP", "2a96b23cb24e866d65ea3cd4939c78b1", 68733610,
	                         "BONUS.MMF", "35d13d927badf3d5f6177a003b072ac6", 19744128),
	              Common::EN_ANY, ADGF_UNSUPPORTED, kGen2),

	{ AD_TABLE_END_MARKER, kGen1 }
};

} // End of namespace Terraglyph
