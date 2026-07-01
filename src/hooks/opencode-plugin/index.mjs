import { readFileSync } from "fs";
import { homedir } from "os";
import { join } from "path";

// ═══════════════════════════════════════════════════════════
//  opencode → ESP32 VibePet Display 直推插件
//
//  用法：
//    1. 烧录 ESP32 WiFi 版固件，记下屏幕显示的 IP
//    2. 将下方 ESP32_IP 改为 ESP32 的 IP 地址
//    3. 用 VibePet 桌面端加载此插件
//
//  状态映射：
//    session.created        → idle
//    session.deleted        → sleeping
//    message.part.updated
//      status=running       → thinking
//      status=tool          → working
//      status=error         → error
//    session.idle           → attention
//    permission.asked       → notification
// ═══════════════════════════════════════════════════════════

const ESP32_IP = process.env.ESP32_HOST || "192.168.31.6";  // ← 改成你的 ESP32 IP
const ESP32_PORT = process.env.ESP32_PORT || "80";
const API_URL = `http://${ESP32_IP}:${ESP32_PORT}/api/state`;

const AGENT_NAME = "opencode";

function mapEvent(event) {
  if (!event || typeof event.type !== "string") return null;
  const status = event.properties?.status?.type;

  if (event.type === "session.created") return { state: "idle", event: "SessionStart" };
  if (event.type === "session.deleted" || event.type === "server.instance.disposed") return { state: "sleeping", event: "SessionEnd" };

  if (event.type === "message.part.updated") {
    if (status === "running")  return { state: "thinking", event: "UserPromptSubmit" };
    if (status === "tool")     return { state: "working",  event: "PreToolUse" };
    if (status === "error")    return { state: "error",    event: "StopFailure" };
  }

  if (event.type === "session.idle")   return { state: "attention",    event: "Stop" };
  if (event.type === "permission.asked") return { state: "notification", event: "PermissionRequest" };

  return null;
}

function extractOutput(event) {
  const p = event.properties;
  if (!p) return "";
  return (p.text || p.output || p.message || "").substring(0, 120);
}

function postState(body) {
  const payload = JSON.stringify(body);
  fetch(API_URL, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: payload,
  }).then(r => {
    if (r.ok) console.log(`[opencode→ESP32] ${body.state}`);
    else console.warn(`[opencode→ESP32] HTTP ${r.status}`);
  }).catch(e => console.warn(`[opencode→ESP32] ${e.message}`));
}

export default async function codePetOpencodePlugin(ctx = {}) {
  return {
    event: async ({ event }) => {
      const mapped = mapEvent(event);
      if (!mapped) return;
      postState({
        agent: AGENT_NAME,
        state: mapped.state,
        event: mapped.event,
        output: extractOutput(event),
      });
    },
  };
}
