# Stargus - Starcraft data extraction tool & launcher for the Stratagus engine
# Minimal Makefile build (no meson, no cmake, no ffmpeg, no ImageMagick).

CXX      ?= c++
CC       ?= cc
AR       ?= ar
CXXFLAGS ?= -O2 -g -std=c++17
CFLAGS   ?= -O2 -g
PREFIX   ?= /usr/local

PKG_CONFIG ?= pkg-config

# StormLib has no pkg-config file; locate it under common Homebrew prefixes.
STORM_PREFIX ?= $(shell \
	if [ -d /opt/homebrew/opt/stormlib ]; then echo /opt/homebrew/opt/stormlib; \
	elif [ -d /usr/local/opt/stormlib ]; then echo /usr/local/opt/stormlib; \
	elif [ -d /opt/local ]; then echo /opt/local; \
	else echo /usr/local; fi)

STORM_CFLAGS := -I$(STORM_PREFIX)/include
STORM_LIBS   := -L$(STORM_PREFIX)/lib -lstorm

PNG_CFLAGS  := $(shell $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS    := $(shell $(PKG_CONFIG) --libs libpng 2>/dev/null)
ZLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags zlib 2>/dev/null)
ZLIB_LIBS   := $(shell $(PKG_CONFIG) --libs zlib 2>/dev/null)
SDL_CFLAGS  := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS    := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)

OBJDIR := build/obj

STRATAGUS_DIR := stratagus
THIRD_PARTY   := $(STRATAGUS_DIR)/third-party
LUA_DIR       := $(THIRD_PARTY)/lua-5.1.5
LUA_SRC_DIR   := $(LUA_DIR)/src
TOLUA_DIR     := $(LUA_DIR)/toluapp-simple
ENGINE_BUILDDIR := build/engine
THIRD_PARTY_SENTINEL := $(TOLUA_DIR)/CMakeLists.txt

STRATAGUS_MAJOR := $(shell sed -n 's/set(STRATAGUS_MAJOR_VERSION \([0-9]*\))/\1/p' $(STRATAGUS_DIR)/CMakeLists.txt 2>/dev/null)
STRATAGUS_MINOR := $(shell sed -n 's/set(STRATAGUS_MINOR_VERSION \([0-9]*\))/\1/p' $(STRATAGUS_DIR)/CMakeLists.txt 2>/dev/null)
STRATAGUS_PATCH := $(shell sed -n 's/set(STRATAGUS_PATCH_LEVEL \([0-9]*\))/\1/p' $(STRATAGUS_DIR)/CMakeLists.txt 2>/dev/null)
STRATAGUS_PATCH2 := $(shell sed -n 's/set(STRATAGUS_PATCH_LEVEL2 \([0-9]*\))/\1/p' $(STRATAGUS_DIR)/CMakeLists.txt 2>/dev/null)

