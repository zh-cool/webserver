#include "router_led.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct test_case {
	enum rl_led_state wan;
	enum rl_led_state lan;
	enum rl_led_state wifi;
	const char *desc;
};

static const struct test_case tests[] = {
	{ RL_ON, RL_OFF, RL_OFF, "Only Wan On" },
	{ RL_OFF, RL_ON, RL_OFF, "Only Lan On" },
	{ RL_OFF, RL_OFF, RL_ON, "Only Wifi On" },
	{ RL_ON, RL_ON, RL_ON, "All On" },
	{ RL_OFF, RL_OFF, RL_OFF, "All Off" },
	{ RL_SLOW_FLS, RL_OFF, RL_OFF, "Wan Slow Flash, others Off" },
	{ RL_OFF, RL_SLOW_FLS, RL_OFF, "Lan Slow Flash, others Off" },
	{ RL_OFF, RL_OFF, RL_SLOW_FLS, "Wifi Slow Flash, others Off" },
	{ RL_FAST_FLS, RL_OFF, RL_OFF, "Wan Fast Flash, others Off" },
	{ RL_ON, RL_SLOW_FLS, RL_FAST_FLS, "Mixed: Wan=On Lan=Slow Wifi=Fast" },
	{ RL_KEEP, RL_KEEP, RL_KEEP, "All Keep (no change)" },
};

static const int num_tests = sizeof(tests) / sizeof(tests[0]);

static const char *led_name(enum rl_led_state s)
{
	switch (s) {
	case RL_OFF:
		return "Off";
	case RL_ON:
		return "On";
	case RL_SLOW_FLS:
		return "Slow Flash";
	case RL_FAST_FLS:
		return "Fast Flash";
	default:
		return "Keep";
	}
}

static void hex_dump(const uint8_t *buf, int len)
{
	for (int i = 0; i < len; i++)
		printf("%02X ", buf[i]);
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/ttyS0";
	int delay = argc > 2 ? atoi(argv[2]) : 500;

	int fd = rl_serial_open(dev);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s\n", dev);
		return 1;
	}
	printf("Serial port: %s\n\n", dev);
	printf("Manual LED test — %d test cases\n", num_tests);
	printf("After each command, visually check the LEDs and input:\n");
	printf("  y = pass, n = fail, q = quit\n\n");

	int passed = 0, failed = 0, skipped = 0;

	for (int i = 0; i < num_tests; i++) {
		uint8_t frame[RL_FRAME_LEN];
		uint8_t reply[RL_FRAME_LEN];
		char input[16];

		rl_build(frame, tests[i].wan, tests[i].lan, tests[i].wifi);

		printf("[%d/%d] %s\n", i + 1, num_tests, tests[i].desc);
		printf("  Expect: Wan=%-11s Lan=%-11s Wifi=%s\n", led_name(tests[i].wan), led_name(tests[i].lan), led_name(tests[i].wifi));
		printf("  TX: ");
		hex_dump(frame, RL_FRAME_LEN);
		printf("\n");

		rl_serial_send(fd, frame);
		int ret = rl_serial_recv(fd, reply, 3000);
		if (ret == RL_FRAME_LEN) {
			printf("  RX: ");
			hex_dump(reply, RL_FRAME_LEN);
			if (reply[3] != RL_CMD_REPLY || rl_checksum(reply, 7) != reply[7])
				printf("  [reply invalid]");
			else
				printf("  [reply OK]");
			printf("\n");
		} else {
			printf("  RX: none (ret=%d)\n", ret);
		}

		printf("  Correct? (y/n/q): ");
		fflush(stdout);
		if (!fgets(input, sizeof(input), stdin))
			break;

		switch (input[0]) {
		case 'y':
		case 'Y':
			passed++;
			printf("  >> PASS\n");
			break;
		case 'n':
		case 'N':
			failed++;
			printf("  >> FAIL\n");
			break;
		case 'q':
		case 'Q':
			skipped += num_tests - i - 1;
			printf("  >> QUIT\n");
			goto done;
		default:
			skipped++;
			printf("  >> SKIP\n");
			break;
		}
		printf("\n");
		usleep(delay * 1000);
	}

	close(fd);
done:
	printf("====================\n");
	printf("Passed:  %d\n", passed);
	printf("Failed:  %d\n", failed);
	printf("Skipped: %d\n", skipped);
	printf("====================\n");
	return failed > 0 ? 1 : 0;
}
