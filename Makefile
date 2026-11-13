BUILD_DIR = build
BUILD_TYPE = Debug
BASE_FLAGS = -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.PHONY: test demo clean

# Configure, build, and run tests only
test:
	cmake -B $(BUILD_DIR) $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=ON -DAW_TPL_BUILD_EXAMPLES=OFF
	cmake --build $(BUILD_DIR) --parallel
	ctest --test-dir $(BUILD_DIR) -C $(BUILD_TYPE) --output-on-failure

# Configure, build, and run aw_tpl_demo only
demo:
	cmake -B $(BUILD_DIR) $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=OFF -DAW_TPL_BUILD_EXAMPLES=ON
	cmake --build $(BUILD_DIR) --target aw_tpl_demo --parallel
	./$(BUILD_DIR)/examples/aw_tpl_demo

clean:
	rm -rf $(BUILD_DIR)
