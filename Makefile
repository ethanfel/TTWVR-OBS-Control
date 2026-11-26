WINCXX ?= i686-w64-mingw32-g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
WINFLAGS := $(CXXFLAGS) -D_WIN32_WINNT=0x0603 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ibuild/include
DLL := build/TTWVROBS.dll
SOURCES := src/Plugin.cpp src/Wrist.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp src/PausePreview.cpp
LIBS := -lwinhttp -lcrypt32 -lbcrypt -lgdi32 -lole32 -lwindowscodecs -luuid

.PHONY: all package test
all: $(DLL)
build/include/nlohmann:
	mkdir -p build/include
	ln -s /usr/include/nlohmann $@
$(DLL): $(SOURCES) $(wildcard src/*.h) build/include/nlohmann
	$(WINCXX) $(WINFLAGS) -shared -static -s $(SOURCES) -o $@ $(LIBS)
package: $(DLL)
	python3 package.py
build/probe.exe: tests/probe.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp src/Preview.h src/Control.h build/include/nlohmann
	$(WINCXX) $(WINFLAGS) -static -Isrc tests/probe.cpp src/Panel.cpp src/Obs.cpp src/Labels.cpp src/Preview.cpp -o $@ $(LIBS)
build/texture-test.exe: tests/texture.cpp src/PausePreview.cpp src/Preview.cpp src/Preview.h
	$(WINCXX) $(WINFLAGS) -static -Isrc tests/texture.cpp src/Preview.cpp -o $@ $(LIBS) -ld3d9
test: all build/probe.exe build/texture-test.exe
	python3 tests/check.py
