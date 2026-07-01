#include "network.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static String wifiSSID = "";
static String wifiPass = "";
static String bridgeHost = "";
static uint16_t bridgePort = 17384;
static bool wifiConnected = false;
static unsigned long lastPollTime = 0;
static int failCount = 0;

// 自动扫描
static bool scanning = false;
static uint32_t scanIP = 0;
static uint32_t scanEnd = 0;
static uint32_t scanStartTime = 0;

// HTTP 服务 + 配置门户
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
<p class="info">保存后 ESP32 会自动扫描局域网找到你的电脑</p>
</body>
</html>
)rawliteral";

// ─── 存储 ────────────────────────────────────────────────
void networkLoadConfig() {
    prefs.begin("vibepet", true);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    bridgeHost = prefs.getString("host", "");
    bridgePort = prefs.getUShort("port", 17384);
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

// ─── 配置门户 ────────────────────────────────────────────
static void handleRoot() { server.send(200, "text/html", CONFIG_PAGE); }

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() == 0) { server.send(400); return; }
    networkSaveConfig(ssid.c_str(), pass.c_str());
    server.send(200, "text/html",
        "<html><body style='background:#1a1a2e;color:#eee;text-align:center;padding:40px;font-family:Arial'>"
        "<h1 style='color:#e94560'>Saved!</h1><p>ESP will scan LAN for your PC...</p></body></html>");
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

// ─── POST /api/state（直推接口）─────────────────────────
static void handleStatePost() {
    if (!server.hasArg("plain")) { server.send(400); return; }
    if (stateParse(server.arg("plain").c_str())) {
        isConnected = true; failCount = 0;
        server.send(200, "text/plain", "OK");
    } else { server.send(400); }
}

// ─── 自动扫描局域网 ─────────────────────────────────────
static bool tryBridge(IPAddress ip) {
    HTTPClient http;
    String url = "http://" + ip.toString() + ":" + String(bridgePort) + "/api/device-snapshot";
    http.begin(url);
    http.setTimeout(300);
    int code = http.GET();
    http.end();
    if (code == 200) {
        bridgeHost = ip.toString();
        // 存到 Preferences 以便下次启动快速连接
        prefs.begin("vibepet", false);
        prefs.putString("host", bridgeHost);
        prefs.end();
        return true;
    }
    return false;
}

static void startScan() {
    IPAddress lip = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    uint32_t ip = (uint32_t)lip;
    uint32_t m = (uint32_t)mask;
    scanIP = (ip & m) + 1;
    scanEnd = (ip | ~m) - 1;
    scanStartTime = millis();
    scanning = true;
    Serial.printf("Scan: %s ~ %s\n",
        IPAddress(scanIP).toString().c_str(),
        IPAddress(scanEnd).toString().c_str());
}

static void continueScan() {
    if (!scanning) return;
    for (int i = 0; i < 10 && scanIP <= scanEnd; i++, scanIP++) {
        if (tryBridge(IPAddress(scanIP))) {
            scanning = false;
            failCount = 0;
            isConnected = true;
            Serial.printf("Found bridge at %s\n", bridgeHost.c_str());
            return;
        }
    }
    if (scanIP > scanEnd) {
        scanning = false;
        if (bridgeHost.length() == 0) {
            Serial.println("Scan: no bridge found on this network");
        }
    }
}

// ─── 轮询桥接 ────────────────────────────────────────────
static void pollBridge() {
    if (bridgeHost.length() == 0) {
        if (!scanning) startScan();
        return;
    }

    HTTPClient http;
    String url = "http://" + bridgeHost + ":" + String(bridgePort) + "/api/device-snapshot";
    http.begin(url);
    http.setTimeout(1500);

    int code = http.GET();
    if (code == 200) {
        stateParse(http.getString().c_str());
        isConnected = true;
        failCount = 0;
    } else {
        failCount++;
        if (failCount > 5) {
            isConnected = false;
            // 超过 5 次失败，重新扫描
            if (!scanning) {
                Serial.println("Bridge lost, rescanning...");
                startScan();
            }
        }
    }
    http.end();
}

// ─── 初始化 ──────────────────────────────────────────────
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

    // 如果有保存的 IP 先试试，没有就自动扫描
    if (bridgeHost.length() > 0) {
        pollBridge();
        Serial.printf("Bridge cached: %s %s\n", bridgeHost.c_str(), isConnected ? "OK" : "unreachable");
    }
    if (!isConnected && !scanning) startScan();
}

void networkHandle() {
    if (configMode) dnsServer.processNextRequest();
    server.handleClient();

    if (configMode) return;

    if (scanning) { continueScan(); return; }

    unsigned long now = millis();
    if (now - lastPollTime < 500) return;
    lastPollTime = now;

    pollBridge();
}

String networkGetSSID() { return wifiSSID; }
String networkGetHost() { return bridgeHost; }
uint16_t networkGetPort() { return bridgePort; }
