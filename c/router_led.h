#ifndef ROUTER_LED_H
#define ROUTER_LED_H

#include <stddef.h>
#include <stdint.h>

/* Protocol constants */
#define RL_FRAME_LEN 8 /* A5 5A 04 CMD D0 D1 D2 SUM */
#define RL_HEADER0 0xA5
#define RL_HEADER1 0x5A
#define RL_LEN 0x04
#define RL_CMD_SET 0x01 /* Host -> Device: set LED */
#define RL_CMD_REPLY 0x81 /* Device -> Host: reply */

/* LED identifiers (bit position) */
#define RL_WAN_LED 0 /* b0~3 */
#define RL_LAN_LED 4 /* b4~7 */
#define RL_WIFI_LED 8 /* b8~11 */

/* LED states */
enum rl_led_state {
	RL_OFF = 1, /* Off */
	RL_ON = 2, /* On */
	RL_SLOW_FLS = 3, /* Slow flash */
	RL_FAST_FLS = 4, /* Fast flash */
	RL_KEEP = 0x0F, /* Keep current state */
};

struct rl_frame {
	uint8_t header0;
	uint8_t header1;
	uint8_t len;
	uint8_t cmd;
	uint8_t data[3];
	uint8_t checksum;
} __attribute__((packed));

/**
 * rl_checksum - Calculate 8-bit sum of raw bytes
 */
uint8_t rl_checksum(const uint8_t *buf, size_t len);

/**
 * rl_build - Build a command frame from per-LED states.
 * @frame[out]  Pre-allocated 7-byte buffer
 * @wan:        Wan LED state
 * @lan:        Lan LED state
 * @wifi:       Wifi LED state
 */
void rl_build(uint8_t frame[RL_FRAME_LEN], enum rl_led_state wan, enum rl_led_state lan, enum rl_led_state wifi);

/**
 * rl_parse - Validate and parse a received frame.
 * @frame[in]       Raw frame buffer (7 bytes)
 * @wan[out]        Parsed Wan LED state, may be NULL
 * @lan[out]        Parsed Lan LED state, may be NULL
 * @wifi[out]       Parsed Wifi LED state, may be NULL
 * @return          0 on success, negative on error
 */
int rl_parse(const uint8_t frame[RL_FRAME_LEN], enum rl_led_state *wan, enum rl_led_state *lan, enum rl_led_state *wifi);

/**
 * rl_serial_open - Open and configure serial port.
 * @device:         e.g. "/dev/ttyS0"
 * @return          fd on success, -1 on error
 */
int rl_serial_open(const char *device);

/**
 * rl_serial_send - Send a frame via serial port.
 * @return          Bytes written, <0 on error
 */
int rl_serial_send(int fd, const uint8_t frame[RL_FRAME_LEN]);

/**
 * rl_serial_recv - Read a frame from serial port (blocking with timeout).
 * @timeout_ms:     Max wait in milliseconds
 * @return          Bytes read (should be RL_FRAME_LEN), <0 on error
 */
int rl_serial_recv(int fd, uint8_t frame[RL_FRAME_LEN], int timeout_ms);

#endif /* ROUTER_LED_H */