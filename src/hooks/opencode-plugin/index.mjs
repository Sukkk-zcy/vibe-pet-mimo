import { readFileSync } from "fs";
import { homedir } from "os";
import { join } from "path";

// ═══════════════════════════════════════════════════════════
//  opencode → ESP32 / VibePet Bridge 状态推送插件
//
//  自动检测桥接：先试 127.0.0.1:17384（C# 桥接），
//  失败时直推 ESP32（配置在 ESP32_HOST）
// ═══════════════════════════════════════════════════════════

const ESP32_HOST = process.env.ESP32_HOST || "192.168.31.6";
const ESP32_PORT = process.env.ESP32_PORT || "80";
const BRIDGE_URL = "http://127.0.0.1:17384/api/hook";
const ESP32_URL = `http://${ESP32_HOST}:${ESP32_PORT}/api/state`;
const AGENT_NAME = "opencode";

// ─── 状态映射 ────────────────────────────────────────────
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

// ─── 发送状态 ────────────────────────────────────────────
function postState(body) {
  const payload = JSON.stringify(body);

  // 同时推送到桥接和 ESP32（桥接没开时直推也能到 ESP32）
  fetch(BRIDGE_URL, {
    method: "POST", headers: { "content-type": "application/json" }, body: payload,
  }).catch(() => {});

  fetch(ESP32_URL, {
    method: "POST", headers: { "content-type": "application/json" }, body: payload,
  }).catch(() => {});
}

// ─── 插件入口 ────────────────────────────────────────────
export default async function codePetOpencodePlugin(ctx = {}) {
  return {
    event: async ({ event }) => {
      const mapped = mapEvent(event);
      let output = extractOutput(event);
      if (!mapped) return;

      postState({
        agent: AGENT_NAME,
        state: mapped.state,
        event: mapped.event,
        output,
      });
    },

    "tool.execute.before": async ({ tool }) => {
      postState({
        agent: AGENT_NAME,
        state: "working",
        event: `tool:${tool}`,
        output: `Running: ${tool}`,
      });
    },

    "tool.execute.after": async ({ tool, args }) => {
      let output = `Done: ${tool}`;
      if (args?.command) output = args.command.substring(0, 120);
      else if (args?.file_path) output = args.file_path.substring(0, 120);
      postState({
        agent: AGENT_NAME,
        state: "thinking",
        event: `tool:${tool}:done`,
        output,
      });
    },

    "chat.message": async ({ model }) => {
      const modelName = model ? `${model.providerID}/${model.modelID}` : "";
      postState({
        agent: AGENT_NAME,
        state: "thinking",
        event: "chat.message",
        output: modelName ? `Model: ${modelName}` : "New message",
      });
    },
  };
}
