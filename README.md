# 嵌入式 Web 服务器

基于 C + Mongoose 的嵌入式 Web 服务器，适用于 ARMv7 Linux（musl，软浮点）。
支持文件上传下载和串口 LED 灯控。

## 快速开始

```bash
# 克隆仓库后进入项目根目录
cd webserver

# 编译
cd c && make webserver_arm

# 传到开发板
scp webserver_arm root@192.168.11.1:/tmp/

# 板上运行
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

### 参数

- 接口：RS232
- 波特率：115200
- 数据位：8
- 校验：无（N）
- 停止位：1

### 帧格式（HEX）

```
A5 5A 04 Cmd Data[3] Checksum
```

| 字节 | 长度 | 说明 |
|------|------|------|
| `A5` | 1 | 前导识别 1 |
| `5A` | 1 | 前导识别 2 |
| `04` | 1 | 帧长度（固定 0x04） |
| `Cmd` | 1 | 命令字：`0x01`=置灯，`0x81`=回复 |
| `Data` | 3 | 24bit LED 状态数据（大端） |
| `Checksum` | 1 | 累加和低 8 位：`(A5+5A+04+Cmd+D0+D1+D2) & 0xFF` |

### Data 位定义

每 4bit 表示一个灯的状态：

| 位域 | 灯 |
|------|----|
| b0~3 | Wan_led |
| b4~7 | Lan_led |
| b8~11 | Wifi_led |
| b12~23 | 保留，置 0 |

每个灯的状态值：

| 值 | 状态 |
|----|------|
| 1 | 灭（Off） |
| 2 | 亮（On） |
| 3 | 慢闪（Slow Flash） |
| 4 | 快闪（Fast Flash） |
| 0, 5~E | 无影响 |
| F | 保持当前状态 |

### 示例

**例 1：** Wan=灭(1)，Lan=亮(2)，Wifi=慢闪(3)

```
Data = 0x000321  →  bytes: 00 03 21
Checksum = A5+5A+04+01+00+03+21 = 0x28
TX: A5 5A 04 01 00 03 21 28
RX: A5 5A 04 81 00 03 21 A8    ← 回复
```

**例 2：** Wan=快闪(4)，其他保持(F)

```
Data = 0x000FF4  →  bytes: 00 0F F4
Checksum = A5+5A+04+01+00+0F+F4 = 0x07
TX: A5 5A 04 01 00 0F F4 07
RX: A5 5A 04 81 00 0F F4 87    ← 回复
```

> 回复帧的命令字为 `0x81`，其余字段与发送帧相同。

### 备注

- 上电后三个灯全灭，需发送命令点亮
- 指令最小间隔 1 秒
- 回复仅在收到的帧格式正确时才有

## 编译

```bash
# 进入 c/ 目录后编译
cd c
make webserver_arm       # Web 服务器（ARM）
make led_auto_test_arm   # LED 交互测试
make led_test_arm        # LED 简单测试
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