LUA_SOURCES := $(filter-out $(LUA_SRC_DIR)/lua.c $(LUA_SRC_DIR)/luac.c $(LUA_SRC_DIR)/print.c,$(wildcard $(LUA_SRC_DIR)/*.c))
LUA_OBJECTS := $(LUA_SOURCES:$(LUA_SRC_DIR)/%.c=$(ENGINE_BUILDDIR)/lua/%.o)
LUA_LIBRARY := $(ENGINE_BUILDDIR)/liblua51.a

TOLUA_RUNTIME_SOURCES := $(TOLUA_DIR)/tolua_event.c $(TOLUA_DIR)/tolua_is.c \
                         $(TOLUA_DIR)/tolua_map.c $(TOLUA_DIR)/tolua_push.c \
                         $(TOLUA_DIR)/tolua_to.c
TOLUA_RUNTIME_OBJECTS := $(TOLUA_RUNTIME_SOURCES:$(TOLUA_DIR)/%.c=$(ENGINE_BUILDDIR)/tolua/%.o)
TOLUA_LIBRARY := $(ENGINE_BUILDDIR)/libtoluapp51.a
TOLUA_APP_SOURCES := $(TOLUA_DIR)/tolua.c $(TOLUA_DIR)/toluabind.c
TOLUA_APP_OBJECTS := $(TOLUA_APP_SOURCES:$(TOLUA_DIR)/%.c=$(ENGINE_BUILDDIR)/tolua/%.o)
TOLUA_APP := $(ENGINE_BUILDDIR)/toluapp

TOLUA_PACKAGES := $(wildcard $(STRATAGUS_DIR)/src/tolua/*.pkg) \
                  $(STRATAGUS_DIR)/src/tolua/stratagus.lua
TOLUA_CPP := $(ENGINE_BUILDDIR)/tolua.cpp

# Keep this in step with Stratagus's vendored_guisan() source list. OpenGL is
# intentionally omitted there and here; Stratagus uses the SDL2 renderer.
GUISAN_SOURCES := $(shell find $(THIRD_PARTY)/guisan/src -name '*.cpp' \
                         ! -path '*/opengl/*' ! -name 'sdlimageloader.cpp' \
                         2>/dev/null | sort)
GUISAN_OBJECTS := $(GUISAN_SOURCES:$(THIRD_PARTY)/guisan/%.cpp=$(ENGINE_BUILDDIR)/guisan/%.o)

ENGINE_SOURCES := $(shell find $(STRATAGUS_DIR)/src -name '*.cpp' \
                         ! -path '*/beos/*' ! -path '*/win32/*' 2>/dev/null | sort)
ENGINE_OBJECTS := $(ENGINE_SOURCES:$(STRATAGUS_DIR)/%.cpp=$(ENGINE_BUILDDIR)/stratagus/%.o) \
                  $(ENGINE_BUILDDIR)/tolua.o $(ENGINE_BUILDDIR)/sdl_mixer.o
ENGINE := build/bin/stratagus

# All library sources (everything under src/ except the two executables).
SOURCES     := $(shell find src -name '*.cpp' | sort)
LIB_SOURCES := $(filter-out src/startool.cpp src/stargus.cpp,$(SOURCES))
LIB_OBJECTS := $(LIB_SOURCES:%.cpp=$(OBJDIR)/%.o)

STARTOOL_OBJ := $(OBJDIR)/src/startool.o
STARGUS_OBJ  := $(OBJDIR)/src/stargus.o

INCLUDES := -Isrc -Isrc/dat -Isrc/kaitai -Isrc/tileset -Isrc/libgrp \
            -Ibuild -Isubprojects/nlohmann_json/single_include \
            $(STORM_CFLAGS) $(PNG_CFLAGS) $(ZLIB_CFLAGS)
DEFINES  := -DHAVE_CONFIG_H -DKS_STR_ENCODING_NONE

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  ICONV_LIBS := -liconv
  ENGINE_PLATFORM_DEFINES := -DUSE_MAC -DHAVE_GETOPT -DHAVE_STRNLEN
else ifeq ($(UNAME_S),Linux)
  ENGINE_PLATFORM_DEFINES := -DUSE_LINUX -DHAVE_GETOPT -DHAVE_STRNLEN
  DL_LIBS := -ldl
endif

LIBS := $(STORM_LIBS) $(PNG_LIBS) $(ZLIB_LIBS) $(ICONV_LIBS)

ENGINE_INCLUDES := -Iengine -I$(STRATAGUS_DIR)/src/include -I$(ENGINE_BUILDDIR) \
                   -I$(LUA_SRC_DIR) -I$(TOLUA_DIR) \
                   -I$(THIRD_PARTY)/guisan/include \
                   -I$(THIRD_PARTY)/mdns \
                   -I$(THIRD_PARTY)/spiritless_po/include \
                   $(SDL_CFLAGS) $(PNG_CFLAGS) $(ZLIB_CFLAGS)
ENGINE_DEFINES := -DUSE_ZLIB -DDYNAMIC_LOAD $(ENGINE_PLATFORM_DEFINES) \
                  -DPIXMAPS='"$(PREFIX)/share/pixmaps"'
ENGINE_LIBS := $(SDL_LIBS) $(PNG_LIBS) $(ZLIB_LIBS) \
               $(ICONV_LIBS) $(DL_LIBS)

all: startool stargus engine

# Populate both nested submodules. This is explicit so builds from a source
# archive can still use pre-populated third-party sources without invoking git.
third-party: $(THIRD_PARTY_SENTINEL)

$(THIRD_PARTY_SENTINEL):
	git submodule update --init --recursive stratagus
	@test -f $@

lua: third-party
	$(MAKE) $(LUA_LIBRARY)

tolua++: third-party
	$(MAKE) $(TOLUA_APP) $(TOLUA_LIBRARY)

check-engine-deps:
	@test -n "$(SDL_LIBS)" && test -n "$(PNG_LIBS)" && test -n "$(ZLIB_LIBS)" || { \
		echo 'Missing engine dependencies: sdl2, libpng, and zlib must be visible to pkg-config (or their flags supplied to make).' >&2; \
		exit 1; \
	}

$(ENGINE_BUILDDIR)/lua/%.o: $(LUA_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fsigned-char -DLUA_USE_DLOPEN -DLUA_USE_MKSTEMP \
		-I$(LUA_SRC_DIR) -c $< -o $@

$(LUA_LIBRARY): $(LUA_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(ENGINE_BUILDDIR)/tolua/%.o: $(TOLUA_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fsigned-char -I$(LUA_SRC_DIR) -I$(TOLUA_DIR) -c $< -o $@

$(TOLUA_LIBRARY): $(TOLUA_RUNTIME_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(TOLUA_APP): $(TOLUA_APP_OBJECTS) $(TOLUA_RUNTIME_OBJECTS) $(LUA_LIBRARY)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(DL_LIBS)

$(TOLUA_CPP): $(TOLUA_APP) $(TOLUA_PACKAGES)
	@mkdir -p $(dir $@)
	cd $(STRATAGUS_DIR)/src/tolua && \
		$(abspath $(TOLUA_APP)) -L stratagus.lua \
		-o $(abspath $@) stratagus.pkg

$(ENGINE_BUILDDIR)/version-generated.h: $(STRATAGUS_DIR)/CMakeLists.txt
	@mkdir -p $(dir $@)
	@printf '%s\n' \
		'/* autogenerated by Makefile */' \
		'#define StratagusMajorVersion $(STRATAGUS_MAJOR)' \
		'#define StratagusMinorVersion $(STRATAGUS_MINOR)' \
		'#define StratagusPatchLevel $(STRATAGUS_PATCH)' \
		'#define StratagusPatchLevel2 $(STRATAGUS_PATCH2)' \
		'#define StratagusLastModifiedDate "unknown"' \
		'#define StratagusLastModifiedTime "unknown"' > $@

$(ENGINE_BUILDDIR)/stratagus/%.o: $(STRATAGUS_DIR)/%.cpp $(ENGINE_BUILDDIR)/version-generated.h engine/SDL_mixer.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fsigned-char $(ENGINE_DEFINES) $(ENGINE_INCLUDES) -c $< -o $@

$(ENGINE_BUILDDIR)/tolua.o: $(TOLUA_CPP) $(ENGINE_BUILDDIR)/version-generated.h
	$(CXX) $(CXXFLAGS) -fsigned-char $(ENGINE_DEFINES) $(ENGINE_INCLUDES) -c $< -o $@

$(ENGINE_BUILDDIR)/sdl_mixer.o: engine/sdl_mixer.cpp engine/SDL_mixer.h
	$(CXX) $(CXXFLAGS) -fsigned-char -Iengine $(SDL_CFLAGS) -c $< -o $@

$(ENGINE_BUILDDIR)/guisan/%.o: $(THIRD_PARTY)/guisan/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fsigned-char $(SDL_CFLAGS) \
		-I$(THIRD_PARTY)/guisan/include -c $< -o $@

$(ENGINE): $(ENGINE_OBJECTS) $(GUISAN_OBJECTS) $(TOLUA_LIBRARY) $(LUA_LIBRARY)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(ENGINE_LIBS)

engine: third-party check-engine-deps
	$(MAKE) $(ENGINE)

# Compile the extraction tool + library objects.
$(OBJDIR)/%.o: %.cpp build/config.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

# Launcher: needs the Stratagus game-launcher header and path macros.
$(STARGUS_OBJ): src/stargus.cpp $(THIRD_PARTY_SENTINEL)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Istratagus/gameheaders \
		-DDATA_PATH='"$(PREFIX)/share/games/stratagus/stargus"' \
		-DSCRIPTS_PATH='"$(PREFIX)/share/games/stratagus/stargus"' \
		-DSTRATAGUS_BIN='"stratagus"' \
		-DSOURCE_DIR='"$(CURDIR)"' \
		-c $< -o $@

startool: $(STARTOOL_OBJ) $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

stargus: $(STARGUS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/config.h:
	@mkdir -p build
	@echo '/* autogenerated by Makefile */' > $@
	@echo '#define PACKAGE_DATA_DIR "$(PREFIX)/share/stargus"' >> $@
	@echo '#define PACKAGE_SOURCE_DIR "$(CURDIR)"' >> $@

clean:
	rm -rf build startool stargus

.PHONY: all clean engine lua tolua++ third-party check-engine-deps
