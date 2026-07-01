#!/usr/bin/env node

// ═══════════════════════════════════════════════════════════
//  VibePet 轻量桥接 — 替换 VibePet 桌面端
//
//  作用：
//    1. 监听端口 17384，接受 agent hook/plugin 上报状态
//    2. 实时推送到 ESP32
//    3. 提供 /api/device-snapshot 给 ESP32 轮询（兼容）
//
//  用法：
//    export ESP32_HOST=192.168.31.6   # 你的 ESP32 IP
//    node bridge-server.mjs
//
//  开机自启（Windows）：
//    powershell 里运行：
//    $Action = New-ScheduledTaskAction -Execute "node" -Argument "X:\path\to\bridge-server.mjs"
//    Register-ScheduledTask -TaskName "VibePetBridge" -Action $Action -Trigger (New-ScheduledTaskTrigger -AtStartup)
// ═══════════════════════════════════════════════════════════

import http from "http";
import fs from "fs";
import os from "os";
import path from "path";

const PORT = parseInt(process.env.BRIDGE_PORT || "17384", 10);
const ESP32_HOST = process.env.ESP32_HOST || "192.168.31.6";
const ESP32_PORT = process.env.ESP32_PORT || "80";
const ESP32_URL = `http://${ESP32_HOST}:${ESP32_PORT}/api/state`;

// ─── 状态管理 ────────────────────────────────────────────

// 所有 agent 的最新状态
const agents = new Map();

function updateAgentState(agentId, agentName, state, event, output) {
  const now = Date.now();
  const key = agentId || agentName || "unknown";

  agents.set(key, {
    agentId,
    agentName: agentName || agentId || "unknown",
    state: state || "idle",
    event: event || "",
    output: output || "",
    updatedAt: now,
  });

  // 计算聚合状态：取第一个非 idle 的 agent
  let agg = { state: "idle", agentName: "", output: "" };
  for (const [, v] of agents) {
    if (v.state !== "idle" && v.state !== "sleeping") {
      agg = v; break;
    }
  }
  if (agg.state === "idle" && agents.size > 0) {
    // 全都 idle，用第一个
    agg = agents.values().next().value;
  }

  // 推送到 ESP32
  pushToESP32(agg);
}

// ─── 推送 ESP32 ──────────────────────────────────────────

let lastPush = "";
function pushToESP32(data) {
  const body = JSON.stringify({
    agent: data.agentName,
    state: data.state,
    event: data.event || "",
    output: (data.output || "").substring(0, 200),
  });

  // 避免重复推送相同状态
  const key = `${data.state}|${data.agentName}|${data.output?.substring(0, 30)}`;
  if (key === lastPush) return;
  lastPush = key;

  // 异步推送，不阻塞
  const req = http.request(ESP32_URL, {
    method: "POST",
    headers: { "content-type": "application/json", "connection": "close" },
  }, res => {
    if (res.statusCode === 200) {
      console.log(`→ ESP32: ${data.state} ${data.agentName}`);
    } else {
      console.warn(`× ESP32: HTTP ${res.statusCode}`);
    }
    res.resume();
  });
  req.on("error", e => {
    // 连接失败静默处理，ESP32 可能正在重启
  });
  req.write(body);
  req.end();
}

// ─── HTTP 服务 ───────────────────────────────────────────

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const method = req.method;

  // CORS
  res.setHeader("access-control-allow-origin", "*");
  res.setHeader("access-control-allow-methods", "GET, POST, OPTIONS");
  if (method === "OPTIONS") {
    res.writeHead(204); res.end(); return;
  }

  // ── hook/plugin 上报入口 ──
  if ((url.pathname === "/api/hook" || url.pathname === "/state") && method === "POST") {
    let body = "";
    req.on("data", c => body += c);
    req.on("end", () => {
      try {
        const data = JSON.parse(body);
        const agentId = data.agentId || data.agent || "unknown";
        const agentName = data.agentName || data.agent || agentId;
        const state = data.state || "idle";
        const event = data.event || "";
        const output = data.output || data.text || "";
        updateAgentState(agentId, agentName, state, event, output);
        res.writeHead(200, { "content-type": "application/json" });
        res.end(JSON.stringify({ ok: true }));
      } catch (e) {
        res.writeHead(400); res.end("bad json");
      }
    });
    return;
  }

  // ── 状态快照（供 ESP32 轮询） ──
  if (url.pathname === "/api/device-snapshot" && method === "GET") {
    const pets = [];
    for (const [, v] of agents) {
      pets.push({
        agentId: v.agentId,
        agentName: v.agentName,
        state: v.state,
        title: v.event,
        output: v.output?.substring(0, 80),
        updatedAt: v.updatedAt,
      });
    }
    const snapshot = {
      pets,
      aggregate: pets.length > 0 ? {
        s: pets[0].state, a: pets[0].agentName, o: pets[0].output,
      } : { s: "idle", a: "", o: "" },
    };
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify(snapshot));
    return;
  }

  // ── 健康检查 ──
  if (url.pathname === "/health") {
    const active = [];
    for (const [, v] of agents) {
      if (v.state !== "idle") active.push(v.agentName);
    }
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify({
      status: "ok",
      agents: agents.size,
      active: active,
      esp32: ESP32_HOST,
      uptime: Math.floor(process.uptime()),
    }));
    return;
  }

  res.writeHead(404); res.end("not found");
});

server.listen(PORT, () => {
  console.log(`┌──────────────────────────────────────────┐`);
  console.log(`│  VibePet Bridge                          │`);
  console.log(`│  监听 :${PORT}                              │`);
  console.log(`│  推送 → ${ESP32_HOST}:${ESP32_PORT}               │`);
  console.log(`│  健康 http://localhost:${PORT}/health       │`);
  console.log(`└──────────────────────────────────────────┘`);
  console.log(`Agent hooks 可 POST → http://localhost:${PORT}/api/hook`);

  // 写入 runtime.json (与 VibePet 兼容)
  const dir = path.join(os.homedir(), ".code-pet");
  const rtp = path.join(dir, "runtime.json");
  try {
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(rtp, JSON.stringify({ port: PORT, pid: process.pid }, null, 2));
  } catch {}
});
