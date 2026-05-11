#include "../c/mongoose.h"
#include "../c/router_led.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *s_listen = "http://0.0.0.0:8080";
static const char *s_web_dir = "/web";
static const char *s_led_dev = "/dev/ttyS0";
static int s_led_fd = -1;
static enum rl_led_state s_wan = RL_OFF, s_lan = RL_OFF, s_wifi = RL_OFF;

/* ---------- helpers ---------- */

static int sh(const char *cmd, char *out, size_t len)
{
	FILE *fp = popen(cmd, "r");
	if (!fp) { out[0] = '\0'; return -1; }
	size_t n = fread(out, 1, len - 1, fp);
	out[n] = '\0';
	if (n > 0 && out[n - 1] == '\n') out[n - 1] = '\0';
	pclose(fp);
	return (int)n;
}

static void json_reply(struct mg_connection *c, int code, const char *json)
{
	mg_http_reply(c, code, "Content-Type: application/json\r\n", "%s", json);
}

/* extract JSON int value: look for "key": <num> */
static int json_int(const char *b, size_t bl, const char *key, int defval)
{
	size_t kl = strlen(key);
	for (size_t i = 0; i + kl < bl; i++) {
		if (b[i] != '"') continue;
		if (strncmp(b + i + 1, key, kl) != 0) continue;
		if (b[i + 1 + kl] != '"') continue;
		const char *n = b + i + kl + 2;
		while (n < b + bl && (*n == ':' || *n == ' ')) n++;
		if (n < b + bl && *n >= '0' && *n <= '9')
			return (int)strtol(n, NULL, 10);
	}
	return defval;
}

/* ---------- API handlers ---------- */

static void api_status(struct mg_connection *c)
{
	char up[32] = "", mt[32] = "", mf[32] = "", hn[64] = "", ver[128] = "";
	sh("awk '{print int($1)}' /proc/uptime", up, sizeof(up));
	sh("free | awk '/Mem:/{print $2}'", mt, sizeof(mt));
	sh("free | awk '/Mem:/{print $4}'", mf, sizeof(mf));
	sh("cat /proc/sys/kernel/hostname", hn, sizeof(hn));
	sh("head -1 /proc/version | awk '{print $1,$2,$3}'", ver, sizeof(ver));
	mg_http_reply(c, 200, "Content-Type: application/json\r\n",
		"{\"uptime\":%s,\"mem_total\":%s,\"mem_free\":%s,\"hostname\":\"%s\",\"version\":\"%s\"}\n",
		up, mt, mf, hn, ver);
}

static void api_wifi_get(struct mg_connection *c)
{
	char s24[64] = "", s5[64] = "", c24[8] = "0", c5[8] = "0";
	sh("iwconfig vap0 2>/dev/null|grep ESSID|cut -d: -f2|tr -d '\"'", s24, sizeof(s24));
	sh("iwconfig vap8 2>/dev/null|grep ESSID|cut -d: -f2|tr -d '\"'", s5, sizeof(s5));
	sh("iwconfig vap0 2>/dev/null|grep -o 'Channel:[0-9]*'|cut -d: -f2", c24, sizeof(c24));
	sh("iwconfig vap8 2>/dev/null|grep -o 'Channel:[0-9]*'|cut -d: -f2", c5, sizeof(c5));
	mg_http_reply(c, 200, "Content-Type: application/json\r\n",
		"{\"wifi24\":{\"ssid\":\"%s\",\"channel\":%s},\"wifi5\":{\"ssid\":\"%s\",\"channel\":%s}}\n",
		s24, c24, s5, c5);
}

static void api_wifi_post(struct mg_connection *c, struct mg_http_message *hm)
{
	/* TODO: apply settings via hostapd_cli or hi_cfm */
	(void)hm;
	json_reply(c, 200, "{\"status\":\"ok\"}\n");
}

