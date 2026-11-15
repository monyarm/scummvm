MODULE := engines/scooby

MODULE_OBJS = \
	art_archive.o \
	cel.o \
	character.o \
	inventory.o \
	menu_rooms.o \
	profile.o \
	room_class.o \
	room.o \
	room_script.o \
	scooby.o \
	sound.o \
	sound_archive.o \
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
