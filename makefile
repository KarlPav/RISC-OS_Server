# Use
# "make" will build the native version (POSIX)
# "make riscos" will build the RISC OS version - Need to check if the env needs setup seems no?
# "make clean" will remove all object files and binaries

# Output control
Q := @
ECHO := @echo

# Cross-compilation setup
GCCSDK_ROOT := $(HOME)/gccsdk
GCCSDK_CROSS := $(GCCSDK_ROOT)/cross/bin

# Default to native build
TARGET_OS ?= native

# Target-specific variables
ifeq ($(TARGET_OS),riscos)
    # RISC OS Toolchain
    CC := $(GCCSDK_CROSS)/arm-unknown-riscos-gcc
    LD := $(CC)
    OUTPUT_BINARY := !RunImageELF
    FINAL_BINARY := !RunImage
    CCFLAGS := -std=gnu99 -mlibscl -mhard-float -mthrowback -Wall -O2 \
               -fno-strict-aliasing -mpoke-function-name -D__riscos__
    LDFLAGS := -L$(GCCSDK_ROOT)/env/lib -lOSLibH32 -lSFLib32
else
    # Native POSIX build - no intermediate binary needed
    CC := gcc
    LD := gcc
    OUTPUT_BINARY := server  # Temporary intermediate file
    FINAL_BINARY :=
    CCFLAGS := -std=gnu99 -Wall -O2 -U__riscos__ -DPLATFORM_POSIX=1
    LDFLAGS :=
endif

# Directories
SRCDIR := c
HDRDIR := h
OBJDIR := o
PLATFORMDIR := $(if $(filter riscos,$(TARGET_OS)),riscos,posix)

# Source files
MAIN_SRCS := $(filter-out $(SRCDIR)/socket_utils_wrapper.c, $(wildcard $(SRCDIR)/*.c))

# Platform-specific implementation
PLATFORM_IMPL := $(PLATFORMDIR)/socket_utils_$(if $(filter riscos,$(TARGET_OS)),riscos,posix).c

# Object files
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MAIN_SRCS)) \
        $(OBJDIR)/socket_utils_wrapper.o \
        $(OBJDIR)/socket_utils_$(if $(filter riscos,$(TARGET_OS)),riscos,posix).o

# Include paths
INCLUDES := -I. -I$(HDRDIR) -I$(PLATFORMDIR)

# Ensure directories exist
$(shell mkdir -p $(OBJDIR) $(PLATFORMDIR))

.PHONY: all clean riscos native

all: $(TARGET_OS)

riscos:
	@$(MAKE) TARGET_OS=riscos all_target

native:
	@$(MAKE) TARGET_OS=native all_target

all_target: $(if $(FINAL_BINARY),$(FINAL_BINARY),$(OUTPUT_BINARY))

# Compilation rules
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(Q)$(CC) -c $(CCFLAGS) $(INCLUDES) $< -o $@
	$(ECHO) "Compiled: $< -> $@"

$(OBJDIR)/socket_utils_wrapper.o: c/socket_utils_wrapper.c
	$(Q)$(CC) -c $(CCFLAGS) $(INCLUDES) $< -o $@
	$(ECHO) "Compiled: $< -> $@"

$(OBJDIR)/socket_utils_riscos.o: riscos/socket_utils_riscos.c
	$(Q)$(CC) -c $(CCFLAGS) $(INCLUDES) -D__riscos__ $< -o $@
	$(ECHO) "Compiled: $< -> $@"

$(OBJDIR)/socket_utils_posix.o: posix/socket_utils_posix.c
	$(Q)$(CC) -c $(CCFLAGS) $(INCLUDES) $< -o $@
	$(ECHO) "Compiled: $< -> $@"

# Main build rule
ifeq ($(TARGET_OS),riscos)
$(FINAL_BINARY): $(OUTPUT_BINARY)
	$(ECHO) "Converting to AIF..."
	$(Q)$(GCCSDK_CROSS)/elf2aif $< $@ 2> aif_error.log || \
	(if grep -q "'$<' is not an ELF file" aif_error.log; then \
		mv -f $< $@; \
	else cat aif_error.log; false; fi)

$(OUTPUT_BINARY): $(OBJS)
	$(ECHO) "Linking $@..."
	$(Q)$(LD) $(CCFLAGS) $(LDFLAGS) -o $@ $^
else
# Native build - direct single-step build
$(OUTPUT_BINARY): $(OBJS)
	$(ECHO) "Linking $@..."
	$(Q)$(LD) $(CCFLAGS) $(LDFLAGS) -o $@ $^
endif

clean:
	rm -rf $(OBJDIR)/*.o $(OUTPUT_BINARY) $(FINAL_BINARY) aif_error.log