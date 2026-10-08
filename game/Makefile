RAYLIB_DIR ?= /private/tmp/jev-raylib-6.0
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?= -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo
ASSET_DIR := $(CURDIR)/assets
SOURCES := src/main.cpp src/World.cpp src/JevBridge.cpp src/Audio.cpp
TARGET := build/foxgame

.PHONY: all run test bundle clean
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
	cp assets/*.png assets/*.otf assets/*.txt "build/こぎつねと冬の灯り.app/Contents/Resources/assets/"
	cp vendor/nlohmann/LICENSE.MIT "build/こぎつねと冬の灯り.app/Contents/Resources/LICENSE-json.txt"
	cp vendor/raylib-LICENSE.txt "build/こぎつねと冬の灯り.app/Contents/Resources/LICENSE-raylib.txt"
	cp Info.plist "build/こぎつねと冬の灯り.app/Contents/Info.plist"

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
	$(RM) $(TARGET) build/test_world build/test_contract build/test_bridge
