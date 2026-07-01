#!/usr/bin/env node
// VibePet Bridge — 被 C# 托盘启动时自动切换到后台模式

import http from "http";
import fs from "fs";
import os from "os";
import path from "path";

const PORT = parseInt(process.env.BRIDGE_PORT || "17384", 10);
const ESP32_HOST = process.env.ESP32_HOST || "192.168.31.6";
const ESP32_PORT = process.env.ESP32_PORT || "80";
const ESP32_URL = `http://${ESP32_HOST}:${ESP32_PORT}/api/state`;

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

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  res.setHeader("access-control-allow-origin", "*");
  if (req.method === "OPTIONS") { res.writeHead(204); res.end(); return; }

  if ((url.pathname === "/api/hook" || url.pathname === "/state") && req.method === "POST") {
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

  if (url.pathname === "/api/device-snapshot" && req.method === "GET") {
    const pets = [];
    for (const [, v] of agents) pets.push({ agentId: v.agentId, agentName: v.agentName, state: v.state, title: v.event, output: v.output?.substring(0, 80), updatedAt: v.updatedAt });
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify({ pets, aggregate: pets[0] || { s: "idle", a: "", o: "" } }));
    return;
  }

  res.writeHead(404); res.end("not found");
});

server.listen(PORT, () => {
  const dir = path.join(os.homedir(), ".code-pet");
  const rtp = path.join(dir, "runtime.json");
  try {
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(rtp, JSON.stringify({ port: PORT, pid: process.pid }, null, 2));
  } catch {}
  console.log(`VibePet Bridge :${PORT} → ${ESP32_HOST}`);
});

// 优雅退出
process.on("SIGTERM", () => server.close(() => process.exit(0)));
process.on("SIGINT", () => server.close(() => process.exit(0)));
