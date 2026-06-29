# ESP32 + 2.8" TFT — BLE 版本

通过 BLE（蓝牙低功耗）直连 VibePet 桌面端，在 ESP32 + ST7789 屏幕上实时显示 AI 编程助手的状态与动画。

**无需 WiFi** — 桌面端通过蓝牙直接推送给 ESP32。

## 硬件需求

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控板 | ESP32-WROOM-32 | 需支持 BLE |
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

## 首次使用：BLE 配对

1. ESP32 烧录后自动启动，屏幕显示 "Connecting..." 并开始 BLE 广播
2. 打开 VibePet 桌面端，选择连接设备
3. 在蓝牙列表中找到 **`VibePet-ESP-Display`** 并配对
4. 配对成功后，桌面端自动推送状态数据，屏幕实时更新

### BLE 服务说明

| 项目 | 值 |
|------|-----|
| 设备名 | `VibePet-ESP-Display` |
| Service UUID | `7b71f91a-3c7b-4c3b-9f2d-2dbdccd5c001` |
| State Char UUID | `7b71f91a-3c7b-4c3b-9f2d-2dbdccd5c002`（Write / Write NR） |

桌面端通过 BLE 写入 JSON 状态数据，ESP32 接收后解析并显示对应动画。

## 配置项说明

| 配置 | 默认值 | 位置 | 说明 |
|------|--------|------|------|
| `BLE_DEVICE_NAME` | VibePet-ESP-Display | `config.h` | BLE 广播设备名 |
| `BLE_SERVICE_UUID` | (见上表) | `config.h` | BLE 服务 UUID |
| `BLE_STATE_CHAR_UUID` | (见上表) | `config.h` | BLE 状态特征 UUID |
| TFT 引脚 | 见上表 | `platformio.ini` | `build_flags` 中定义 |

> ⚠️ 如需修改 BLE UUID，请确保桌面端配置与之匹配。

## 按键操作

| 操作 | 功能 |
|------|------|
| ← / → 短按 | 切换页面（主页 ↔ 详情页） |
| 确认 短按 | 回到主页 |

## 串口调试

连接串口后输入命令快速测试：

```
test working     # 测试 "工作中" 状态
test thinking    # 测试 "思考中" 状态
test error       # 测试 "错误" 状态
test sleeping    # 测试 "睡眠" 状态
test idle        # 测试 "空闲" 状态
help             # 查看所有命令
```

## 文件结构

```
src/firmware/esp32-tft-ble/
├── platformio.ini       # 平台配置 + TFT 引脚
├── include/
│   ├── config.h         # 引脚、常量、BLE UUID
│   ├── ble.h            # BLE 函数声明
│   ├── state.h          # 状态数据结构
│   └── display.h        # 显示函数声明
├── src/
│   ├── main.cpp         # 主程序, FreeRTOS 任务
│   ├── state.cpp        # JSON 解析, 状态管理
│   ├── ble.cpp          # BLE 服务端初始化与回调
│   └── display.cpp      # TFT 渲染 (机甲风角色动画)
├── lib/
│   └── README
└── test/
    └── README
```
