#include "network.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static String wifiSSID = "";
static String wifiPass = "";

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
input[type=text],input[type=password]{width:100%;padding:10px;border:1px solid #333;border-radius:5px;background:#16213e;color:#eee;font-size:16px}
button{width:100%;padding:12px;margin-top:20px;background:#e94560;color:#fff;border:none;border-radius:5px;font-size:18px;cursor:pointer}
button:hover{background:#c81d4e}
.info{text-align:center;color:#888;margin-top:10px;font-size:13px}
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
<p class="info">电脑端请运行 VibePetBridge.exe 推送到本设备</p>
</body>
</html>
)rawliteral";

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
    wifiSSID = ssid; wifiPass = pass;
}

static void handleRoot() { server.send(200, "text/html", CONFIG_PAGE); }

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() == 0) { server.send(400); return; }
    networkSaveConfig(ssid.c_str(), pass.c_str());
    server.send(200, "text/html",
        "<html><body style='background:#1a1a2e;color:#eee;text-align:center;padding:40px;font-family:Arial'>"
        "<h1 style='color:#e94560'>Saved!</h1><p>Restarting...</p></body></html>");
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

// ─── POST /api/state ─────────────────────────────────────
static void handleStatePost() {
    if (!server.hasArg("plain")) { server.send(400); return; }
    if (stateParse(server.arg("plain").c_str())) {
        isConnected = true;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "bad json");
    }
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

    server.on("/api/state", HTTP_POST, handleStatePost);
    server.begin();
    Serial.printf("Ready at http://%s/api/state\n", WiFi.localIP().toString().c_str());
}

void networkHandle() {
    if (configMode) dnsServer.processNextRequest();
    server.handleClient();
}

String networkGetSSID() { return wifiSSID; }
