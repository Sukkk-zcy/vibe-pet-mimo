#!/usr/bin/env node
// VibePet Bridge — 系统托盘版
// 双击运行，右键退出

import http from "http";
import fs from "fs";
import os from "os";
import path from "path";
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const Systray = require("systray2").default;

const PORT = parseInt(process.env.BRIDGE_PORT || "17384", 10);
const ESP32_HOST = process.env.ESP32_HOST || "192.168.31.6";
const ESP32_PORT = process.env.ESP32_PORT || "80";
const ESP32_URL = `http://${ESP32_HOST}:${ESP32_PORT}/api/state`;

// ─── 状态管理 ────────────────────────────────────────────
const agents = new Map();
let lastPush = "";

function pushToESP32(data) {
  const body = JSON.stringify({
    agent: data.agentName,
    state: data.state,
    event: data.event || "",
    output: (data.output || "").substring(0, 200),
  });
  const key = `${data.state}|${data.agentName}|${data.output?.substring(0, 30)}`;
  if (key === lastPush) return;
  lastPush = key;

  const req = http.request(ESP32_URL, {
    method: "POST",
    headers: { "content-type": "application/json", "connection": "close" },
  }, res => { res.resume(); });
  req.on("error", () => {});
  req.write(body);
  req.end();
}

function updateState(agentId, agentName, state, event, output) {
  const key = agentId || agentName || "unknown";
  agents.set(key, { agentId, agentName: agentName || agentId || "unknown", state: state || "idle", event: event || "", output: output || "", updatedAt: Date.now() });

  let agg = { state: "idle", agentName: "", output: "" };
  for (const [, v] of agents) {
    if (v.state !== "idle" && v.state !== "sleeping") { agg = v; break; }
  }
  if (agg.state === "idle" && agents.size > 0) agg = agents.values().next().value;
  pushToESP32(agg);
}

// ─── HTTP 服务 ───────────────────────────────────────────
const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const method = req.method;
  res.setHeader("access-control-allow-origin", "*");
  if (method === "OPTIONS") { res.writeHead(204); res.end(); return; }

  if ((url.pathname === "/api/hook" || url.pathname === "/state") && method === "POST") {
    let body = "";
    req.on("data", c => body += c);
    req.on("end", () => {
      try {
        const d = JSON.parse(body);
        updateState(d.agentId || d.agent, d.agentName || d.agent, d.state, d.event, d.output || d.text);
        res.writeHead(200, { "content-type": "application/json" });
        res.end(JSON.stringify({ ok: true }));
      } catch { res.writeHead(400); res.end("bad json"); }
    });
    return;
  }

  if (url.pathname === "/api/device-snapshot" && method === "GET") {
    const pets = [];
    for (const [, v] of agents) pets.push({ agentId: v.agentId, agentName: v.agentName, state: v.state, title: v.event, output: v.output?.substring(0, 80), updatedAt: v.updatedAt });
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify({ pets, aggregate: pets[0] || { s: "idle", a: "", o: "" } }));
    return;
  }

  res.writeHead(404); res.end("not found");
});

server.listen(PORT, () => {
  // 写入 runtime.json 兼容已有 hook
  const dir = path.join(os.homedir(), ".code-pet");
  const rtp = path.join(dir, "runtime.json");
  try {
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(rtp, JSON.stringify({ port: PORT, pid: process.pid }, null, 2));
  } catch {}
});

// ─── 系统托盘 ───────────────────────────────────────────
const systray = new Systray({
  menu: {
    icon: "", // 使用默认图标
    title: "VibePet Bridge",
    tooltip: `VibePet Bridge :${PORT} → ${ESP32_HOST}`,
    items: [
      {
        title: "VibePet Bridge 运行中",
        tooltip: `端口 ${PORT} → ESP32 ${ESP32_HOST}`,
        checked: false,
        enabled: false,
      },
      {
        title: "-",  // 分隔线
        checked: false,
        enabled: false,
      },
      {
        title: "退出",
        tooltip: "关闭桥接服务",
        checked: false,
        enabled: true,
      },
    ],
  },
  debug: false,
  copyDir: true,
});

systray.onClick(action => {
  if (action.item.title === "退出") {
    console.log("正在退出...");
    try { fs.unlinkSync(path.join(os.homedir(), ".code-pet", "runtime.json")); } catch {}
    server.close();
    systray.kill(false);
    process.exit(0);
  }
  if (action.item.index === 0) {
    // 更新状态显示
    const active = [];
    for (const [, v] of agents) {
      if (v.state !== "idle") active.push(`${v.agentName}:${v.state}`);
    }
    const status = active.length > 0 ? active.join(", ") : "空闲";
    systray.sendAction({
      type: "update-menu",
      item: { ...action.item, title: `VibePet Bridge — ${status}`, tooltip: `端口 ${PORT} → ESP32 ${ESP32_HOST}` },
      seq_id: action.seq_id,
    });
  }
});

// 定时更新托盘提示
setInterval(() => {
  const active = [];
  for (const [, v] of agents) {
    if (v.state !== "idle") active.push(`${v.agentName}:${v.state}`);
  }
  const status = active.length > 0 ? active.join(", ") : "空闲";
  systray.sendAction({
    type: "update-menu",
    item: { title: `VibePet Bridge — ${status}`, tooltip: `端口 ${PORT} → ESP32 ${ESP32_HOST}`, checked: false, enabled: false },
    seq_id: 0,
  });
}, 3000);

console.log(`VibePet Bridge :${PORT} → ${ESP32_HOST}`);
