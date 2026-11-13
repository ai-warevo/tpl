BUILD_TYPE = Debug

ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    EXT = .exe
    BASE_FLAGS = -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_CXX_FLAGS="/std:c++20 /EHsc"
else
    PLATFORM = linux
    EXT =
    BASE_FLAGS = -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
endif

BUILD_DIR = build/$(PLATFORM)
BUILD_DIR_TEST = $(BUILD_DIR)/test
BUILD_DIR_DEMO = $(BUILD_DIR)/demo
ifeq ($(PLATFORM),windows)
    DEMO_BIN = ./$(BUILD_DIR_DEMO)/examples/$(BUILD_TYPE)/aw_tpl_demo$(EXT)
else
    DEMO_BIN = ./$(BUILD_DIR_DEMO)/examples/aw_tpl_demo
endif

.PHONY: test demo clean

# Configure, build, and run tests only
test:
	cmake -B $(BUILD_DIR_TEST) --fresh $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=ON -DAW_TPL_BUILD_EXAMPLES=OFF
	cmake --build $(BUILD_DIR_TEST) --config $(BUILD_TYPE) --parallel
	ctest --test-dir $(BUILD_DIR_TEST) -C $(BUILD_TYPE) --output-on-failure

# Configure, build, and run aw_tpl_demo only
demo:
	cmake -B $(BUILD_DIR_DEMO) --fresh $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=OFF -DAW_TPL_BUILD_EXAMPLES=ON
	cmake --build $(BUILD_DIR_DEMO) --config $(BUILD_TYPE) --target aw_tpl_demo --parallel
	$(DEMO_BIN)

clean:
	rm -rf $(BUILD_DIR)
