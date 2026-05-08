#include "mongoose.h"
#include "router_led.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *s_data_dir = "./data";
static const char *s_listen_on = "http://0.0.0.0:8080";
static const char *s_led_dev = "/dev/ttyS0";
static const size_t s_max_upload = 100UL * 1024UL * 1024UL; // 100 MB
static int s_led_fd = -1;

/* ---------- HTML pages ---------- */

static const char *s_page_html =
	"<!DOCTYPE html>\n"
	"<html><head><meta charset=\"utf-8\">\n"
	"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
	"<title>File Server</title>\n"
	"<style>\n"
	"*{box-sizing:border-box}\n"
	"body{font-family:system-ui,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5}\n"
	"h1{color:#333}.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 1px 3px rgba(0,0,0,0.1);margin-bottom:20px}\n"
	"input[type=file]{display:block;margin:10px 0}\n"
	"button{background:#4f46e5;color:#fff;border:none;padding:10px 20px;border-radius:6px;cursor:pointer}\n"
	"button:hover{background:#4338ca}#st{margin-top:10px;white-space:pre-wrap}\n"
	"ul{list-style:none;padding:0}\n"
	"li{padding:8px 0;border-bottom:1px solid #eee;display:flex;justify-content:space-between}\n"
	"a{color:#4f46e5;text-decoration:none}a:hover{text-decoration:underline}\n"
	".sz{color:#666;font-size:0.9em}\n"
	"</style></head><body>\n"
	"<h1>File Server</h1>\n"
	"<div class=\"card\"><h2>Upload</h2>\n"
	"<form id=\"fm\" enctype=\"multipart/form-data\">\n"
	"<input type=\"file\" name=\"file\" id=\"file\" required>\n"
	"<button type=\"submit\">Upload</button></form>\n"
	"<div id=\"st\"></div></div>\n"
	"<div class=\"card\"><h2>Files</h2>\n"
	"<ul id=\"fl\"><li>Loading...</li></ul></div>\n"
	"<p><a href=\"/led\">LED Control</a></p>\n"
	"<script>\n"
	"const sz=b=>b>=1073741824?(b/1073741824).toFixed(1)+' GB':b>=1048576?(b/1048576).toFixed(1)+' MB':b>=1024?(b/1024).toFixed(1)+' KB':b+' B';\n"
	"document.getElementById('fm').onsubmit=async function(e){\n"
	"e.preventDefault();const st=document.getElementById('st');\n"
	"const fd=new FormData();fd.append('file',document.getElementById('file').files[0]);\n"
	"st.textContent='Uploading...';\n"
	"try{const r=await fetch('/upload',{method:'POST',body:fd});\n"
	"st.textContent=r.ok?'OK: '+await r.text():'Error: '+await r.text();ls();\n"
	"}catch(e){st.textContent='Error: '+e;}};\n"
	"async function ls(){try{\n"
	"const r=await fetch('/list');const a=await r.json();\n"
	"const ul=document.getElementById('fl');\n"
	"ul.innerHTML=a.length===0?'<li>No files</li>':a.map(f=>'<li><a href=\"/f/'+f.n+'\">'+f.n+'</a><span class=\"sz\">'+sz(f.s)+'</span></li>').join('');\n"
	"}catch(e){}}ls();\n"
	"</script></body></html>";

