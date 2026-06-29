import { readFileSync } from "fs";
import { homedir } from "os";
import { join } from "path";

const AGENT_ID = "mimocode";
const AGENT_NAME = "MiMoCode";
const RUNTIME_PATH = join(homedir(), ".code-pet", "runtime.json");
const PORTS = [17384, 17385, 17386, 17387, 17388];

let cachedPort = null;
let lastSessionId = "mimocode:default";
let lastOutput = "";

function readRuntimePort() {
  try {
    const data = JSON.parse(readFileSync(RUNTIME_PATH, "utf8"));
    return Number.isInteger(Number(data.port)) ? Number(data.port) : null;
  } catch {
    return null;
  }
}

function ports() {
  const out = [];
  const seen = new Set();
  const add = (port) => {
    if (Number.isInteger(port) && !seen.has(port)) {
      seen.add(port);
      out.push(port);
    }
  };
  add(cachedPort);
  add(readRuntimePort());
  for (const port of PORTS) add(port);
  return out;
}

function normalizeSessionId(value) {
  const raw = typeof value === "string" && value ? value : "default";
  return raw.startsWith(`${AGENT_ID}:`) ? raw : `${AGENT_ID}:${raw}`;
}

function eventSessionId(event) {
  const props = event && event.properties && typeof event.properties === "object" ? event.properties : {};
  return props.sessionID || event.sessionID || lastSessionId;
}

function mapEvent(event) {
  if (!event || typeof event.type !== "string") return null;
  const status = event.properties && event.properties.status && event.properties.status.type;

  if (event.type === "session.created") return { state: "idle", name: "SessionStart" };
  if (event.type === "session.deleted" || event.type === "server.instance.disposed") return { state: "sleeping", name: "SessionEnd" };

  if (event.type === "message.part.updated") {
    if (status === "running") return { state: "thinking", name: "UserPromptSubmit" };
    if (status === "tool") return { state: "working", name: "PreToolUse" };
    if (status === "error") return { state: "error", name: "StopFailure" };
  }

  if (event.type === "session.idle") return { state: "attention", name: "Stop" };
  if (event.type === "permission.asked") return { state: "notification", name: "PermissionRequest" };

  return null;
}

function postState(body) {
  const payload = JSON.stringify(body);
  for (const port of ports()) {
    fetch(`http://127.0.0.1:${port}/api/hook`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: payload,
    }).then((res) => {
      if (res.ok) cachedPort = port;
    }).catch(() => {});
  }
}

export default async function codePetMimocodePlugin(ctx = {}) {
  const cwd = typeof ctx.directory === "string" ? ctx.directory : "";

  return {
    event: async ({ event }) => {
      const mapped = mapEvent(event);
      const rawSessionId = eventSessionId(event);
      if (rawSessionId) lastSessionId = normalizeSessionId(rawSessionId);

      let output = "";
      if (event.properties) {
        if (event.properties.text) output = event.properties.text;
        else if (event.properties.output) output = event.properties.output;
        else if (event.properties.message) output = event.properties.message;
      }
      if (output) lastOutput = output.substring(0, 120);

      if (!mapped) return;
      postState({
        agentId: AGENT_ID,
        agentName: AGENT_NAME,
        sessionId: lastSessionId,
        cwd,
        state: mapped.state,
        event: mapped.name,
        output: lastOutput,
      });
    },

    "tool.execute.before": async ({ tool, sessionID, callID }) => {
      const sessionId = sessionID ? normalizeSessionId(sessionID) : lastSessionId;
      lastOutput = `Running: ${tool}`;
      postState({
        agentId: AGENT_ID,
        agentName: AGENT_NAME,
        sessionId,
        cwd,
        state: "working",
        event: `tool:${tool}`,
        output: lastOutput,
      });
    },

    "tool.execute.after": async ({ tool, sessionID, callID, args }) => {
      const sessionId = sessionID ? normalizeSessionId(sessionID) : lastSessionId;
      let output = `Done: ${tool}`;
      if (args && args.command) output = args.command.substring(0, 120);
      else if (args && args.pattern) output = `Search: ${args.pattern}`.substring(0, 120);
      else if (args && args.file_path) output = args.file_path.substring(0, 120);
      lastOutput = output;
      postState({
        agentId: AGENT_ID,
        agentName: AGENT_NAME,
        sessionId,
        cwd,
        state: "thinking",
        event: `tool:${tool}:done`,
        output: lastOutput,
      });
    },

    "chat.message": async ({ sessionID, agent, model }) => {
      const sessionId = sessionID ? normalizeSessionId(sessionID) : lastSessionId;
      const modelName = model ? `${model.providerID}/${model.modelID}` : "";
      lastOutput = modelName ? `Model: ${modelName}` : "New message";
      postState({
        agentId: AGENT_ID,
        agentName: AGENT_NAME,
        sessionId,
        cwd,
        state: "thinking",
        event: "chat.message",
        output: lastOutput,
      });
    },
  };
}
