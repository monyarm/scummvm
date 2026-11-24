MODULE := engines/terraglyph

MODULE_OBJS = \
	rooms/common/accusation.o \
	rooms/showdown/alley.o \
	rooms/phantomknight/arena_check.o \
	rooms/phantomknight/arena.o \
	rooms/phantomknight/armory_check.o \
	rooms/phantomknight/armory.o \
	formats/art_archive.o \
	rooms/phantomknight/below_balcony_check.o \
	rooms/phantomknight/below_balcony.o \
	rooms/showdown/boot_hill.o \
	rooms/phantomknight/brad_check.o \
	rooms/phantomknight/brad.o \
	formats/cel.o \
	rooms/showdown/chase_check.o \
	rooms/showdown/chase.o \
	rooms/showdown/corral.o \
	character.o \
	clues.o \
	rooms/phantomknight/dining_check.o \
	rooms/phantomknight/dining.o \
	rooms/phantomknight/doors_check.o \
	rooms/phantomknight/doors.o \
	rooms/phantomknight/ewan_check.o \
	rooms/phantomknight/ewan.o \
	rooms/phantomknight/drawbridge_check.o \
	rooms/phantomknight/drawbridge.o \
	rooms/phantomknight/gallery_check.o \
	rooms/phantomknight/gallery.o \
	rooms/phantomknight/garden_check.o \
	rooms/phantomknight/garden.o \
	rooms/phantomknight/ghost_spot_check.o \
	rooms/common/ghost_spot.o \
	rooms/showdown/gummy.o \
	rooms/showdown/hotel.o \
	rooms/idle_check.o \
	graphics/inventory.o \
	rooms/showdown/jail.o \
	rooms/phantomknight/kitchen_check.o \
	rooms/phantomknight/kitchen.o \
	rooms/phantomknight/kst_check.o \
	rooms/phantomknight/kst.o \
	rooms/phantomknight/maze_check.o \
	rooms/phantomknight/maze_interior_check.o \
	rooms/phantomknight/maze_interior.o \
	rooms/phantomknight/maze.o \
	rooms/phantomknight/maria_check.o \
	rooms/phantomknight/maria.o \
	rooms/common/menu.o \
	rooms/showdown/office.o \
	rooms/phantomknight/passage_check.o \
	rooms/phantomknight/passage.o \
	rooms/common/options.o \
	rooms/showdown/post.o \
	rooms/showdown/pienoon.o \
	rooms/phantomknight/princess_check.o \
	rooms/phantomknight/princess.o \
	profile.o \
	rooms/room_class.o \
	graphics/room.o \
	rooms/room_script.o \
	rooms/common/setup/room_setup.o \
	rooms/showdown/saloon.o \
	terraglyph.o \
	rooms/showdown/sheriff.o \
	rooms/phantomknight/souvenir_check.o \
	rooms/phantomknight/souvenir.o \
	audio/sound.o \
	rooms/common/signin.o \
	formats/sound_archive.o \
	rooms/showdown/strongbox.o \
	rooms/showdown/stage.o \
	rooms/phantomknight/throne_check.o \
	rooms/phantomknight/throne.o \
	rooms/showdown/station.o \
	rooms/phantomknight/storage_check.o \
	rooms/phantomknight/storage.o \
	rooms/showdown/store.o \
	rooms/showdown/story_check.o \
	rooms/showdown/story.o \
	toolbar.o \
	rooms/showdown/tunnel.o \
	rooms/showdown/vault.o \
	rooms/phantomknight/wizards_bedroom_check.o \
	rooms/phantomknight/wizards_bedroom.o \
	rooms/phantomknight/wizards_check.o \
	rooms/phantomknight/wizards.o \
	console.o \
	metaengine.o \
	movies/movie.o

# This module can be built as a plugin
ifeq ($(ENABLE_TERRAGLYPH), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