static const char *s_led_html = "<!DOCTYPE html>\n"
				"<html><head><meta charset=\"utf-8\">\n"
				"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
				"<title>LED Control</title>\n"
				"<style>\n"
				"*{box-sizing:border-box}\n"
				"body{font-family:system-ui,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5}\n"
				"h1{color:#333}.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 1px 3px rgba(0,0,0,0.1);margin-bottom:20px}\n"
				"label{display:inline-block;width:80px;font-weight:bold}\n"
				"select{width:140px;padding:6px;margin:5px 0;border-radius:4px}\n"
				"button{background:#4f46e5;color:#fff;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;margin-top:10px}\n"
				"button:hover{background:#4338ca}\n"
				"#st{white-space:pre-wrap;font-family:monospace;margin-top:10px}\n"
				"a.home{color:#4f46e5}\n"
				"</style></head><body>\n"
				"<h1>LED Control</h1>\n"
				"<div class=\"card\">\n"
				"<p id=\"cur\">%s</p>\n"
				"<label>Wan:</label>\n"
				"<select id=\"wan\">\n"
				"<option value=\"1\">Off</option><option value=\"2\">On</option>\n"
				"<option value=\"3\">Slow Flash</option><option value=\"4\">Fast Flash</option>\n"
				"<option value=\"15\">Keep</option></select><br>\n"
				"<label>Lan:</label>\n"
				"<select id=\"lan\">\n"
				"<option value=\"1\">Off</option><option value=\"2\">On</option>\n"
				"<option value=\"3\">Slow Flash</option><option value=\"4\">Fast Flash</option>\n"
				"<option value=\"15\">Keep</option></select><br>\n"
				"<label>Wifi:</label>\n"
				"<select id=\"wifi\">\n"
				"<option value=\"1\">Off</option><option value=\"2\">On</option>\n"
				"<option value=\"3\">Slow Flash</option><option value=\"4\">Fast Flash</option>\n"
				"<option value=\"15\">Keep</option></select><br>\n"
				"<button onclick=\"setled()\">Set LEDs</button>\n"
				"<div id=\"st\"></div></div>\n"
				"<p><a class=\"home\" href=\"/\">Back</a></p>\n"
				"<script>\n"
				"function sel(id,v){document.getElementById(id).value=v}\n"
				"sel('wan',%d);sel('lan',%d);sel('wifi',%d);\n"
				"async function setled(){const r=await fetch('/led',{method:'POST',"
				"headers:{'Content-Type':'application/json'},"
				"body:JSON.stringify({wan:+wan.value,lan:+lan.value,wifi:+wifi.value})});"
				"const d=await r.json();"
				"document.getElementById('st').textContent=r.ok?JSON.stringify(d):d.error;"
				"document.getElementById('cur').textContent="
				"'Wan='+d.wan+' Lan='+d.lan+' Wifi='+d.wifi;"
				"sel('wan',d.wan);sel('lan',d.lan);sel('wifi',d.wifi)}\n"
				"</script></body></html>";

/* ---------- LED helper ---------- */

/* Last known state, init as 'unknown' (0 = not set yet) */
static enum rl_led_state s_last_wan = RL_KEEP;
static enum rl_led_state s_last_lan = RL_KEEP;
static enum rl_led_state s_last_wifi = RL_KEEP;

static enum rl_led_state parse_led_val(int v)
{
	if (v <= 0 || v > RL_KEEP)
		return RL_KEEP;
	return (enum rl_led_state)v;
}

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
		return "?";
	}
}

static int led_send_and_recv(enum rl_led_state wan, enum rl_led_state lan, enum rl_led_state wifi, char *result, size_t rlen)
{
	uint8_t frame[RL_FRAME_LEN];
	uint8_t reply[RL_FRAME_LEN];
	int ret;
	char tx[48], rx[48];

	rl_build(frame, wan, lan, wifi);

	snprintf(tx, sizeof(tx), "%02X %02X %02X %02X %02X %02X %02X %02X", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7]);

	if (s_led_fd < 0) {
		snprintf(result, rlen, "Serial port not open\nLast set: Wan=%s Lan=%s Wifi=%s\nTX: %s", led_name(s_last_wan), led_name(s_last_lan), led_name(s_last_wifi), tx);
		return -1;
	}

	ret = rl_serial_send(s_led_fd, frame);
	if (ret < 0) {
		snprintf(result, rlen, "TX: %s\nsend failed", tx);
		return -1;
	}

	ret = rl_serial_recv(s_led_fd, reply, 2000);
	if (ret == RL_FRAME_LEN) {
		snprintf(rx, sizeof(rx), "%02X %02X %02X %02X %02X %02X %02X %02X", reply[0], reply[1], reply[2], reply[3], reply[4], reply[5], reply[6], reply[7]);
		snprintf(result, rlen, "TX: %s\nRX: %s\nOK - Wan=%s Lan=%s Wifi=%s", tx, rx, led_name(wan), led_name(lan), led_name(wifi));
		s_last_wan = wan;
		s_last_lan = lan;
		s_last_wifi = wifi;
		return 0;
	}

	snprintf(result, rlen, "TX: %s\nno reply", tx);
	return -1;
}

