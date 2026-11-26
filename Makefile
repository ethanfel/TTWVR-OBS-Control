WINCXX ?= i686-w64-mingw32-g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
RELEASE := $(strip $(file <VERSION))
# xNVSE exposes a numeric version: 1.1.8-r1 becomes 1010801.
BUILD_NUMBER := $(shell printf '%d%02d%02d%02d' $(subst ., ,$(subst -r,.,$(RELEASE))))
WINFLAGS := $(CXXFLAGS) -DTTWVROBS_BUILD=$(BUILD_NUMBER) -DTTWVROBS_RELEASE='"$(RELEASE)"' -D_WIN32_WINNT=0x0603 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ibuild/include
DLL := build/TTWVROBS.dll
SOURCES := src/Plugin.cpp src/Wrist.cpp src/WristLayout.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp src/PausePreview.cpp
LIBS := -lwinhttp -lcrypt32 -lbcrypt -lgdi32 -lole32 -lwindowscodecs -luuid

.PHONY: all package test
all: $(DLL)
build/include/nlohmann:
	mkdir -p build/include
	ln -s /usr/include/nlohmann $@
$(DLL): VERSION $(SOURCES) $(wildcard src/*.h) build/include/nlohmann
	$(WINCXX) $(WINFLAGS) -shared -static -s $(SOURCES) -o $@ $(LIBS)
package: $(DLL)
	python3 package.py
build/probe.exe: VERSION tests/probe.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp src/Preview.h src/Control.h build/include/nlohmann
	$(WINCXX) $(WINFLAGS) -static -Isrc tests/probe.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp -o $@ $(LIBS)
build/texture-test.exe: tests/texture.cpp src/PausePreview.cpp src/Preview.cpp src/Preview.h
	$(WINCXX) $(WINFLAGS) -static -Isrc tests/texture.cpp src/Preview.cpp -o $@ $(LIBS) -ld3d9
build/wrist-test.exe: tests/wrist.cpp src/Wrist.cpp src/WristLayout.cpp src/WristLayout.h src/Control.h src/Wrist.h src/Panel.h src/Preview.h
	$(WINCXX) $(WINFLAGS) -municode -static -Isrc tests/wrist.cpp src/WristLayout.cpp -o $@
test: all build/probe.exe build/texture-test.exe build/wrist-test.exe
	python3 tests/check.py
