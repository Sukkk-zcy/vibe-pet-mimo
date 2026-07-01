#!/usr/bin/env node

// ═══════════════════════════════════════════════════════════
//  ESP32 客户端 — 独立运行，不需要 VibePet 桌面端
//
//  用法：
//     export ESP32_HOST=192.168.31.6
//     node esp32-client.mjs
//
//  侦听 ~/.code-pet/runtime.json 中的桥接状态变化，
//  并实时推送到 ESP32。
// ═══════════════════════════════════════════════════════════

const ESP32_HOST = process.env.ESP32_HOST || "192.168.31.6";
const ESP32_PORT = process.env.ESP32_PORT || "80";
const API_URL = `http://${ESP32_HOST}:${ESP32_PORT}/api/state`;

const RUNTIME_PATH = require("path").join(require("os").homedir(), ".code-pet", "runtime.json");
const http = require("http");
const fs = require("fs");

let lastState = "";

function postToESP32(data) {
  const body = JSON.stringify(data);
  const req = http.request(API_URL, {
    method: "POST",
    headers: { "content-type": "application/json" },
  }, res => {
    if (res.statusCode === 200) {
      console.log(`[ESP32] ${data.state} ${data.output?.substring(0, 40) || ""}`);
    }
  });
  req.on("error", e => console.warn(`[ESP32] ${e.message}`));
  req.write(body);
  req.end();
}

// 轮询 runtime.json，检测桥接状态变化
function pollRuntime() {
  try {
    if (!fs.existsSync(RUNTIME_PATH)) return;
    const data = JSON.parse(fs.readFileSync(RUNTIME_PATH, "utf8"));
    const current = data?.state || "";
    if (current && current !== lastState) {
      lastState = current;
      postToESP32({
        agent: "MiMoCode",
        state: current,
        event: data?.event || "",
        output: data?.output || "",
      });
    }
  } catch {}
}

console.log(`ESP32 client: pushing to ${API_URL}`);
console.log(`Watching ${RUNTIME_PATH}...`);
pollRuntime();
setInterval(pollRuntime, 1000);
