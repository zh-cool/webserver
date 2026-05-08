#include "router_led.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

uint8_t rl_checksum(const uint8_t *buf, size_t len)
{
	uint8_t sum = 0;
	size_t i;

	for (i = 0; i < len; i++)
		sum += buf[i];

	return sum;
}

void rl_build(uint8_t frame[RL_FRAME_LEN], enum rl_led_state wan, enum rl_led_state lan, enum rl_led_state wifi)
{
	uint32_t data = 0;

	data |= ((uint32_t)wan & 0x0F) << RL_WAN_LED;
	data |= ((uint32_t)lan & 0x0F) << RL_LAN_LED;
	data |= ((uint32_t)wifi & 0x0F) << RL_WIFI_LED;

	frame[0] = RL_HEADER0;
	frame[1] = RL_HEADER1;
	frame[2] = RL_LEN;
	frame[3] = RL_CMD_SET;
	frame[4] = (data >> 16) & 0xFF;
	frame[5] = (data >> 8) & 0xFF;
	frame[6] = (data >> 0) & 0xFF;
	frame[7] = rl_checksum(frame, 7);
}

int rl_parse(const uint8_t frame[RL_FRAME_LEN], enum rl_led_state *wan, enum rl_led_state *lan, enum rl_led_state *wifi)
{
	uint32_t data;

	if (frame[0] != RL_HEADER0 || frame[1] != RL_HEADER1)
		return -EINVAL;
	if (frame[2] != RL_LEN)
		return -EINVAL;
	if (frame[3] != RL_CMD_SET && frame[3] != RL_CMD_REPLY)
		return -EINVAL;

	if (rl_checksum(frame, 7) != frame[7])
		return -EINVAL;

	data = ((uint32_t)(frame[4]) << 16) | ((uint32_t)frame[5] << 8) | (uint32_t)(frame[6]);

	if (wan)
		*wan = (data >> RL_WAN_LED) & 0x0F;
	if (lan)
		*lan = (data >> RL_LAN_LED) & 0x0F;
	if (wifi)
		*wifi = (data >> RL_WIFI_LED) & 0x0F;

	return 0;
}

int rl_serial_open(const char *device)
{
	int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	struct termios tio;

	if (fd < 0)
		return -1;

	memset(&tio, 0, sizeof(tio));
	tio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
	tio.c_iflag = IGNPAR;
	tio.c_oflag = 0;
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1;

	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &tio) != 0) {
		close(fd);
		return -1;
	}

	/* Clear non-blocking for normal read/write */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

	return fd;
}

int rl_serial_send(int fd, const uint8_t frame[RL_FRAME_LEN])
{
	ssize_t n;

	n = write(fd, frame, RL_FRAME_LEN);
	tcdrain(fd);

	return n == RL_FRAME_LEN ? (int)n : -1;
}

int rl_serial_recv(int fd, uint8_t frame[RL_FRAME_LEN], int timeout_ms)
{
	struct timeval tv = {
		.tv_sec = timeout_ms / 1000,
		.tv_usec = (timeout_ms % 1000) * 1000,
	};
	fd_set fds;
	size_t pos = 0;
	int ret;

	while (pos < RL_FRAME_LEN) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		ret = select(fd + 1, &fds, NULL, NULL, &tv);
		if (ret == 0)
			return -ETIMEDOUT;
		if (ret < 0)
			return -errno;

		ret = read(fd, frame + pos, RL_FRAME_LEN - pos);
		if (ret <= 0)
			return -1;

		pos += ret;
	}

	return (int)pos;
}
