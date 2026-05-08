# 嵌入式 Web 服务器

基于 C + Mongoose 的嵌入式 Web 服务器，适用于 ARMv7 Linux（musl，软浮点）。  
支持文件上传下载和串口 LED 灯控。

## 快速开始

```bash
make -C c
scp c/webserver_arm root@192.168.11.1:/tmp/
/tmp/webserver_arm
```

浏览器打开 `http://192.168.11.1:8080/`

## 功能

| 接口 | 说明 |
|------|------|
| `/` | 文件上传页面 |
| `/upload` | POST 文件上传（上限 100MB） |
| `/f/*` | 文件下载 |
| `/led` | GET：控制页面，POST：JSON API |

## LED 控制

```bash
# 设置 Wan=On, Lan=慢闪, Wifi=快闪
curl -X POST http://192.168.11.1:8080/led \
  -H 'Content-Type: application/json' \
  -d '{"wan":2,"lan":3,"wifi":4}'
```

LED 值：`1`=灭, `2`=亮, `3`=慢闪, `4`=快闪, `15`=保持

## LED 串口协议

RS232, 115200, 8/N/1。帧格式：`A5 5A 04 Cmd Data[3] 校验和`

## 编译

```bash
make -C c webserver_arm       # Web 服务器（ARM）
make -C c led_auto_test_arm   # LED 交互测试
make -C c led_test_arm        # LED 简单测试
```

## 目录结构

```
├── c/            C + Mongoose 实现
│   ├── main.c          HTTP 服务器
│   ├── mongoose.h/c    Mongoose 库
│   ├── router_led.h/c  LED 灯控协议
│   ├── led_auto_test.c LED 交互测试
│   └── led_browser_test.py  浏览器自动化测试
├── go/           Go 版本（存档）
└── README.md
```
