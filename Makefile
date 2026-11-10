CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -fdiagnostics-color=always
LDFLAGS :=
LDLIBS :=

ROOTDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SRCDIR := $(ROOTDIR)/src
INCDIR := $(ROOTDIR)/inc
BUILDDIR := $(ROOTDIR)/build

BUILD_TYPE ?= debug
ifeq ($(filter $(BUILD_TYPE),debug release),)
$(error BUILD_TYPE must be 'debug' or 'release')
endif

ifeq ($(BUILD_TYPE),release)
OPTFLAGS := -O2 -DNDEBUG
TARGET := QtreGBPaint-release
OBJDIR := $(ROOTDIR)/obj-release
DEPDIR := $(ROOTDIR)/deps-release
else
OPTFLAGS := -g -O0
TARGET := QtreGBPaint
OBJDIR := $(ROOTDIR)/obj
DEPDIR := $(ROOTDIR)/deps
endif

TARGET_PATH := $(BUILDDIR)/$(TARGET)
IMGUIFILE_DIALOG_LIB := $(ROOTDIR)/lib/ImGuiFileDialog/libImGuiFileDialog.a

SRCS := $(shell find $(SRCDIR) -name '*.cpp')
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:$(SRCDIR)/%.cpp=$(DEPDIR)/%.d)

# ==== Libraries and includes ====
CXXFLAGS += $(OPTFLAGS) \
	-I$(INCDIR) \
	-I$(ROOTDIR)/lib/tinyfiledialogs/src \
	-I$(ROOTDIR)/lib/imgui/src \
	-I$(ROOTDIR)/lib/imgui/src/backends \
	-I$(ROOTDIR)/lib/ImGuiFileDialog \
	$(shell sdl2-config --cflags)
LDLIBS += \
	$(IMGUIFILE_DIALOG_LIB) \
	$(ROOTDIR)/lib/imgui/libimgui.a \
	$(ROOTDIR)/lib/tinyfiledialogs/libtinyfiledialog.a \
	$(shell sdl2-config --libs) -pthread

all: $(TARGET_PATH)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR) $(DEPDIR)
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(DEPDIR)/$*)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c $< -o $@

$(TARGET_PATH): $(OBJS) $(IMGUIFILE_DIALOG_LIB) | $(BUILDDIR)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(IMGUIFILE_DIALOG_LIB):
	$(MAKE) -C $(ROOTDIR)/lib/ImGuiFileDialog

$(OBJDIR) $(BUILDDIR) $(DEPDIR):
	mkdir -p $@

test:
	$(MAKE) -C tests

test-asan:
	$(MAKE) -C tests test-asan

test-ubsan:
	$(MAKE) -C tests test-ubsan

test-sanitize:
	$(MAKE) -C tests test-sanitize

# Full verification: normal suite plus every sanitizer variant. Run it with
# `make -j$(nproc) test-full`; the recursive makes share the jobserver.
test-full: test test-asan test-ubsan test-sanitize

run: $(TARGET_PATH)
	$(TARGET_PATH)

release:
	$(MAKE) BUILD_TYPE=release all

run-release:
	$(MAKE) BUILD_TYPE=release run

clean:
	$(RM) -r $(ROOTDIR)/obj $(ROOTDIR)/obj-release \
		$(ROOTDIR)/deps $(ROOTDIR)/deps-release $(BUILDDIR)
	$(MAKE) -C tests clean

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

.PHONY: all clean test test-full test-asan test-ubsan test-sanitize run release run-release
