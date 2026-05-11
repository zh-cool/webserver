CROSS := /home/austin/Project/HuaWei/HSAN19/sdk/turnkey/tmp/tools/cross_toolchain/arm-mix510-linux/bin/arm-mix510-linux-
CC    := $(CROSS)gcc
STRIP := $(CROSS)strip
CFLAGS  := -Os -s -Wall -Wextra -Werror -Wno-unused-parameter -ffunction-sections -fdata-sections
CFLAGS += -DMG_IO_SIZE=1048576 -DMG_MAX_RECV_SIZE=110000000UL
LDFLAGS := -Wl,--gc-sections -Wl,-s

# Shared source files in ../c/
SHARED := ../c/mongoose.c ../c/router_led.c

all: admin_server_arm

admin_server_arm: server.c $(SHARED) ../c/mongoose.h ../c/router_led.h
	$(CC) $(CFLAGS) server.c $(SHARED) $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo ""
	$(CROSS)size $@

clean:
	rm -f admin_server_arm

.PHONY: all clean
