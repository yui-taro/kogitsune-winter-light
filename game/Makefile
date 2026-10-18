RAYLIB_DIR ?= /private/tmp/jev-raylib-6.0
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?= -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo
ASSET_DIR := $(CURDIR)/assets
SOURCES := src/main.cpp src/World.cpp src/JevBridge.cpp src/Audio.cpp
TARGET := build/foxgame

.PHONY: all run test bundle web clean
all: $(TARGET)

$(TARGET): $(SOURCES) include/World.hpp include/JevBridge.hpp include/Audio.hpp vendor/nlohmann/json.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude -Ivendor -I$(RAYLIB_DIR)/src -DASSET_DIR='"$(ASSET_DIR)"' $(SOURCES) \
		-L$(RAYLIB_DIR)/src -lraylib -lcurl $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

bundle: $(TARGET)
	mkdir -p "build/こぎつねと冬の灯り.app/Contents/MacOS" "build/こぎつねと冬の灯り.app/Contents/Resources/assets"
	cp $(TARGET) "build/こぎつねと冬の灯り.app/Contents/MacOS/foxgame"
	cp assets/*.png assets/*.otf assets/*.txt assets/*.ogg assets/*.pak "build/こぎつねと冬の灯り.app/Contents/Resources/assets/"
	cp vendor/nlohmann/LICENSE.MIT "build/こぎつねと冬の灯り.app/Contents/Resources/LICENSE-json.txt"
	cp vendor/raylib-LICENSE.txt "build/こぎつねと冬の灯り.app/Contents/Resources/LICENSE-raylib.txt"
	cp Info.plist "build/こぎつねと冬の灯り.app/Contents/Info.plist"

# Browser build (needs emcc on PATH and raylib built with PLATFORM=PLATFORM_WEB).
RAYLIB_WEB_DIR ?= $(RAYLIB_DIR)
WEB_ASSETS := $(filter-out assets/OFL.txt,$(wildcard assets/*.png assets/*.otf assets/*.ogg assets/*.pak assets/*.txt))
web: $(SOURCES) web/shell.html
	mkdir -p build-web
	em++ -std=c++17 -O2 -Iinclude -Ivendor -I$(RAYLIB_WEB_DIR)/src -DPLATFORM_WEB $(SOURCES) \
		$(RAYLIB_WEB_DIR)/src/libraylib.web.a -o build-web/index.html \
		-sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 \
		-sGL_ENABLE_GET_PROC_ADDRESS=1 -sSTACK_SIZE=1048576 \
		-sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain \
		'-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$$stringToNewUTF8,$$UTF8ToString' \
		$(foreach f,$(WEB_ASSETS),--preload-file $(f)@/assets/$(notdir $(f))) \
		--shell-file web/shell.html

test: build/test_world build/test_contract
	./build/test_world
	./build/test_contract

build/test_contract: ../tests/test_contract.cpp src/World.cpp include/World.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude ../tests/test_contract.cpp src/World.cpp -o $@

build/test_bridge: ../tests/test_bridge.cpp src/World.cpp src/JevBridge.cpp include/World.hpp include/JevBridge.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude -Ivendor ../tests/test_bridge.cpp src/World.cpp src/JevBridge.cpp -lcurl -o $@

build/test_world: ../tests/test_world.cpp src/World.cpp include/World.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude ../tests/test_world.cpp src/World.cpp -o $@

clean:
	$(RM) -r build-web
	$(RM) $(TARGET) build/test_world build/test_contract build/test_bridge
