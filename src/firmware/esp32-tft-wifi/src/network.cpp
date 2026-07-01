#include "network.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include <ArduinoJson.h>

static Preferences prefs;
static String wifiSSID = "";
static String wifiPass = "";

static WebServer server(80);
static DNSServer dnsServer;
static bool configMode = false;

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
input[type=text],input[type=password]{width:100%;padding:10px;border:1px solid #333;border-radius:5px;background:#16213e;color:#eee;font-size:16px}
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
<button type="submit">Save & Restart</button>
</form>
<p class="info">After saving, the device will restart.</p>
</body>
</html>
)rawliteral";

// ─── WiFi 配置存储 ────────────────────────────────────────

void networkLoadConfig() {
    prefs.begin("vibepet", true);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    currentBrightness = prefs.getInt("bright", 128);
    prefs.end();
}

void networkSaveConfig(const char* ssid, const char* pass) {
    prefs.begin("vibepet", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putInt("bright", currentBrightness);
    prefs.end();
    wifiSSID = ssid;
    wifiPass = pass;
}

// ─── 配置门户 ────────────────────────────────────────────

static void handleRoot() {
    server.send(200, "text/html", CONFIG_PAGE);
}

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() == 0) { server.send(400, "text/plain", "SSID required"); return; }

    networkSaveConfig(ssid.c_str(), pass.c_str());
    server.send(200, "text/html",
        "<html><body style='font-family:Arial;text-align:center;padding:40px;background:#1a1a2e;color:#eee'>"
        "<h1 style='color:#e94560'>Saved!</h1><p>Restarting...</p></body></html>");
    delay(2000);
    ESP.restart();
}

void networkStartConfigPortal() {
    configMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    dnsServer.start(53, "*", apIP);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);
    server.begin();
}

// ─── 状态接收 API ────────────────────────────────────────

static void handleState() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "no body");
        return;
    }

    String body = server.arg("plain");
    bool ok = stateParse(body.c_str());
    if (ok) {
        isConnected = true;
        server.send(200, "text/plain", "OK");
        Serial.printf("[API] %s <- %s\n", pet.state.c_str(), pet.agent.c_str());
    } else {
        server.send(400, "text/plain", "parse failed");
    }
}

// ─── 初始化 ──────────────────────────────────────────────

void networkInit() {
    networkLoadConfig();

    if (wifiSSID.length() == 0) {
        networkStartConfigPortal();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        networkStartConfigPortal();
        return;
    }

    configTime(8 * 3600, 0, "ntp1.aliyun.com");
    Serial.printf("WiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());

    // 启动 HTTP 服务
    server.on("/api/state", HTTP_POST, handleState);
    server.begin();
    Serial.printf("API server started at http://%s/api/state\n", WiFi.localIP().toString().c_str());
}

// ─── 每帧调用 ────────────────────────────────────────────

void networkHandle() {
    if (configMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

void networkHandleClient() {
    networkHandle();
}

String networkGetSSID() { return wifiSSID; }
