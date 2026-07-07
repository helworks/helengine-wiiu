.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

HELENGINE_CORE_CPP_ROOT ?=
TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

TARGET := helengine_wiiu
BUILD := build
SOURCES := src src/platform/wiiu
# WiiUApplication.cpp, WiiUInputBackend.cpp, WiiUSceneBootstrap.cpp, and future runtime seam sources remain under src/platform/wiiu and are discovered through wildcard source enumeration.
DATA := data
SHADER_SOURCES := tools/wiiu-shaders
SHADER_COMPILER := tools/cafeglsl/glslcompiler.elf
REQUIRED_SHADER_BINFILES := diagnostic_square_shader.bin diagnostic_triangle_shader.bin scene_cube_flat_color_shader.bin scene_opaque_lit_shader.bin ui_quad_shader.bin
INCLUDES := src
CONTENT :=
APP_CONTENT := $(CONTENT)
ICON :=
TV_SPLASH :=
DRC_SPLASH :=
GENERATED_CONFIG := $(HELENGINE_CORE_CPP_ROOT)/helcpp_config.hpp
GENERATED_CORE_TRANSLATION_UNIT :=

CFLAGS := -g -Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS += $(INCLUDE) -D__WIIU__ -D__WUT__
CXXFLAGS := $(CFLAGS) -std=gnu++20
ASFLAGS := -g $(ARCH)
LDFLAGS := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS := -lwut

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=0
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=0
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=0
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=0
else
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_amalgamated.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_amalgamated.cpp
else ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_unity.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_unity.cpp
else
$(error HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp or helengine_core_unity.cpp)
endif
ifeq ($(wildcard $(GENERATED_CONFIG)),)
$(error HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp)
endif
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=1
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/GeneratedRuntimeModuleRegistration.hpp),)
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=1
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=1
else
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=0
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION=0
endif
endif

LIBDIRS := $(PORTLIBS) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(BUILD)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
	$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
	$(CURDIR)/$(SHADER_SOURCES) \
	$(if $(strip $(GENERATED_CORE_TRANSLATION_UNIT)),$(HELENGINE_CORE_CPP_ROOT))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp))) $(GENERATED_CORE_TRANSLATION_UNIT)
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
RAW_BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
BINFILES := $(sort $(filter-out $(REQUIRED_SHADER_BINFILES),$(RAW_BINFILES)) $(REQUIRED_SHADER_BINFILES))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD)

else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).wuhb

$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES)

%_shader.bin: $(TOPDIR)/$(SHADER_SOURCES)/%.vs $(TOPDIR)/$(SHADER_SOURCES)/%.ps
	@echo $(notdir $@)
	@$(TOPDIR)/$(SHADER_COMPILER) -ps $(TOPDIR)/$(SHADER_SOURCES)/$*.ps -vs $(TOPDIR)/$(SHADER_SOURCES)/$*.vs -o $@

%_bin.h %.bin.o: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)
endif
