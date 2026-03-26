#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO := /opt/devkitpro
endif

ifeq ($(strip $(DEVKITARM)),)
export DEVKITARM := $(DEVKITPRO)/devkitARM
endif

ifeq ($(strip $(LIBNDS)),)
export LIBNDS := $(DEVKITPRO)/libnds
endif

ifeq ($(strip $(PORTLIBS)),)
export PORTLIBS := $(DEVKITPRO)/portlibs/nds
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# Project settings
#---------------------------------------------------------------------------------
TARGET   := Minecraft
BUILD    := build
SOURCES  := source
INCLUDES := include build
DATA     :=
GRAPHICS :=
AUDIO    :=
ICON     := icon.bmp
NITRO    :=

#---------------------------------------------------------------------------------
# Code generation options
#---------------------------------------------------------------------------------
ARCH := -march=armv5te -mtune=arm946e-s -marm

CFLAGS := -g -Wall -Wextra -O3 -ffast-math -fomit-frame-pointer -ffunction-sections -fdata-sections \
	$(ARCH)
CFLAGS += $(INCLUDE) -DARM9 -D__NDS__
CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Libraries
#---------------------------------------------------------------------------------
LIBS := -lnds9 -lfat

# Top-level library roots (must contain include/ and lib/)
LIBDIRS := $(LIBNDS)

#---------------------------------------------------------------------------------
# Build rules
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(BINFILES:.bin=.o) \
	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)

# Critical for ds_rules: explicit linker search paths.
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean $(BUILD) gen_textures gen_menu_assets

all: gen_textures gen_menu_assets $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

gen_textures:
	@echo Generating texture data from textures/*.png ...
	@python3 $(CURDIR)/tools/generate_texture_data.py

gen_menu_assets:
	@echo Generating menu assets from bg/logo/font ...
	@python3 $(CURDIR)/tools/generate_menu_assets.py

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).nds : $(OUTPUT).elf

$(OUTPUT).elf : $(OFILES)

%.o : %.bin
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
