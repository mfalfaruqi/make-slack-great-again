# Shortcuts for the scripts in scripts/ (Linux and macOS; Windows: scripts\build.ps1).
# The scripts and CMake stay the source of truth: every target here is one call.
#
#   make                 MinSizeRel build -> build/
#   make test            build and run every suite
#   make test T=shell    only the ctest entries matching "shell" (ctest -R)
#   make test L=claude   only the entries with that label (ctest -L)
#   make run             build, then start msga
#   make help            every target

UNAME := $(shell uname -s)
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu)

ifeq ($(UNAME),Darwin)
CONFIGURE := scripts/configure-mac.sh
BIN_PATH  := msga.app/Contents/MacOS/msga
else
CONFIGURE := scripts/configure-linux.sh
BIN_PATH  := msga
endif

# What `make test` hands to ctest: T is a name regex, L a label.
CTEST_SELECT := $(if $(T),-R '$(T)') $(if $(L),-L '$(L)')

.DEFAULT_GOAL := build
.PHONY: help configure build debug test test-debug demo asan run run-debug run-demo \
        format translations icons coverage release clean

help: ## List the targets
	@awk -F':.*## ' '/^[a-z-]+:.*## / { printf "  %-14s %s\n", $$1, $$2 }' $(MAKEFILE_LIST)

configure: ## Install the build dependencies and the git hooks
	$(CONFIGURE)

build: ## MinSizeRel build -> build/
	scripts/build.sh

debug: ## Debug build -> build-debug/
	scripts/build.sh --debug

# With T or L: build (the tests option stays on in build/), then run the selection.
test: ## Build and run the suites (T=<regex> or L=<label> narrows them)
ifeq ($(strip $(T)$(L)),)
	scripts/build.sh --test
else
	cmake -S . -B build -G Ninja -DMSGA_BUILD_TESTS=ON
	cmake --build build --parallel $(NPROC)
	ctest --test-dir build --output-on-failure -j $(NPROC) $(CTEST_SELECT)
endif

test-debug: ## The suites on a Debug build -> build-debug/
	scripts/build.sh --debug --test

demo: ## Build with the fixture workspace compiled in (msga --demo)
	scripts/build.sh --demo

asan: ## AddressSanitizer + LeakSanitizer build -> build-asan/
	scripts/build.sh --asan

run: build ## Build, then start msga
	build/$(BIN_PATH)

run-debug: debug ## Debug build, then start msga
	build-debug/$(BIN_PATH)

# The tests' fixture plus #updates (100 messages, 25 pinned), written to
# build/demo: tests/assets itself stays as the suites expect it.
run-demo: demo ## Start msga on the demo workspace (the test fixture plus #updates)
	scripts/gen-demo-fixture.py build/demo
	build/$(BIN_PATH) --demo build/demo

format: ## clang-format the src/ and tests/ trees (FILES="a.cpp b.h" for some)
	scripts/format.sh $(FILES)

translations: build ## Update the .po files and languages_generated.cpp
	cmake --build build --target translations

icons: build ## Regenerate gfx/icons_generated.* from gfx/ui/*.svg
	cmake --build build --target icons

coverage: ## Coverage report -> build-cov/coverage.html (needs gcovr)
	scripts/coverage.sh

release: ## Clean release-flags build -> build-release/, with the size report
	scripts/release.sh

clean: ## Remove every build directory
	rm -rf build build-debug build-asan build-cov build-release
