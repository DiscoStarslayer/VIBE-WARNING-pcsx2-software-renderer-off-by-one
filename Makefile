# (AI-assisted) Standalone PS2SDK reproducer.
PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
EE_TOOL_PREFIX := $(PS2DEV)/ee/bin/mips64r5900el-ps2-elf-

EE_BIN = software-texture-cache-bug.elf
EE_OBJS = main.o
EE_LIBS = -ldraw -lgraph -lpacket -ldma
EE_CFLAGS = -std=gnu99 -Wextra

.PHONY: all clean
all: $(EE_BIN)
clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
