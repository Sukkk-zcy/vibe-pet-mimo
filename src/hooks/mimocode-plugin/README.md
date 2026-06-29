# MiMoCode Plugin

将 [MiMoCode CLI](https://github.com/Sukkk-zcy/mimocode) 的工作状态实时同步到 VibePet。

## 安装

1. 将 `mimocode-plugin` 目录放到 VibePet 的 `src/hooks/` 下
2. 在 VibePet 桌面端中启用 MiMoCode 插件
3. 确保 VibePet 桌面端正在运行，桥接服务端口已打开（默认 `17384`）

## 工作原理

插件通过监听 MiMoCode CLI 的事件（会话创建、消息更新、工具调用等），将状态通过 HTTP POST 发送到 VibePet 桌面端的桥接服务：

```
MiMoCode CLI → 插件事件监听 → HTTP POST → VibePet 桌面端桥接 → BLE/WiFi → ESP32 显示屏
```

## 配置

不需要额外配置。插件自动：
- 从 `~/.code-pet/runtime.json` 读取桥接端口
- 兜底扫描端口 `17384`–`17388`
- 自动识别缓存有效端口

## 状态映射

| MiMoCode 事件 | VibePet 状态 | 说明 |
|---------------|-------------|------|
| `session.created` | idle | 新会话创建 |
| `session.deleted` | sleeping | 会话结束 |
| `message.part.updated` (running) | thinking | AI 正在思考 |
| `message.part.updated` (tool) | working | AI 正在使用工具 |
| `message.part.updated` (error) | error | 出错 |
| `session.idle` | attention | AI 响应完成 |
| `permission.asked` | notification | 等待用户确认 |
| `tool.execute.before` | working | 正在执行工具 |
| `tool.execute.after` | thinking | 工具执行完毕 |
| `chat.message` | thinking | 新消息处理中 |