/* ---------- Handlers ---------- */

static void handle_upload(struct mg_connection *c, struct mg_http_message *hm)
{
	size_t ofs = 0;
	struct mg_http_part part;
	int found = 0;

	while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
		if (part.filename.len == 0)
			continue;

		const char *end = part.filename.buf + part.filename.len;
		const char *p = end;
		while (p > part.filename.buf && p[-1] != '/' && p[-1] != '\\')
			p--;
		const char *fn = p;
		size_t fn_len = end - p;

		if (fn_len == 0 || fn_len > 255) {
			mg_http_reply(c, 400, "", "invalid filename\n");
			return;
		}

		if (hm->body.len > s_max_upload) {
			mg_http_reply(c, 413, "", "file too large\n");
			return;
		}

		char path[512];
		int n = snprintf(path, sizeof(path), "%s/", s_data_dir);
		if (n < 0 || (size_t)n + fn_len + 1 > sizeof(path)) {
			mg_http_reply(c, 500, "", "path too long\n");
			return;
		}
		memcpy(path + n, fn, fn_len);
		path[n + fn_len] = '\0';

		FILE *fp = fopen(path, "wb");
		if (!fp) {
			mg_http_reply(c, 500, "", "write failed\n");
			return;
		}
		size_t written = fwrite(part.body.buf, 1, part.body.len, fp);
		fclose(fp);

		if (written != part.body.len) {
			remove(path);
			mg_http_reply(c, 500, "", "write failed\n");
			return;
		}

		mg_http_reply(c, 200, "", "%.*s (%lu bytes)", (int)fn_len, fn, (unsigned long)written);
		found = 1;
		break;
	}

	if (!found)
		mg_http_reply(c, 400, "", "no file field\n");
}

static void list_files(struct mg_connection *c)
{
	DIR *dp = opendir(s_data_dir);
	if (!dp) {
		mg_http_reply(c, 200, "Content-Type: application/json\r\n", "[]\n");
		return;
	}

	char *buf = malloc(16384);
	if (!buf) {
		closedir(dp);
		mg_http_reply(c, 500, "", "OOM\n");
		return;
	}

	size_t pos = 0;
	int remaining = 16384;
	int need;

	need = snprintf(buf + pos, remaining, "[");
	if (need > 0) {
		pos += need;
		remaining -= need;
	}

	struct dirent *de;
	int first = 1;
	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		char fpath[512];
		snprintf(fpath, sizeof(fpath), "%s/%s", s_data_dir, de->d_name);
		if (stat(fpath, &st) != 0 || !S_ISREG(st.st_mode))
			continue;

		const char *name = de->d_name;
		int has_specials = 0;
		for (const char *p = name; *p; p++) {
			if (*p == '"' || *p == '\\' || *p < 0x20) {
				has_specials = 1;
				break;
			}
		}
		if (has_specials)
			continue;

		if (first)
			need = snprintf(buf + pos, remaining, "{\"n\":\"%s\",\"s\":%ld}", name, (long)st.st_size);
		else
			need = snprintf(buf + pos, remaining, ",{\"n\":\"%s\",\"s\":%ld}", name, (long)st.st_size);

		if (need > 0 && need < remaining) {
			pos += need;
			remaining -= need;
			first = 0;
		} else {
			break;
		}
	}

	snprintf(buf + pos, remaining, "]");
	closedir(dp);

	mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", buf);
	free(buf);
}

static void serve_file(struct mg_connection *c, struct mg_http_message *hm)
{
	struct mg_str remainder = hm->uri;
	if (remainder.len >= 3) {
		remainder.buf += 3;
		remainder.len -= 3;
	}

	char filename[256];
	mg_url_decode(remainder.buf, remainder.len, filename, sizeof(filename), 0);

	if (!mg_path_is_sane(mg_str(filename))) {
		mg_http_reply(c, 400, "", "bad path\n");
		return;
	}

	char path[512];
	snprintf(path, sizeof(path), "%s/%s", s_data_dir, filename);

	struct mg_http_serve_opts opts = { .root_dir = s_data_dir, .fs = NULL };
	mg_http_serve_file(c, hm, path, &opts);
}

