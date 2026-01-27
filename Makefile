BUILD_DIR ?= build-make
GENERATOR ?= Unix Makefiles

.PHONY: all configure build clean rebuild

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)"

build: configure
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
