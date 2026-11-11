CXX ?= g++
CXXFLAGS ?= -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -fdiagnostics-color=always
LDFLAGS ?=
LDLIBS ?=

# Optional sanitizer variant. Use SANITIZER=address, SANITIZER=undefined or
# SANITIZER=address,undefined together with a matching BUILD_SUFFIX so each
# variant keeps its own objects and flags are never mixed.
SANITIZER ?=
SANITIZER_FLAGS ?= $(if $(SANITIZER),-fsanitize=$(SANITIZER) -fno-sanitize-recover=all -fno-omit-frame-pointer)
BUILD_SUFFIX ?=

# Mixing sanitized objects with normal ones breaks linking, so require an
# explicit suffix when SANITIZER is set manually. The test-asan/test-ubsan/
# test-sanitize targets set both variables for you.
ifneq ($(SANITIZER),)
ifeq ($(BUILD_SUFFIX),)
$(error BUILD_SUFFIX is required when SANITIZER is set)
endif
endif

TEST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOTDIR := $(abspath $(TEST_DIR)/..)
BUILDDIR := $(TEST_DIR)/build$(BUILD_SUFFIX)
OBJDIR := $(BUILDDIR)/obj

CXXFLAGS += $(SANITIZER_FLAGS) -I$(ROOTDIR)/inc -I$(ROOTDIR)/lib \
	-I$(ROOTDIR)/lib/imgui/src -I$(ROOTDIR)/lib/imgui/src/backends
LDFLAGS += $(SANITIZER_FLAGS)
LDLIBS += -lz

# Código de producción que usan los tests headless.
PROJECT_SRCS := \
	$(ROOTDIR)/src/core/view_transform.cpp \
	$(ROOTDIR)/src/paint/app_config.cpp \
	$(ROOTDIR)/src/paint/canvas_document.cpp \
	$(ROOTDIR)/src/paint/image_transform.cpp \
	$(ROOTDIR)/src/paint/image_export.cpp \
	$(ROOTDIR)/src/paint/document_file.cpp \
	$(ROOTDIR)/src/paint/floating_region_editor.cpp \
	$(ROOTDIR)/src/paint/paint_backend.cpp \
	$(ROOTDIR)/src/paint/paint_session.cpp \
	$(ROOTDIR)/src/paint/cursor_asset.cpp \
	$(ROOTDIR)/src/paint/draw_buffer.cpp \
	$(ROOTDIR)/src/paint/image_buffer.cpp \
	$(ROOTDIR)/src/paint/indexed_image.cpp \
	$(ROOTDIR)/src/paint/palette.cpp \
	$(ROOTDIR)/src/paint/undo_history.cpp \
	$(ROOTDIR)/src/tools/draw_tool.cpp \
	$(ROOTDIR)/src/tools/freehand_tool.cpp \
	$(ROOTDIR)/src/tools/line_tool.cpp \
	$(ROOTDIR)/src/tools/curve_tool.cpp \
	$(ROOTDIR)/src/tools/pencil_tool.cpp \
	$(ROOTDIR)/src/tools/brush_tool.cpp \
	$(ROOTDIR)/src/tools/eraser_tool.cpp \
	$(ROOTDIR)/src/tools/fill_tool.cpp \
	$(ROOTDIR)/src/tools/pick_color_tool.cpp \
	$(ROOTDIR)/src/tools/selection_tool.cpp \
	$(ROOTDIR)/src/tools/shape_tool.cpp \
	$(ROOTDIR)/src/tools/square_tool.cpp \
	$(ROOTDIR)/src/tools/circle_tool.cpp

PROJECT_OBJS := $(patsubst $(ROOTDIR)/src/%.cpp,$(OBJDIR)/src/%.o,$(PROJECT_SRCS))
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TEST_SRCS))
DOCTEST_OBJ := $(OBJDIR)/doctest_main.o

TEST_BINS := \
	$(BUILDDIR)/test_image_model \
	$(BUILDDIR)/test_app_config \
	$(BUILDDIR)/test_image_export \
	$(BUILDDIR)/test_dirty \
	$(BUILDDIR)/test_draw_buffer \
	$(BUILDDIR)/test_undo \
	$(BUILDDIR)/test_clamp \
	$(BUILDDIR)/test_view_transform \
	$(BUILDDIR)/test_indexed_image \
	$(BUILDDIR)/test_cursor_asset \
	$(BUILDDIR)/test_tool_options \
	$(BUILDDIR)/test_canvas_resize \
	$(BUILDDIR)/test_canvas_tiles \
	$(BUILDDIR)/test_canvas_transform \
	$(BUILDDIR)/test_document_file \
	$(BUILDDIR)/test_window_title \
	$(BUILDDIR)/test_file_session \
	$(BUILDDIR)/test_line \
	$(BUILDDIR)/test_curve \
	$(BUILDDIR)/test_pencil \
	$(BUILDDIR)/test_brush \
	$(BUILDDIR)/test_eraser \
	$(BUILDDIR)/test_fill \
	$(BUILDDIR)/test_pick_color \
	$(BUILDDIR)/test_circle \
	$(BUILDDIR)/test_floating_region

IMAGE_OBJS := $(OBJDIR)/src/paint/image_buffer.o
PALETTE_OBJS := $(IMAGE_OBJS) $(OBJDIR)/src/paint/palette.o
CURSOR_OBJS := $(OBJDIR)/src/paint/cursor_asset.o \
	$(OBJDIR)/src/paint/indexed_image.o