static void handle_led(struct mg_connection *c, struct mg_http_message *hm)
{
	if (mg_match(hm->method, mg_str("POST"), NULL)) {
		/* POST /led — parse JSON body, send command, return JSON */
		int wan_val = RL_KEEP, lan_val = RL_KEEP, wifi_val = RL_KEEP;
		int ok = 0;

		/* Naive JSON int extractor: find "key": <num> in body */
		const char *b = hm->body.buf;
		size_t bl = hm->body.len;
		for (size_t i = 0; i < bl; i++) {
			int *v = NULL;
			if (strncmp(b + i, "\"wan\"", 5) == 0)
				v = &wan_val;
			else if (strncmp(b + i, "\"lan\"", 5) == 0)
				v = &lan_val;
			else if (strncmp(b + i, "\"wifi\"", 6) == 0)
				v = &wifi_val;
			if (!v)
				continue;
			const char *n = b + i + (v == &wifi_val ? 6 : 5);
			while (n < b + bl && (*n == ':' || *n == ' '))
				n++;
			if (n < b + bl && *n >= '0' && *n <= '9') {
				*v = (int)strtol(n, NULL, 10);
				ok = 1;
			}
		}

		if (!ok) {
			mg_http_reply(c, 400, "Content-Type: application/json\r\n", "{\"error\":\"invalid JSON\"}\n");
			return;
		}

		enum rl_led_state wan = parse_led_val(wan_val);
		enum rl_led_state lan = parse_led_val(lan_val);
		enum rl_led_state wifi = parse_led_val(wifi_val);

		char result[256] = "";
		led_send_and_recv(wan, lan, wifi, result, sizeof(result));

		mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"ok\",\"wan\":%d,\"lan\":%d,\"wifi\":%d}\n", (int)wan, (int)lan, (int)wifi);
		return;
	}

	/* GET /led — serve HTML page */
	char result[256];
	enum rl_led_state wan = s_last_wan;
	enum rl_led_state lan = s_last_lan;
	enum rl_led_state wifi = s_last_wifi;
	snprintf(result, sizeof(result), "Wan=%s Lan=%s Wifi=%s", led_name(wan), led_name(lan), led_name(wifi));

	char page[4096];
	snprintf(page, sizeof(page), s_led_html, result, (int)wan, (int)lan, (int)wifi);
	mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", page);
}

/* ---------- Main ---------- */

static void event_handler(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev != MG_EV_HTTP_MSG)
		return;

	struct mg_http_message *hm = (struct mg_http_message *)ev_data;

	if (mg_match(hm->uri, mg_str("/"), NULL)) {
		mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", s_page_html);
	} else if (mg_match(hm->uri, mg_str("/upload"), NULL)) {
		handle_upload(c, hm);
	} else if (mg_match(hm->uri, mg_str("/list"), NULL)) {
		list_files(c);
	} else if (mg_match(hm->uri, mg_str("/led"), NULL)) {
		handle_led(c, hm);
	} else if (mg_match(hm->uri, mg_str("/f/*"), NULL)) {
		serve_file(c, hm);
	} else {
		mg_http_reply(c, 404, "", "Not Found\n");
	}
}

int main(void)
{
	char *env;

	if ((env = getenv("ADDR")) != NULL)
		s_listen_on = env;
	if ((env = getenv("DATA_DIR")) != NULL)
		s_data_dir = env;
	if ((env = getenv("LED_DEV")) != NULL)
		s_led_dev = env;

	mkdir(s_data_dir, 0755);

	/* Init serial port for LED control (non-fatal) */
	s_led_fd = rl_serial_open(s_led_dev);
	if (s_led_fd < 0)
		fprintf(stderr, "WARNING: cannot open %s, LED control disabled\n", s_led_dev);
	else
		printf("LED serial port: %s\n", s_led_dev);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	mg_http_listen(&mgr, s_listen_on, event_handler, NULL);

	printf("listening on %s, data dir: %s\n", s_listen_on, s_data_dir);
	for (;;)
		mg_mgr_poll(&mgr, 1000);

	if (s_led_fd >= 0)
		close(s_led_fd);
	mg_mgr_free(&mgr);
	return 0;
}
