ifndef OS
	OS := $(shell uname -s)
endif

BRANCH ?= main

RELEASE_BUILD := 0
ifeq ($(RELEASE_BUILD),0)
VERSIONED_OPTION :=
else
VERSIONED_OPTION := --versioned
endif	

BUILD_VERSION := 0_1_$(shell cat version.txt)

DEMO_PROJECT_NAME := Godot3DMultiplayer

# MacOS
ifeq ($(OS),Darwin)
GODOT_BIN := /Applications/Godot.app/Contents/MacOS/Godot
TEMP_RESULT := $(shell mkdir -p ./dist/macOS)
DIST_DIR := $(realpath ./dist/macOS)
GODOT_PRESET := macOS
SCONS_PLATFORM := macos
EXPORT_EXT := dmg
# Linux
else ifeq ($(OS),Linux)
GODOT_BIN := godot
TEMP_RESULT := $(shell mkdir -p ./dist/linux)
DIST_DIR := $(shell realpath ./dist/linux)
GODOT_PRESET := Linux/Client
GODOT_PRESET_SERVER := Linux/Server
SCONS_PLATFORM := linux
EXPORT_EXT := run
# Windows
else ifeq ($(OS),Windows_NT)
ifdef GODOT_EXE
GODOT_BIN := $(GODOT_EXE)
else
GODOT_BIN := godot
endif
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))
DIST_DIR := $(subst /,\,$(mkfile_dir)dist/windows)
TEMP_RESULT := $(shell if not exist "$(DIST_DIR)" mkdir $(DIST_DIR))
#TEMP_RESULT := $(shell mkdir $(DIST_DIR))
GODOT_PRESET := Windows Desktop
SCONS_PLATFORM := windows
EXPORT_EXT := exe
# Unsupported OS
else
GODOT_BIN := godot
DIST_DIR := ./dist/$(OS)
endif

all: variables go gd

variables:
	@echo -----------------------------------------------------
	@echo OS: $(OS)
	@echo RELEASE_BUILD: $(RELEASE_BUILD)
	@echo GODOT_BIN: $(GODOT_BIN)
	@echo DIST_DIR: $(DIST_DIR)
	@echo SCONS_PLATFORM: $(SCONS_PLATFORM)
	@echo EXPORT_EXT: $(EXPORT_EXT)
	@echo BUILD_VERSION: $(BUILD_VERSION)
	@echo -----------------------------------------------------

# build pion wrapper
ifeq ($(OS),Windows_NT)
go:
	cd ./thirdparty/pionc/ && make build
	mkdir -p ./build/extension-4.1/webrtc/lib
	cp ./thirdparty/pionc/lib/libwebrtc.dll ./build/extension-4.1/webrtc/lib/libwebrtc.dll
else
go:
	cd ./thirdparty/pionc/ && make build
endif

# build godot extension
ifeq ($(OS),Windows_NT)
scons:
	scons $(VERSIONED_OPTION) platform=$(SCONS_PLATFORM) use_mingw=yes
else
scons:
	scons $(VERSIONED_OPTION) platform=$(SCONS_PLATFORM)
endif

demos: 
	./copy_output.sh ./build/extension-4.1 demo_projects.txt

# build extension and copy output to demo projects
gd: scons

dev:
	scons platform=macos use_mingw=yes dev_build=1

ifeq ($(OS),Darwin)
opus:
	cd ./thirdparty/opus && ./autogen.sh && ./configure && make
else ifeq ($(OS),Linux)
opus:
	cd ./thirdparty/opus && ./autogen.sh && ./configure CFLAGS="-fPIC" && make
else ifeq ($(OS),Windows_NT)
opus: variables
	mkdir -p "thirdparty/opus/.libs"
	cd thirdparty/opus/.libs && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
	cmake --build thirdparty/opus/.libs --config Release
#	copy /A /Y misc\scripts\build_opus.bat thirdparty\opus\build_opus.bat
#	cd .\thirdparty\opus && .\build_opus.bat
else
opus:
	@echo "Unsupported operating system"
	exit 1
endif

# run unit tests
tests: 
	cd ./build/tests && test

.PHONY: clean
clean:
	scons -c