CANVAS_OBJS := $(PALETTE_OBJS) \
	$(OBJDIR)/src/paint/image_transform.o \
	$(OBJDIR)/src/core/view_transform.o \
	$(OBJDIR)/src/paint/canvas_document.o \
	$(OBJDIR)/src/paint/floating_region_editor.o \
	$(OBJDIR)/src/paint/paint_session.o \
	$(OBJDIR)/src/paint/draw_buffer.o \
	$(OBJDIR)/src/paint/undo_history.o \
	$(OBJDIR)/src/tools/draw_tool.o

SHAPE_OBJS := $(OBJDIR)/src/tools/shape_tool.o

all: test

test: $(TEST_BINS)
	@set -e; for test in $(TEST_BINS); do \
		echo "== $$test =="; \
		$$test; \
	done

# Run one test binary, e.g. `make test-circle` or `make test-tool_options`.
# The stem maps to tests/build/test_<stem>.
test-%: $(BUILDDIR)/test_%
	$<

# Sanitizer variants. Each one uses a separate build directory.
test-asan:
	$(MAKE) -C $(TEST_DIR) BUILD_SUFFIX=-asan SANITIZER=address test

test-ubsan:
	$(MAKE) -C $(TEST_DIR) BUILD_SUFFIX=-ubsan SANITIZER=undefined test

test-sanitize:
	$(MAKE) -C $(TEST_DIR) BUILD_SUFFIX=-sanitize SANITIZER=address,undefined test

test-full: test test-asan test-ubsan test-sanitize

$(OBJDIR)/src/%.o: $(ROOTDIR)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(DOCTEST_OBJ): $(TEST_DIR)/doctest_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/test_image_model: $(OBJDIR)/tests/test_image_model.o $(PALETTE_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_app_config: $(OBJDIR)/tests/test_app_config.o \
		$(OBJDIR)/src/paint/app_config.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_image_export: $(OBJDIR)/tests/test_image_export.o \
		$(OBJDIR)/src/paint/image_export.o \
		$(OBJDIR)/src/paint/image_buffer.o \
		$(OBJDIR)/src/paint/palette.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_dirty: $(OBJDIR)/tests/test_dirty.o $(IMAGE_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_draw_buffer: $(OBJDIR)/tests/test_draw_buffer.o \
		$(IMAGE_OBJS) $(OBJDIR)/src/paint/draw_buffer.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_undo: $(OBJDIR)/tests/test_undo.o $(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_clamp: $(OBJDIR)/tests/test_clamp.o $(CANVAS_OBJS) \
		$(SHAPE_OBJS) $(OBJDIR)/src/tools/square_tool.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_view_transform: $(OBJDIR)/tests/test_view_transform.o \
		$(OBJDIR)/src/core/view_transform.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_indexed_image: $(OBJDIR)/tests/test_indexed_image.o \
		$(OBJDIR)/src/paint/indexed_image.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_cursor_asset: $(OBJDIR)/tests/test_cursor_asset.o \
		$(CURSOR_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_tool_options: $(OBJDIR)/tests/test_tool_options.o \
		$(CANVAS_OBJS) $(SHAPE_OBJS) \
		$(OBJDIR)/src/tools/square_tool.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_canvas_resize: $(OBJDIR)/tests/test_canvas_resize.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_canvas_tiles: $(OBJDIR)/tests/test_canvas_tiles.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_canvas_transform: $(OBJDIR)/tests/test_canvas_transform.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_document_file: $(OBJDIR)/tests/test_document_file.o \
		$(OBJDIR)/src/paint/document_file.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_window_title: $(OBJDIR)/tests/test_window_title.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_file_session: $(OBJDIR)/tests/test_file_session.o \
		$(OBJDIR)/src/paint/paint_backend.o \
		$(OBJDIR)/src/paint/document_file.o \
		$(OBJDIR)/src/paint/image_export.o \
		$(CANVAS_OBJS) $(CURSOR_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_line: $(OBJDIR)/tests/test_line.o \
		$(OBJDIR)/src/tools/line_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_curve: $(OBJDIR)/tests/test_curve.o \
		$(OBJDIR)/src/tools/curve_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_pencil: $(OBJDIR)/tests/test_pencil.o \
		$(OBJDIR)/src/tools/freehand_tool.o \
		$(OBJDIR)/src/tools/pencil_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_circle: $(OBJDIR)/tests/test_circle.o \
		$(OBJDIR)/src/tools/circle_tool.o \
		$(SHAPE_OBJS) \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_brush: $(OBJDIR)/tests/test_brush.o \
		$(OBJDIR)/src/tools/brush_tool.o \
		$(OBJDIR)/src/tools/freehand_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_eraser: $(OBJDIR)/tests/test_eraser.o \
		$(OBJDIR)/src/tools/eraser_tool.o \
		$(OBJDIR)/src/tools/freehand_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_fill: $(OBJDIR)/tests/test_fill.o \
		$(OBJDIR)/src/tools/fill_tool.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_pick_color: $(OBJDIR)/tests/test_pick_color.o \
		$(OBJDIR)/src/tools/pick_color_tool.o \
		$(OBJDIR)/src/tools/fill_tool.o \
		$(OBJDIR)/src/tools/selection_tool.o \
		$(OBJDIR)/src/paint/paint_backend.o \
		$(OBJDIR)/src/paint/document_file.o \
		$(OBJDIR)/src/paint/image_export.o \
		$(CANVAS_OBJS) $(CURSOR_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_floating_region: $(OBJDIR)/tests/test_floating_region.o \
		$(CANVAS_OBJS) $(OBJDIR)/src/tools/selection_tool.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) -r $(TEST_DIR)/build $(TEST_DIR)/build-asan \
		$(TEST_DIR)/build-ubsan $(TEST_DIR)/build-sanitize

-include $(PROJECT_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(DOCTEST_OBJ:.o=.d)

.PHONY: all test test-full test-asan test-ubsan test-sanitize clean
