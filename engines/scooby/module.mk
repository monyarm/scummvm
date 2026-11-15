MODULE := engines/scooby

MODULE_OBJS = \
	accusation_rooms.o \
	art_archive.o \
	cel.o \
	character.o \
	inventory.o \
	menu_rooms.o \
	options_rooms.o \
	profile.o \
	room_class.o \
	room.o \
	room_script.o \
	room_setup.o \
	scooby.o \
	sound.o \
	signin_room.o \
	sound_archive.o \
	story_check.o \
	story_rooms.o \
	toolbar_rooms.o \
	console.o \
	metaengine.o \
	movie.o

# This module can be built as a plugin
ifeq ($(ENABLE_SCOOBY), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
