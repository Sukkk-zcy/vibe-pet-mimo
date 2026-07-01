#include "network.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <ArduinoJson.h>

static Preferences prefs;
static String wifiSSID = "";
static String wifiPass = "";
static String bridgeHost = DEFAULT_BRIDGE_HOST;
static uint16_t bridgePort = DEFAULT_BRIDGE_PORT;
static bool wifiConnected = false;
static unsigned long lastPollTime = 0;
static int failCount = 0;

static WebServer server(80);
static DNSServer dnsServer;
static bool configMode = false;

// ─── 配置页面 ────────────────────────────────────────────
static const char* CONFIG_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>VibePet Setup</title>
<style>
body{font-family:Arial,sans-serif;max-width:400px;margin:40px auto;padding:20px;background:#1a1a2e;color:#eee}
h1{color:#e94560;text-align:center}
label{display:block;margin:15px 0 5px;font-weight:bold}
input[type=text],input[type=password],input[type=number]{width:100%;padding:10px;border:1px solid #333;border-radius:5px;background:#16213e;color:#eee;font-size:16px}
button{width:100%;padding:12px;margin-top:20px;background:#e94560;color:#fff;border:none;border-radius:5px;font-size:18px;cursor:pointer}
button:hover{background:#c81d4e}
.info{text-align:center;color:#888;margin-top:10px;font-size:14px}
</style>
</head>
<body>
<h1>VibePet Setup</h1>
<form action="/save" method="POST">
<label>WiFi SSID</label>
<input type="text" name="ssid" required>
<label>WiFi Password</label>
<input type="password" name="pass">
<label>VibePet 电脑 IP</label>
<input type="text" name="host" value="192.168.1.2">
<label>端口</label>
<input type="number" name="port" value="17384">
<button type="submit">Save & Restart</button>
</form>
<p class="info">填你电脑的 IP，运行着 VibePet 桌面端的那台</p>
</body>
</html>
)rawliteral";

// ─── WiFi 存储 ────────────────────────────────────────────
void networkLoadConfig() {
    prefs.begin("vibepet", true);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    bridgeHost = prefs.getString("host", DEFAULT_BRIDGE_HOST);
    bridgePort = prefs.getUShort("port", DEFAULT_BRIDGE_PORT);
    currentBrightness = prefs.getInt("bright", 128);
    prefs.end();
}

void networkSaveConfig(const char* ssid, const char* pass, const char* host, uint16_t port) {
    prefs.begin("vibepet", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("host", host);
    prefs.putUShort("port", port);
    prefs.putInt("bright", currentBrightness);
    prefs.end();
    wifiSSID = ssid; wifiPass = pass; bridgeHost = host; bridgePort = port;
}

// ─── 配置门户 ────────────────────────────────────────────
static void handleRoot() { server.send(200, "text/html", CONFIG_PAGE); }

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String host = server.arg("host");
    uint16_t port = server.arg("port").toInt();
    if (ssid.length() == 0 || host.length() == 0) { server.send(400, "text/plain", "missing"); return; }
    if (port == 0) port = 17384;
    networkSaveConfig(ssid.c_str(), pass.c_str(), host.c_str(), port);
    server.send(200, "text/html", "<html><body style='background:#1a1a2e;color:#eee;text-align:center;padding:40px;font-family:Arial'><h1 style='color:#e94560'>Saved!</h1><p>Restarting...</p></body></html>");
    delay(2000);
    ESP.restart();
}

void networkStartConfigPortal() {
    configMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    dnsServer.start(53, "*", IPAddress(192,168,4,1));
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);
    server.begin();
}

// ─── POST /api/state（供将来直接推送）────────────────────
static void handleStatePost() {
    if (!server.hasArg("plain")) { server.send(400); return; }
    if (stateParse(server.arg("plain").c_str())) {
        isConnected = true;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "bad json");
    }
}

// ─── 轮询桥接 ────────────────────────────────────────────
static void pollBridge() {
    if (bridgeHost.length() == 0) return;

    HTTPClient http;
    String url = "http://" + bridgeHost + ":" + String(bridgePort) + "/api/device-snapshot";
    http.begin(url);
    http.setTimeout(2000);

    int code = http.GET();
    if (code == 200) {
        stateParse(http.getString().c_str());
        isConnected = true;
        failCount = 0;
    } else {
        failCount++;
        if (failCount > 10) isConnected = false;
    }
    http.end();
}

void networkInit() {
    networkLoadConfig();

    if (wifiSSID.length() == 0) { networkStartConfigPortal(); return; }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) delay(100);

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        networkStartConfigPortal();
        return;
    }

    configTime(8 * 3600, 0, "ntp1.aliyun.com");
    Serial.printf("WiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());

    // 启动 HTTP 服务（接收直推 + 配置页面）
    server.on("/api/state", HTTP_POST, handleStatePost);
    server.begin();

    // 首次轮询
    pollBridge();
    Serial.printf("Bridge: %s :%d %s\n", bridgeHost.c_str(), bridgePort,
        isConnected ? "connected" : "unreachable");
}

void networkHandle() {
    if (configMode) dnsServer.processNextRequest();
    server.handleClient();

    if (!configMode && millis() - lastPollTime > 500) {
        lastPollTime = millis();
        pollBridge();
    }
}

String networkGetSSID() { return wifiSSID; }
String networkGetHost() { return bridgeHost; }
uint16_t networkGetPort() { return bridgePort; }
