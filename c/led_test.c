#include "router_led.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	const char *dev = "/dev/ttyS0";
	int fd;
	uint8_t frame[RL_FRAME_LEN];
	uint8_t reply[RL_FRAME_LEN];

	fd = rl_serial_open(dev);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s\n", dev);
		return 1;
	}
	printf("Serial port opened: %s\n", dev);

	/* Turn all LEDs on */
	rl_build(frame, RL_ON, RL_ON, RL_ON);
	printf("TX: ");
	for (int i = 0; i < RL_FRAME_LEN; i++)
		printf("%02X ", frame[i]);
	printf("\n");

	if (rl_serial_send(fd, frame) < 0) {
		fprintf(stderr, "Send failed\n");
		close(fd);
		return 1;
	}

	/* Wait for reply */
	if (rl_serial_recv(fd, reply, 2000) == RL_FRAME_LEN) {
		printf("RX: ");
		for (int i = 0; i < RL_FRAME_LEN; i++)
			printf("%02X ", reply[i]);
		printf("\n");
	}

	/* Turn Wan off, Lan on, Wifi slow flash */
	rl_build(frame, RL_OFF, RL_ON, RL_SLOW_FLS);
	printf("TX: ");
	for (int i = 0; i < RL_FRAME_LEN; i++)
		printf("%02X ", frame[i]);
	printf("\n");

	rl_serial_send(fd, frame);
	rl_serial_recv(fd, reply, 2000);

	close(fd);
	return 0;
}