static void api_network_get(struct mg_connection *c)
{
	char ip[32] = "", mask[32] = "", dhcp[64] = "";
	sh("ifconfig br0|grep 'inet addr'|cut -d: -f2|awk '{print $1}'", ip, sizeof(ip));
	sh("ifconfig br0|grep 'Mask'|cut -d: -f4", mask, sizeof(mask));
	/* use first dnsmasq line as DHCP range hint */
	sh("grep 'dhcp-range' /tmp/etc/dnsmasq.conf 2>/dev/null|head -1|awk -F, '{print $2\",\"$3}'", dhcp, sizeof(dhcp));
	if (!ip[0]) strcpy(ip, "192.168.11.1");
	if (!mask[0]) strcpy(mask, "255.255.255.0");
	if (!dhcp[0]) strcpy(dhcp, "192.168.11.2,192.168.11.254");
	mg_http_reply(c, 200, "Content-Type: application/json\r\n",
		"{\"lan_ip\":\"%s\",\"netmask\":\"%s\",\"dhcp_start\":\"%s\"}\n",
		ip, mask, dhcp);
}

static void api_network_post(struct mg_connection *c, struct mg_http_message *hm)
{
	/* TODO: apply via ifconfig + dnsmasq config rewrite */
	(void)hm;
	json_reply(c, 200, "{\"status\":\"ok\"}\n");
}

static void api_devices(struct mg_connection *c)
{
	char jbuf[4096];
	size_t pos = 0;
	int first = 1;
	FILE *fp = fopen("/proc/net/arp", "r");
	if (!fp) { json_reply(c, 200, "[]\n"); return; }
	char line[256];
	fgets(line, sizeof(line), fp); /* skip header */
	while (fgets(line, sizeof(line), fp) && pos < sizeof(jbuf) - 100) {
		char ip[32], mac[32];
		/* /proc/net/arp: IP hw_type flags HW_address mask device */
		if (sscanf(line, "%31s %*s %*s %31s", ip, mac) != 2) continue;
		int n = snprintf(jbuf + pos, sizeof(jbuf) - pos,
			"%s{\"ip\":\"%s\",\"mac\":\"%s\"}",
			first ? "" : ",", ip, mac);
		if (n > 0 && (size_t)n < sizeof(jbuf) - pos) {
			pos += n;
			first = 0;
		}
	}
	fclose(fp);
	if (!first)
		mg_http_reply(c, 200, "Content-Type: application/json\r\n", "[%s]\n", jbuf);
	else
		json_reply(c, 200, "[]\n");
}

static void api_led_get(struct mg_connection *c)
{
	mg_http_reply(c, 200, "Content-Type: application/json\r\n",
		"{\"wan\":%d,\"lan\":%d,\"wifi\":%d}\n",
		(int)s_wan, (int)s_lan, (int)s_wifi);
}

static void api_led_post(struct mg_connection *c, struct mg_http_message *hm)
{
	int wv = json_int(hm->body.buf, hm->body.len, "wan", (int)s_wan);
	int lv = json_int(hm->body.buf, hm->body.len, "lan", (int)s_lan);
	int fv = json_int(hm->body.buf, hm->body.len, "wifi", (int)s_wifi);

	wv = (wv < 1 || wv > 15) ? RL_KEEP : wv;
	lv = (lv < 1 || lv > 15) ? RL_KEEP : lv;
	fv = (fv < 1 || fv > 15) ? RL_KEEP : fv;

	s_wan = (enum rl_led_state)wv;
	s_lan = (enum rl_led_state)lv;
	s_wifi = (enum rl_led_state)fv;

	if (s_led_fd >= 0) {
		uint8_t frame[RL_FRAME_LEN];
		rl_build(frame, s_wan, s_lan, s_wifi);
		rl_serial_send(s_led_fd, frame);
		rl_serial_recv(s_led_fd, frame, 2000);
	}

	api_led_get(c);
}

