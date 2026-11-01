# SLEER - build
#
#   make          build build/sleer
#   make test     build and run the engine tests (no SDL needed)
#   make smoke    render one frame headlessly and save a screenshot
#   make run      build and launch the app
#   make clean    remove build/

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

# MinGW ships gcc but often no "cc" alias, so prefer gcc unless CC was given.
ifeq ($(origin CC),default)
CC := gcc
endif

BUILD    := build
SDL3_DIR ?= .deps/sdl3

CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CFLAGS   += -MMD -MP
CPPFLAGS += -Isrc -isystem third_party -isystem $(SDL3_DIR)/include

APP_SRC  := src/main.c src/panels.c src/ui.c src/dither.c src/image.c src/palette.c src/worker.c
TEST_SRC := tests/test_dither.c src/dither.c src/image.c src/palette.c
APP_OBJ  := $(APP_SRC:%.c=$(BUILD)/%.o)
TEST_OBJ := $(TEST_SRC:%.c=$(BUILD)/%.o)

APP_LDLIBS  := -L$(SDL3_DIR)/lib -lSDL3 -lm
TEST_LDLIBS := -lm

# SDL3 is loaded at runtime from a DLL on Windows; keep a copy next to the exe.
ifeq ($(OS),Windows_NT)
RUNTIME_DEPS := $(BUILD)/SDL3.dll
$(BUILD)/SDL3.dll: $(SDL3_DIR)/bin/SDL3.dll
	cp $< $@
else
RUNTIME_DEPS :=
endif

.PHONY: all test run smoke clean

all: $(BUILD)/sleer$(EXE)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/sleer$(EXE): $(APP_OBJ) $(RUNTIME_DEPS)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJ) $(APP_LDLIBS)

$(BUILD)/test_dither$(EXE): $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) $(TEST_LDLIBS)

test: $(BUILD)/test_dither$(EXE)
	$(BUILD)/test_dither$(EXE)

run: $(BUILD)/sleer$(EXE)
	$(BUILD)/sleer$(EXE)

smoke: $(BUILD)/sleer$(EXE)
	$(BUILD)/sleer$(EXE) --smoke-test

clean:
	rm -rf $(BUILD)

-include $(APP_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
