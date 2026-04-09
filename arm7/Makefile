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

include $(DEVKITARM)/ds_rules

TARGET   := audio_arm7
BUILD    := build
SOURCES  := source
INCLUDES := ../include

ARCH := -mthumb -mthumb-interwork -mcpu=arm7tdmi -mtune=arm7tdmi
CFLAGS := -g -Wall -Wextra -O3 -ffast-math -fomit-frame-pointer -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += $(INCLUDE) -DARM7 -D__NDS__
CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=ds_arm7.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lnds7
LIBDIRS := $(LIBNDS)

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
	$(if $(wildcard $(DEVKITPRO)/calico/include),-I$(DEVKITPRO)/calico/include)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean $(BUILD)

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

clean:
	@rm -fr $(BUILD) $(TARGET).elf

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).elf : $(OFILES)

%.o : %.bin
	$(bin2o)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