static void api_system(struct mg_connection *c, struct mg_http_message *hm)
{
	if (mg_match(hm->uri, mg_str("/api/system/reboot"), NULL)) {
		json_reply(c, 200, "{\"status\":\"rebooting\"}\n");
		system("sleep 1 && reboot &");
	} else if (mg_match(hm->uri, mg_str("/api/system/reset"), NULL)) {
		json_reply(c, 200, "{\"status\":\"resetting\"}\n");
		system("hi_cfm test restore 2>/dev/null; sleep 1 && reboot &");
	} else {
		json_reply(c, 404, "{\"error\":\"unknown action\"}\n");
	}
}

/* ---------- static file server ---------- */

static void serve_web(struct mg_connection *c, struct mg_http_message *hm)
{
	struct mg_str uri = hm->uri;
	if (uri.len < 6) {
		mg_http_reply(c, 302, "Location: /web/index.html\r\n", "");
		return;
	}
	const char *name = uri.buf + 5; /* skip "/web" */
	size_t name_len = uri.len - 5;
	/* skip leading slash if present */
	if (*name == '/') { name++; name_len--; }

	char fname[256];
	mg_url_decode(name, name_len, fname, sizeof(fname), 0);

	char fpath[512];
	snprintf(fpath, sizeof(fpath), "%s/%s", s_web_dir, fname);

	struct mg_http_serve_opts opts = { .root_dir = s_web_dir, .fs = NULL };
	mg_http_serve_file(c, hm, fpath, &opts);
}

/* ---------- main routing ---------- */

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev != MG_EV_HTTP_MSG) return;

	struct mg_http_message *hm = (struct mg_http_message *)ev_data;
	struct mg_str u = hm->uri;

	if (u.len >= 4 && memcmp(u.buf, "/web", 4) == 0) {
		serve_web(c, hm);
	} else if (u.len >= 5 && memcmp(u.buf, "/api/", 5) == 0) {
		if (mg_match(u, mg_str("/api/status"), NULL))
			api_status(c);
		else if (mg_match(u, mg_str("/api/wifi"), NULL))
			mg_match(hm->method, mg_str("POST"), NULL) ? api_wifi_post(c, hm) : api_wifi_get(c);
		else if (mg_match(u, mg_str("/api/network"), NULL))
			mg_match(hm->method, mg_str("POST"), NULL) ? api_network_post(c, hm) : api_network_get(c);
		else if (mg_match(u, mg_str("/api/devices"), NULL))
			api_devices(c);
		else if (mg_match(u, mg_str("/api/led"), NULL))
			mg_match(hm->method, mg_str("POST"), NULL) ? api_led_post(c, hm) : api_led_get(c);
		else if (mg_match(u, mg_str("/api/system/"), NULL))
			api_system(c, hm);
		else
			json_reply(c, 404, "{\"error\":\"unknown api\"}\n");
	} else if (mg_match(u, mg_str("/"), NULL)) {
		mg_http_reply(c, 302, "Location: /web/index.html\r\n", "");
	} else {
		mg_http_reply(c, 404, "", "Not Found\n");
	}
}
/* ---------- main ---------- */

int main(void)
{
	char *env;
	if ((env = getenv("ADDR")) != NULL)  s_listen = env;
	if ((env = getenv("WEB_DIR")) != NULL) s_web_dir = env;
	if ((env = getenv("LED_DEV")) != NULL) s_led_dev = env;

	mkdir(s_web_dir, 0755);

	s_led_fd = rl_serial_open(s_led_dev);
	if (s_led_fd < 0)
		fprintf(stderr, "WARNING: cannot open %s, LED disabled\n", s_led_dev);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	mg_http_listen(&mgr, s_listen, ev_handler, NULL);
	printf("admin server on %s, web dir: %s\n", s_listen, s_web_dir);
	for (;;) mg_mgr_poll(&mgr, 1000);

	if (s_led_fd >= 0) close(s_led_fd);
	mg_mgr_free(&mgr);
	return 0;
}
