BUILD_TYPE = Debug
BASE_FLAGS = -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    EXT = .exe
else
    PLATFORM = linux
    EXT =
endif

BUILD_DIR = build/$(PLATFORM)
BUILD_DIR_TEST = $(BUILD_DIR)/test
BUILD_DIR_DEMO = $(BUILD_DIR)/demo
ifeq ($(PLATFORM),windows)
    DEMO_DIR = ./$(BUILD_DIR_DEMO)/examples/$(BUILD_TYPE)
else
    DEMO_DIR = ./$(BUILD_DIR_DEMO)/examples
endif

.PHONY: test demo clean release release-dry-run

# Configure, build, and run tests only
test:
	cmake -B $(BUILD_DIR_TEST) --fresh $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=ON -DAW_TPL_BUILD_EXAMPLES=OFF
	cmake --build $(BUILD_DIR_TEST) --config $(BUILD_TYPE) --parallel
	ctest --test-dir $(BUILD_DIR_TEST) -C $(BUILD_TYPE) --output-on-failure

# Configure, build, and run every example in examples/
demo:
	cmake -B $(BUILD_DIR_DEMO) --fresh $(BASE_FLAGS) -DAW_TPL_BUILD_TESTS=OFF -DAW_TPL_BUILD_EXAMPLES=ON
	cmake --build $(BUILD_DIR_DEMO) --config $(BUILD_TYPE) --target aw_tpl_examples --parallel
	for example in $(DEMO_DIR)/aw_tpl_example_*$(EXT); do echo "=== $$example"; "$$example" || exit 1; done

clean:
	rm -rf $(BUILD_DIR)

# Cut a SemVer release from CHANGELOG.md [Unreleased]: bump, commit, tag. `make release PUSH=1` also pushes.
ifeq ($(PLATFORM),windows)
    RELEASE_CMD = powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release.ps1
    RELEASE_PUSH = -Push
    RELEASE_DRY = -DryRun
else
    RELEASE_CMD = bash scripts/release.sh
    RELEASE_PUSH = --push
    RELEASE_DRY = --dry-run
endif

release:
	$(RELEASE_CMD) $(if $(PUSH),$(RELEASE_PUSH))

release-dry-run:
	$(RELEASE_CMD) $(RELEASE_DRY)
