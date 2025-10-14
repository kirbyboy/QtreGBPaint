CXX := g++
CXXFLAGS  := -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -fdiagnostics-color=always

ROOTDIR := $(shell pwd)
SRCDIR := $(ROOTDIR)/src
INCDIR := $(ROOTDIR)/inc
OBJDIR := $(ROOTDIR)/obj
BUILDDIR := $(ROOTDIR)/build
DEPDIR := $(ROOTDIR)/deps

SRCS := $(shell find $(SRCDIR) -name '*.cpp')
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:$(SRCDIR)/%.cpp=$(DEPDIR)/%.d)

# ==== Libraries and includes ====
CXXFLAGS += -I$(INCDIR) -Ilib/tinyfiledialogs/src -Ilib/imgui/src -Ilib/imgui/src/backends `sdl2-config --cflags`
LDFLAGS  := lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a `sdl2-config --libs`

TARGET := QtreGBPaint

all: $(BUILDDIR)/$(TARGET)
	
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR) $(DEPDIR)
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(DEPDIR)/$*)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@$(CXX) $(CXXFLAGS) -MM -MP -MT $@ $< > $(DEPDIR)/$*.d	

$(BUILDDIR)/$(TARGET): $(OBJS) | $(BUILDDIR) 
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

$(OBJDIR) $(BUILDDIR) $(DEPDIR):
	mkdir -p $@
	
clean:
	$(RM) -r $(OBJDIR) $(DEPDIR) $(BUILDDIR)
	
ifneq ($(MAKECMDGOALS),clean)	
-include $(DEPS)
endif

.PHONY: clean all
