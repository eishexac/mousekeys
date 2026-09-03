# mousekeys native build.
#
# One portable core, one platform backend per OS, zero third-party
# dependencies. `make` builds build/mousekeysd for the host OS; `make check`
# builds and runs the core unit tests, which are platform-independent and
# run anywhere a C++17 compiler exists — including a qube used to review
# this source before it enters dom0.
#
# Determinism notes for packagers: no __DATE__/__TIME__ anywhere, and
# -ffile-prefix-map strips the build path, so an identical toolchain
# produces an identical binary.
#
# The Hammerspoon spoon (the .lua files in this repository's root) is the
# original macOS implementation and still works; this native build is its
# replacement and the only implementation for Linux.

VERSION := 0.1.0

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -Isrc -DMK_VERSION=\"$(VERSION)\" -ffile-prefix-map=$(CURDIR)=.

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  PLATFORM_SRC := src/platform/macos/macos_backend.cpp src/platform/macos/alert.mm \
                  src/platform/macos/menubar.mm src/platform/macos/loginitem.mm
  LDLIBS := -framework ApplicationServices -framework CoreFoundation \
            -framework AppKit -framework ServiceManagement
  MACOS_PREBUILD := :
  MACOS_LDFLAGS :=
  # Release DMGs are built per-arch: the CI release workflow sets MACOS_ARCH to
  # `-arch arm64` or `-arch x86_64` and builds a separate .app for each (macOS
  # Tahoe/26 is the last macOS to support Intel). Unset by default, so a local
  # build targets the host arch; set it for a universal or cross build, e.g.
  # `make app MACOS_ARCH="-arch arm64 -arch x86_64"`.
  MACOS_ARCH ?=
else
  PLATFORM_SRC := src/platform/linux/linux_backend.cpp
  LDLIBS :=
  MACOS_PREBUILD := :
  MACOS_LDFLAGS :=
  MACOS_ARCH :=
endif

CORE_SRC := src/core/config.cpp src/core/movement.cpp src/core/scroll.cpp \
            src/core/click.cpp src/core/engine.cpp src/core/default_config.cpp
HDRS := $(wildcard src/core/*.h src/platform/*.h src/platform/*/*.h)

PREFIX ?= /usr/local

.PHONY: all build check clean version install sign app sign-app icon

all: build

build: build/mousekeysd

build/mousekeysd: $(CORE_SRC) $(PLATFORM_SRC) src/main.cpp $(HDRS)
	@mkdir -p build
	@$(MACOS_PREBUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MACOS_ARCH) -o $@ $(CORE_SRC) $(PLATFORM_SRC) src/main.cpp $(LDLIBS) $(MACOS_LDFLAGS)

# -O0 -g after CXXFLAGS overrides -O2; asserts must not be compiled out.
build/test_core: $(CORE_SRC) test/test_core.cpp $(HDRS)
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O0 -g -o $@ $(CORE_SRC) test/test_core.cpp

check: build/test_core
	./build/test_core

# One line, nothing else: release tooling asks every project its version
# this way.
version:
	@echo $(VERSION)

install: build
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 build/mousekeysd $(DESTDIR)$(PREFIX)/bin/mousekeysd

# Assemble mousekeys.app — the primary macOS distribution. As a registered
# .app, macOS lists it in Accessibility and Login Items on its own, which a
# bare binary can't do. The daemon binary goes in Contents/MacOS/mousekeys;
# in_app_bundle() then makes it run directly (no LaunchAgent handoff).
# Render AppIcon.icns from the mark, committed to the repo. The mark is a
# chrome (two-tone gray) keycap with a dark Caps Lock (U+21EA) legend — the
# actual mouse-layer key on both macOS and Linux — and a black, white-bordered
# pointer (mac-style) tilted slightly left, its tip just right of the key's
# center, resting on the key.
ICON_OPTS := mono black darkglyph 'glyph:⇪' rounded badge:0.36 mac tilt:-12 bdx:0.22 bdy:0.10
icon: tools/mkicon.mm
	@mkdir -p build
	$(CXX) -fobjc-arc -std=c++17 -framework Cocoa tools/mkicon.mm -o build/mkicon
	./build/mkicon build/mousekeys.iconset $(ICON_OPTS)
	iconutil -c icns build/mousekeys.iconset -o src/platform/macos/AppIcon.icns
	@echo "wrote src/platform/macos/AppIcon.icns"

APP := build/mousekeys.app
app: build/mousekeysd src/platform/macos/Info.plist.in
	rm -rf $(APP)
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources
	cp build/mousekeysd $(APP)/Contents/MacOS/mousekeys
	sed 's/@VERSION@/$(VERSION)/g' src/platform/macos/Info.plist.in > $(APP)/Contents/Info.plist
	@[ -f src/platform/macos/AppIcon.icns ] && cp src/platform/macos/AppIcon.icns $(APP)/Contents/Resources/AppIcon.icns || echo "note: no AppIcon.icns yet (run make icon)"
	@echo "built $(APP)"

# Developer ID sign the whole bundle (release builds; CI does this in the
# release workflow). Hardened Runtime + timestamp for notarization.
sign-app: app
	@test -n "$(MOUSEKEYS_CODESIGN_ID)" || { echo "set MOUSEKEYS_CODESIGN_ID=..." >&2; exit 1; }
	codesign --force --deep --options runtime --timestamp --sign "$(MOUSEKEYS_CODESIGN_ID)" $(APP)
	@codesign -dvvv $(APP) 2>&1 | grep -E 'Authority=|Identifier=|TeamIdentifier'

# Developer ID code signing for distribution (macOS). Hardened Runtime
# (--options runtime) and a secure --timestamp are what notarization
# requires and what gives the binary a STABLE identity, so the Accessibility
# grant survives upgrades. Pass the identity:
#
#   make sign MOUSEKEYS_CODESIGN_ID="Developer ID Application: NAME (TEAMID)"
#
# List identities with: security find-identity -v -p codesigning
MOUSEKEYS_CODESIGN_ID ?=
sign: build
	@test -n "$(MOUSEKEYS_CODESIGN_ID)" || { \
		echo "sign: set MOUSEKEYS_CODESIGN_ID=\"Developer ID Application: NAME (TEAMID)\"" >&2; \
		exit 1; }
	codesign --force --options runtime --timestamp \
		--sign "$(MOUSEKEYS_CODESIGN_ID)" build/mousekeysd
	@echo "--- signature ---"
	@codesign -dvvv build/mousekeysd 2>&1 | grep -E 'Authority=|TeamIdentifier|flags='

clean:
	rm -rf build
