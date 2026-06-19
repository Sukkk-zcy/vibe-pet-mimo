"use strict";

const { execFile } = require("child_process");

const AGENTS = [
  {
    id: "codex",
    name: "Codex",
    match: (line) => line.includes("/Codex.app/") || /\bcodex(?:\.exe)?\b/i.test(line),
  },
  {
    id: "cursor",
    name: "Cursor",
    match: (line) => line.includes("/Cursor.app/") || /\bcursor(?:\.exe)?\b/i.test(line),
  },
  {
    id: "windsurf",
    name: "Windsurf",
    match: (line) => line.includes("/Windsurf.app/") || /\bwindsurf(?:\.exe)?\b/i.test(line),
  },
];

function processListCommand(platform = process.platform) {
  if (platform === "win32") return { file: "tasklist", args: ["/fo", "csv", "/nh"] };
  return { file: "ps", args: ["axo", "args"] };
}

class PresenceMonitor {
  constructor(onState, options = {}) {
    this.onState = onState;
    this.hasSession = typeof options.hasSession === "function" ? options.hasSession : () => false;
    this.intervalMs = options.intervalMs || 5000;
    this.verbose = !!options.verbose;
    this.interval = null;
    this.inFlight = false;
  }

  start() {
    if (this.interval) return;
    this.poll();
    this.interval = setInterval(() => this.poll(), this.intervalMs);
  }

  stop() {
    if (this.interval) clearInterval(this.interval);
    this.interval = null;
  }

  poll() {
    if (this.inFlight) return;
    this.inFlight = true;
    const command = processListCommand();
    execFile(command.file, command.args, { maxBuffer: 1024 * 1024 }, (err, stdout) => {
      this.inFlight = false;
      if (err) {
        if (this.verbose) console.warn(`[presence] process list failed: ${err.message}`);
        return;
      }
      const lines = String(stdout || "").split("\n");
      for (const agent of AGENTS) {
        if (this.hasSession(agent.id)) continue;
        if (!lines.some((line) => agent.match(line))) continue;
        this.onState({
          agentId: agent.id,
          agentName: agent.name,
          sessionId: "app",
          state: "idle",
          event: "AppRunning",
          title: "Waiting for hook events",
          source: "process-presence",
        });
        if (this.verbose) console.log(`[presence] ${agent.name} app detected`);
      }
    });
  }
}

module.exports = {
  processListCommand,
  PresenceMonitor,
};
