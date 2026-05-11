CROSS := /home/austin/Project/HuaWei/HSAN19/sdk/turnkey/tmp/tools/cross_toolchain/arm-mix510-linux/bin/arm-mix510-linux-
CC    := $(CROSS)gcc
STRIP := $(CROSS)strip
CFLAGS  := -Os -s -Wall -Wextra -Werror -Wno-unused-parameter -ffunction-sections -fdata-sections
CFLAGS += -DMG_IO_SIZE=1048576 -DMG_MAX_RECV_SIZE=110000000UL
LDFLAGS := -Wl,--gc-sections -Wl,-s

all: webserver_arm led_test_arm led_auto_test_arm admin_server_arm

webserver_arm: main.c mongoose.c mongoose.h router_led.c router_led.h
	$(CC) $(CFLAGS) main.c mongoose.c router_led.c $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo ""
	$(CROSS)size $@

led_test_arm: led_test.c router_led.c router_led.h
	$(CC) $(CFLAGS) led_test.c router_led.c $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo ""
	$(CROSS)size $@

led_auto_test_arm: led_auto_test.c router_led.c router_led.h
	$(CC) $(CFLAGS) led_auto_test.c router_led.c $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo ""
	$(CROSS)size $@

admin_server_arm: server.c mongoose.c mongoose.h router_led.c router_led.h
	$(CC) $(CFLAGS) server.c mongoose.c router_led.c $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo ""
	$(CROSS)size $@

clean:
	rm -f webserver_arm led_test_arm led_auto_test_arm admin_server_arm

.PHONY: all clean
