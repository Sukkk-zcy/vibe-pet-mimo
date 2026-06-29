# ESP32 + 2.8" TFT — Wi-Fi 版本

通过 WiFi 轮询 VibePet 桥接服务，在 ESP32 + ST7789 屏幕上实时显示 AI 编程助手的状态与动画。

## 硬件需求

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控板 | ESP32-WROOM-32 | 4MB Flash |
| 显示屏 | 2.8寸 ST7789 IPS | 320×240，SPI 接口 |
| 按键 | 3个 | 左/确认/右（GPIO 25/26/27） |
| 蜂鸣器 | 可选 | GPIO 13 |
| 电池 | 可选 | ADC 检测 GPIO 34 |

## 引脚配置

所有引脚定义在 `include/config.h` 中，**请根据你的硬件修改**：

| 功能 | GPIO | 说明 |
|------|------|------|
| TFT MOSI | 23 | SPI 数据线 |
| TFT SCLK | 18 | SPI 时钟 |
| TFT CS | 5 | 片选 |
| TFT DC | 2 | 数据/命令 |
| TFT RST | 4 | 复位 |
| TFT 背光 | 17 | 可调亮度 |
| BTN_LEFT | 25 | 左键（上拉） |
| BTN_OK | 26 | 确认键（上拉） |
| BTN_RIGHT | 27 | 右键（上拉） |
| BUZZER | 13 | 蜂鸣器 |
| BATTERY | 34 | ADC 检测 |

TFT 驱动参数同样在 `config.h` 和 `platformio.ini` 的 `build_flags` 中配置。

## 软件依赖

- [PlatformIO](https://platformio.org/)
- 库（`platformio.ini` 自动安装）：
  - `bodmer/TFT_eSPI@^2.5.43`
  - `bblanchon/ArduinoJson@^7.0.4`

## 编译与烧录

```bash
# 编译
pio run

# 烧录
pio run -t upload

# 查看串口日志
pio device monitor -b 115200
```

## 首次使用：WiFi 配置

ESP32 启动后会自动检测是否已保存 WiFi 配置：

1. **未配置** → 自动进入配置模式，开启热点 `VibePet-Setup`（密码 `12345678`）
2. 手机/电脑连接此热点
3. 浏览器访问 `192.168.4.1`
4. 填写：
   - **WiFi SSID / 密码** — 你的 2.4G WiFi
   - **Bridge Host** — VibePet 桌面端所在电脑的 IP（查看桌面端设置或 `~/.code-pet/runtime.json` 中的 `port`）
   - **Bridge Port** — 默认 `17384`
5. 保存后 ESP32 自动重启连接

**已有配置但需要修改**：同时长按左右键 2 秒进入配置模式。

## 配置项说明

| 配置 | 默认值 | 位置 | 说明 |
|------|--------|------|------|
| `POLL_INTERVAL_MS` | 500 | `config.h` | 轮询间隔（毫秒） |
| `DEFAULT_BRIDGE_HOST` | 192.168.1.2 | `config.h` | 默认桥接 IP |
| `DEFAULT_BRIDGE_PORT` | 17384 | `config.h` | 默认桥接端口 |
| `WIFI_AP_SSID` | VibePet-Setup | `config.h` | 配置模式热点名 |
| `WIFI_AP_PASS` | 12345678 | `config.h` | 配置模式密码 |
| TFT 引脚 | 见上表 | `platformio.ini` | `build_flags` 中定义 |

## 按键操作

| 操作 | 功能 |
|------|------|
| ← / → 短按 | 切换页面（主页 ↔ 详情页） |
| 确认 短按 | 回到主页 |
| ← + → 长按 2 秒 | 进入 WiFi 配置模式 |

## 桥接地址获取

1. 打开 VibePet 桌面端
2. 查看 `~/.code-pet/runtime.json` 中的 `port` 字段
3. 在配置页面填写该端口对应的桥接地址

## 文件结构

```
src/firmware/esp32-tft-wifi/
├── platformio.ini       # 平台配置 + TFT 引脚
├── include/
│   ├── config.h         # 引脚、常量、默认值
│   ├── state.h          # 状态数据结构
│   ├── network.h        # 网络函数声明
│   └── display.h        # 显示函数声明
├── src/
│   ├── main.cpp         # 主程序, FreeRTOS 任务
│   ├── state.cpp        # JSON 解析, 状态管理
│   ├── network.cpp      # WiFi, WebServer, DNS, HTTP 轮询
│   └── display.cpp      # TFT 渲染 (猫角色动画)
├── lib/
│   └── README
└── test/
    └── README
```
