# Web 管理界面

基于 C + Mongoose 的嵌入式 Web 管理后台，适用于 HSAN19 (ARMv7 Linux, musl 软浮点)。

## 架构

```
浏览器 ──HTTP──> admin_server_arm (port 8080)
                      │
           ┌──────────┴──────────┐
           │                     │
    /web/* 静态文件         /api/* REST 接口
           │                     │
     ┌─────┴─────┐         ┌────┴────┐
     │            │         │         │
  /web/*.html  /web/css/  popen(    fopen(
  /web/js/    /web/js/     shell)   /proc/*)
```

前后端分离：
- **前端**：纯 HTML/CSS/JS，部署在板子 `/web/` 目录，通过 `fetch()` 调后端 API
- **后端**：C 单进程，同时提供静态文件服务和 REST API

## API 参考

### `GET /api/status` — 系统状态

```json
{"uptime":336419,"mem_total":230780,"mem_free":43524,"hostname":"hsan","version":"Linux version 5.10.0"}
```

### `GET /api/wifi` — WiFi 配置

```json
{"wifi24":{"ssid":"SXBCTV-bb0040-2.4G","channel":1},"wifi5":{"ssid":"SXBCTV-bb0040-5G","channel":149}}
```

### `POST /api/wifi` — 修改 WiFi（暂未实现）

```json
Request: {"ssid_24":"new-ssid","password_24":"xxx","ssid_5":"...","password_5":"..."}
Response: {"status":"ok"}
```

### `GET /api/network` — 网络配置

```json
{"lan_ip":"192.168.11.1","netmask":"255.255.255.0","dhcp_start":"192.168.11.2,192.168.11.254"}
```

### `POST /api/network` — 修改网络（暂未实现）

### `GET /post /api/led` — LED 控制

```json
Request: {"wan":2,"lan":1,"wifi":3}
Response: {"wan":2,"lan":1,"wifi":3}
```

LED 值：`1`=灭, `2`=亮, `3`=慢闪, `4`=快闪

### `GET /api/devices` — 在线设备

```json
[{"ip":"192.168.11.10","mac":"48:f3:17:0e:83:c4"},...]
```

### `POST /api/system/reboot` — 重启

```json
{"status":"rebooting"}
```

### `POST /api/system/reset` — 恢复出厂

```json
{"status":"resetting"}
```

## 前端文件结构

```
/web/
├── index.html          总框架（左侧导航 + 内容区 SPA）
├── dashboard.html      仪表盘（系统信息、在线设备）
├── wifi.html           WiFi 设置（SSID、信道）
├── network.html        网络设置（LAN IP、DHCP）
├── led.html            LED 控制（灯泡图标）
├── system.html         系统管理（重启、恢复出厂、固件升级）
├── css/
│   └── style.css       深色仪表盘样式
└── js/
    └── api.js          API 封装（get/post/toast）
```

### SPA 机制

`index.html` 通过 `fetch()` 加载各页面 HTML 片段，插入到 `<div id="content">`。
由于 `innerHTML` 不执行 `<script>` 标签，`load()` 函数在插入后手动重建 script 节点。

## 编译

```bash
cd server
make admin_server_arm
```

会在 `server/` 下生成 `admin_server_arm` 二进制，引用 `../c/mongoose.c` 和 `../c/router_led.c`。

## 部署

```bash
# 1. 编译
make admin_server_arm

# 2. 复制到 tftp 目录
cp admin_server_arm /tftpboot/

# 3. 板端下载
tftp -g -r admin_server_arm <host_ip>

# 4. 前端文件传到 /web/
#    将 web/ 目录下所有文件通过 tftp 或 scp 传到板子的 /web/

# 5. 启动
/tmp/admin_server_arm &

# 浏览器打开 http://192.168.11.1:8080/
```

## LED 串口协议

参见 `c/router_led.h` 和项目根目录 `README.md`。
