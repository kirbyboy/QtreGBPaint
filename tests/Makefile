CXX ?= g++
CXXFLAGS ?= -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -fdiagnostics-color=always
LDFLAGS ?=
LDLIBS ?=

TEST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOTDIR := $(abspath $(TEST_DIR)/..)
BUILDDIR := $(TEST_DIR)/build
OBJDIR := $(BUILDDIR)/obj

CXXFLAGS += -I$(ROOTDIR)/inc -I$(ROOTDIR)/lib

# Código de producción que usan los tests headless.
PROJECT_SRCS := \
	$(ROOTDIR)/src/core/view_transform.cpp \
	$(ROOTDIR)/src/paint/canvas_document.cpp \
	$(ROOTDIR)/src/paint/floating_region_editor.cpp \
	$(ROOTDIR)/src/paint/paint_session.cpp \
	$(ROOTDIR)/src/paint/cursor_asset.cpp \
	$(ROOTDIR)/src/paint/draw_buffer.cpp \
	$(ROOTDIR)/src/paint/image_buffer.cpp \
	$(ROOTDIR)/src/paint/indexed_image.cpp \
	$(ROOTDIR)/src/paint/palette.cpp \
	$(ROOTDIR)/src/paint/undo_history.cpp \
	$(ROOTDIR)/src/tools/draw_tool.cpp \
	$(ROOTDIR)/src/tools/selection_tool.cpp \
	$(ROOTDIR)/src/tools/square_tool.cpp

PROJECT_OBJS := $(patsubst $(ROOTDIR)/src/%.cpp,$(OBJDIR)/src/%.o,$(PROJECT_SRCS))
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TEST_SRCS))
DOCTEST_OBJ := $(OBJDIR)/doctest_main.o

TEST_BINS := \
	$(BUILDDIR)/test_image_model \
	$(BUILDDIR)/test_dirty \
	$(BUILDDIR)/test_draw_buffer \
	$(BUILDDIR)/test_undo \
	$(BUILDDIR)/test_clamp \
	$(BUILDDIR)/test_view_transform \
	$(BUILDDIR)/test_indexed_image \
	$(BUILDDIR)/test_cursor_asset \
	$(BUILDDIR)/test_tool_options \
	$(BUILDDIR)/test_canvas_resize \
	$(BUILDDIR)/test_floating_region

IMAGE_OBJS := $(OBJDIR)/src/paint/image_buffer.o
PALETTE_OBJS := $(IMAGE_OBJS) $(OBJDIR)/src/paint/palette.o
CURSOR_OBJS := $(OBJDIR)/src/paint/cursor_asset.o \
	$(OBJDIR)/src/paint/indexed_image.o
CANVAS_OBJS := $(PALETTE_OBJS) \
	$(OBJDIR)/src/core/view_transform.o \
	$(OBJDIR)/src/paint/canvas_document.o \
	$(OBJDIR)/src/paint/floating_region_editor.o \
	$(OBJDIR)/src/paint/paint_session.o \
	$(OBJDIR)/src/paint/draw_buffer.o \
	$(OBJDIR)/src/paint/undo_history.o \
	$(OBJDIR)/src/tools/draw_tool.o

all: test

test: $(TEST_BINS)
	@set -e; for test in $(TEST_BINS); do \
		echo "== $$test =="; \
		$$test; \
	done

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

$(BUILDDIR)/test_dirty: $(OBJDIR)/tests/test_dirty.o $(IMAGE_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_draw_buffer: $(OBJDIR)/tests/test_draw_buffer.o \
		$(IMAGE_OBJS) $(OBJDIR)/src/paint/draw_buffer.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_undo: $(OBJDIR)/tests/test_undo.o $(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_clamp: $(OBJDIR)/tests/test_clamp.o $(CANVAS_OBJS) \
		$(OBJDIR)/src/tools/square_tool.o $(DOCTEST_OBJ)
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
		$(CANVAS_OBJS) $(OBJDIR)/src/tools/square_tool.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_canvas_resize: $(OBJDIR)/tests/test_canvas_resize.o \
		$(CANVAS_OBJS) $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/test_floating_region: $(OBJDIR)/tests/test_floating_region.o \
		$(CANVAS_OBJS) $(OBJDIR)/src/tools/selection_tool.o $(DOCTEST_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) -r $(BUILDDIR)

-include $(PROJECT_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(DOCTEST_OBJ:.o=.d)

.PHONY: all test clean
