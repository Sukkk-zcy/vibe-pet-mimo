# VibePet ESP32 — WiFi 直推版

ESP32 + 2.8寸 TFT 屏幕，作为 HTTP 服务端接收 AI 编程助手的实时状态并显示。

**不再依赖 VibePet 桌面端桥接！插件直接推送状态到 ESP32。**

## 架构

```
MiMoCode CLI  ─→  mimocode-plugin  ─HTTP POST─→  ESP32 (HTTP Server)
opencode CLI  ─→  opencode-plugin  ─HTTP POST─→  ESP32 (HTTP Server)
                                                    │
                                                    ▼
                                               ST7789 屏幕
                                              大字状态 + 输出
```

## 硬件需求

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控板 | ESP32-WROOM-32 | 4MB Flash |
| 显示屏 | 2.8寸 ST7789 IPS | 320×240，SPI 接口 |
| 按键 | 3个 | 左/确认/右（GPIO 25/26/27） |

## 引脚配置

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

所有引脚在 `include/config.h` 中配置，TFT 驱动参数在 `platformio.ini` 的 `build_flags` 中。

## 快速开始

### 1. 编译烧录

```bash
cd VibePet-WiFi
pio run -t upload
```

### 2. 配置 WiFi

ESP32 启动后会自动进入配置模式，开启热点 `VibePet-Setup`（密码 `12345678`）。

1. 手机/电脑连接此热点
2. 浏览器访问 `192.168.4.1`
3. 填写你的 WiFi 名称和密码
4. 保存后 ESP32 重启连接

### 3. 记下 ESP32 IP

连接成功后，屏幕会显示 ESP32 的 IP 地址（如 `192.168.31.6`）5 秒钟。

**以后随时按确认键切换到详情页查看 IP。**

### 4. 配置插件

在电脑上修改插件中的 `ESP32_IP` 为上面记下的地址：

**MiMoCode 插件** (`src/hooks/mimocode-plugin/index.mjs`)：
```js
const ESP32_IP = "192.168.31.6";
```

**opencode 插件** (`src/hooks/opencode-plugin/index.mjs`)：
```js
const ESP32_IP = "192.168.31.6";
```

也可以通过环境变量覆盖：
```bash
ESP32_HOST=192.168.31.6  # 插件自动读取
```

### 5. 加载插件

通过 VibePet 桌面端加载对应插件即可。之后 AI 编程助手工作时的状态会实时推送到 ESP32。

## API 接口

ESP32 提供 HTTP API，插件直接 POST 状态数据：

```
POST http://<esp32-ip>/api/state
Content-Type: application/json

{
  "agent": "MiMoCode",
  "state": "thinking",
  "event": "UserPromptSubmit",
  "output": "分析用户请求中..."
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent` | string | Agent 名称（显示在顶栏） |
| `state` | string | 状态：idle / thinking / working / building / juggling / attention / notification / error / sweeping / sleeping |
| `event` | string | 可选，事件名 |
| `output` | string | 可选，当前输出文字（显示在底部） |

## 按键操作

| 操作 | 功能 |
|------|------|
| ← / → 短按 | 切换页面（主页 ↔ 详情页） |
| 确认 短按 | 回到主页 |
| ← + → 长按 2 秒 | 进入 WiFi 配置模式 |

## 重新配置 WiFi

同时长按左右键 2 秒进入配置模式，ESP32 会重启热点。

## 文件结构

```
VibePet-WiFi/
├── platformio.ini        # PlatformIO 配置 + TFT 引脚
├── include/
│   ├── config.h          # 引脚、常量定义
│   ├── network.h         # 网络函数声明
│   ├── state.h           # 状态数据结构
│   └── display.h         # 显示函数声明
├── src/
│   ├── main.cpp          # 主程序，FreeRTOS 任务
│   ├── network.cpp       # WiFi + HTTP 服务端 (POST /api/state)
│   ├── state.cpp         # JSON 解析、状态管理
│   └── display.cpp       # TFT 渲染
└── README.md
```
