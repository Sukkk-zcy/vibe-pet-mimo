"use strict";

const fs = require("fs");

const SLIP_END = 0xc0;
const SLIP_ESC = 0xdb;
const SLIP_ESC_END = 0xdc;
const SLIP_ESC_ESC = 0xdd;
let SerialPortClass = null;
let espToolModule = null;

function getSerialPort() {
  if (SerialPortClass) return SerialPortClass;
  try {
    ({ SerialPort: SerialPortClass } = require("serialport"));
  } catch (err) {
    const message = err && err.message ? err.message : String(err || "");
    throw new Error(`serialport native module is not available: ${message}`);
  }
  return SerialPortClass;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function toUint8Array(data) {
  if (data instanceof Uint8Array) return data;
  if (Buffer.isBuffer(data)) return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  return new Uint8Array(data || []);
}

function concatBytes(a, b) {
  const out = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

function formatHex(bytes) {
  return Array.from(bytes)
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

class NodeEspTransport {
  constructor(portPath, options = {}) {
    this.path = portPath;
    this.tracing = Boolean(options.tracing);
    this.onLog = typeof options.onLog === "function" ? options.onLog : () => {};
    this.shouldCancel = typeof options.shouldCancel === "function" ? options.shouldCancel : () => false;
    this.port = null;
    this.buffer = new Uint8Array(0);
    this.baudrate = 0;
    this.traceLog = "";
    this.lastTraceTime = Date.now();
    this.lastError = null;
    this._DTR_state = false;
  }

  getInfo() {
    return `Node Serial ${this.path}`;
  }

  getPid() {
    return undefined;
  }

  trace(message) {
    if (!this.tracing) return;
    const delta = Date.now() - this.lastTraceTime;
    const text = `TRACE ${delta.toFixed(3)} ${message}`;
    this.traceLog += `${text}\n`;
    this.onLog(`${text}\n`);
  }

  hexify(bytes) {
    return formatHex(toUint8Array(bytes));
  }

  hexConvert(bytes, autoSplit = true) {
    const data = toUint8Array(bytes);
    if (!autoSplit || data.length <= 16) return this.hexify(data).padEnd(16, " ");

    let out = "";
    for (let offset = 0; offset < data.length; offset += 16) {
      const line = data.slice(offset, offset + 16);
      const left = this.hexify(line.slice(0, 8)).padEnd(16, " ");
      const right = this.hexify(line.slice(8)).padEnd(16, " ");
      const ascii = Array.from(line)
        .map((byte) => (byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : "."))
        .join("");
      out += `\n    ${left} ${right} | ${ascii}`;
    }
    return out;
  }

  throwIfCancelled() {
    if (this.shouldCancel()) {
      throw new Error("Firmware flash cancelled.");
    }
  }

  async connect(baud = 115200, serialOptions = {}) {
    this.throwIfCancelled();
    await this.disconnect();
    this.buffer = new Uint8Array(0);
    this.lastError = null;

    const SerialPort = getSerialPort();
    this.port = new SerialPort({
      path: this.path,
      baudRate: baud,
      autoOpen: false,
      dataBits: serialOptions.dataBits || 8,
      stopBits: serialOptions.stopBits || 1,
      parity: serialOptions.parity || "none",
      rtscts: serialOptions.flowControl === "hardware",
    });

    this.port.on("data", (chunk) => {
      this.buffer = concatBytes(this.buffer, toUint8Array(chunk));
    });
    this.port.on("error", (err) => {
      this.lastError = err;
    });

    await new Promise((resolve, reject) => {
      this.port.open((err) => (err ? reject(err) : resolve()));
    });
    this.baudrate = baud;
  }

  async disconnect() {
    if (!this.port) return;
    const current = this.port;
    this.port = null;
    if (!current.isOpen) return;
    await new Promise((resolve) => {
      current.close(() => resolve());
    });
  }

  slipWriter(data) {
    const input = toUint8Array(data);
    const out = [SLIP_END];
    for (const byte of input) {
      if (byte === SLIP_ESC) out.push(SLIP_ESC, SLIP_ESC_ESC);
      else if (byte === SLIP_END) out.push(SLIP_ESC, SLIP_ESC_END);
      else out.push(byte);
    }
    out.push(SLIP_END);
    return new Uint8Array(out);
  }

  async write(data) {
    this.throwIfCancelled();
    if (!this.port || !this.port.isOpen) throw new Error("Serial port is not open.");
    const outData = this.slipWriter(data);
    if (this.tracing) this.trace(`Write ${outData.length} bytes: ${this.hexConvert(outData)}`);
    await new Promise((resolve, reject) => {
      this.port.write(Buffer.from(outData), (writeErr) => {
        if (writeErr) {
          reject(writeErr);
          return;
        }
        this.port.drain((drainErr) => (drainErr ? reject(drainErr) : resolve()));
      });
    });
  }

  async waitForBytes(timeout) {
    const start = Date.now();
    while (Date.now() - start < timeout) {
      this.throwIfCancelled();
      if (this.lastError) throw this.lastError;
      if (this.buffer.length > 0) {
        const bytes = this.buffer;
        this.buffer = new Uint8Array(0);
        return bytes;
      }
      await sleep(1);
    }
    return new Uint8Array(0);
  }

  async read(timeout) {
    let partialPacket = null;
    let isEscaping = false;

    while (true) {
      const readBytes = await this.waitForBytes(timeout);
      if (!readBytes.length) {
        throw new Error(partialPacket === null ? "Serial data stream stopped." : "No serial data received.");
      }
      if (this.tracing) this.trace(`Read ${readBytes.length} bytes: ${this.hexConvert(readBytes)}`);

      for (let i = 0; i < readBytes.length; i += 1) {
        const byte = readBytes[i];
        if (partialPacket === null) {
          if (byte !== SLIP_END) {
            throw new Error(`Invalid head of packet (0x${byte.toString(16)}).`);
          }
          partialPacket = new Uint8Array(0);
        } else if (isEscaping) {
          isEscaping = false;
          if (byte === SLIP_ESC_END) partialPacket = concatBytes(partialPacket, new Uint8Array([SLIP_END]));
          else if (byte === SLIP_ESC_ESC) partialPacket = concatBytes(partialPacket, new Uint8Array([SLIP_ESC]));
          else throw new Error(`Invalid SLIP escape (0xdb, 0x${byte.toString(16)}).`);
        } else if (byte === SLIP_ESC) {
          isEscaping = true;
        } else if (byte === SLIP_END) {
          if (i + 1 < readBytes.length) {
            this.buffer = concatBytes(readBytes.slice(i + 1), this.buffer);
          }
          return partialPacket;
        } else {
          partialPacket = concatBytes(partialPacket, new Uint8Array([byte]));
        }
      }
    }
  }

  flushInput() {
    this.buffer = new Uint8Array(0);
  }

  readLoop() {
    // serialport data events continuously fill this.buffer; esptool-js calls
    // readLoop() after baud-rate changes when using the browser transport.
  }

  async flushOutput() {
    if (!this.port || !this.port.isOpen) return;
    await new Promise((resolve, reject) => {
      this.port.drain((err) => (err ? reject(err) : resolve()));
    });
  }

  inWaiting() {
    return this.buffer.length;
  }

  peek() {
    return this.buffer;
  }

  async setSignals(signals) {
    if (!this.port || !this.port.isOpen) return;
    await new Promise((resolve, reject) => {
      this.port.set(signals, (err) => (err ? reject(err) : resolve()));
    });
  }

  async setRTS(state) {
    await this.setSignals({ rts: Boolean(state) });
    await this.setDTR(this._DTR_state);
  }

  async setDTR(state) {
    this._DTR_state = Boolean(state);
    await this.setSignals({ dtr: this._DTR_state });
  }

  async waitForUnlock(timeout) {
    await sleep(timeout);
  }
}

async function loadEspTool() {
  if (espToolModule) return espToolModule;
  const bundlePath = require.resolve("esptool-js/bundle.js");
  const source = fs.readFileSync(bundlePath);
  const dataUrl = `data:text/javascript;base64,${source.toString("base64")}`;
  espToolModule = await import(dataUrl);
  return espToolModule;
}

async function listSerialPorts() {
  const SerialPort = getSerialPort();
  const ports = await SerialPort.list();
  return ports.map((item) => ({
    path: item.path,
    label: [item.manufacturer, item.friendlyName, item.serialNumber].filter(Boolean).join(" - ") || item.path,
  }));
}

async function flashEspMainBin(options = {}) {
  const {
    port,
    firmwarePath,
    flashAddress = 0x0,
    baudRate = 460800,
    onLog = () => {},
    onProgress = () => {},
    shouldCancel = () => false,
  } = options;

  if (!port) throw new Error("Serial port is required.");
  if (!firmwarePath || !fs.existsSync(firmwarePath)) {
    throw new Error(`main.bin not found: ${firmwarePath}`);
  }

  const firmware = new Uint8Array(fs.readFileSync(firmwarePath));
  if (!firmware.length) throw new Error(`main.bin is empty: ${firmwarePath}`);

  const { ESPLoader } = await loadEspTool();
  const transport = new NodeEspTransport(port, { onLog, shouldCancel });
  const terminal = {
    clean() {},
    writeLine(data) {
      onLog(`${data}\n`);
    },
    write(data) {
      onLog(String(data));
    },
  };

  const loader = new ESPLoader({
    transport,
    baudrate: baudRate,
    terminal,
    debugLogging: false,
  });

  const throwIfCancelled = () => {
    if (shouldCancel()) throw new Error("Firmware flash cancelled.");
  };

  try {
    onLog(`Opening ${port} at ${baudRate} baud\n`);
    const chip = await loader.main("default_reset");
    throwIfCancelled();
    onLog(`Connected to ${chip}\n`);
    onLog(`Writing ${firmware.length} bytes from main.bin at 0x${flashAddress.toString(16)}\n`);
    await loader.writeFlash({
      fileArray: [{ data: firmware, address: flashAddress }],
      flashMode: "keep",
      flashFreq: "keep",
      flashSize: "keep",
      eraseAll: false,
      compress: true,
      reportProgress(_fileIndex, written, total) {
        onProgress(written, total);
      },
    });
    throwIfCancelled();
    await loader.after("hard_reset");
  } finally {
    await transport.disconnect();
  }
}

module.exports = {
  flashEspMainBin,
  listSerialPorts,
};
