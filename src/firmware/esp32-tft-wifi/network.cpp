#include "network.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <ESPmDNS.h>

static Preferences prefs;
static String wifiSSID = "";
static String wifiPass = "";
static String bridgeHost = DEFAULT_BRIDGE_HOST;
static String resolvedIP = "";     // 缓存解析后的 IP
static uint16_t bridgePort = DEFAULT_BRIDGE_PORT;
static bool wifiConnected = false;
static unsigned long lastPollTime = 0;
static int reconnectAttempts = 0;
static int resolveCounter = 0;     // 每 10 轮重新解析主机名

static WebServer server(80);
static DNSServer dnsServer;
static bool configMode = false;

static const char* CONFIG_PAGE_HTML = R"rawliteral(
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
input[type=text],input[type=number]{width:100%;padding:10px;border:1px solid #333;border-radius:5px;background:#16213e;color:#eee;font-size:16px}
button{width:100%;padding:12px;margin-top:20px;background:#e94560;color:#fff;border:none;border-radius:5px;font-size:18px;cursor:pointer}
button:hover{background:#c81d4e}
.info{text-align:center;color:#888;margin-top:10px;font-size:14px}
.hint{color:#666;font-size:13px;margin:2px 0 0 0}
</style>
</head>
<body>
<h1>VibePet Setup</h1>
<form action="/save" method="POST">
<label>WiFi SSID</label>
<input type="text" name="ssid" required>
<label>WiFi Password</label>
<input type="text" name="pass">
<label>Bridge Host (IP 或主机名)</label>
<input type="text" name="host" value="192.168.1.2">
<p class="hint">💡 填电脑的主机名（如 DESKTOP-ABC），ESP 会自动解析 IP，电脑重启也不怕</p>
<label>Bridge Port</label>
<input type="number" name="port" value="17384" min="1" max="65535">
<button type="submit">Save & Restart</button>
</form>
<p class="info">After saving, the device will restart and connect to your WiFi.</p>
</body>
</html>
)rawliteral";

// 判断字符串是否为 IP 地址格式
static bool looksLikeIP(const String& s) {
    int dots = 0;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '.') { dots++; continue; }
        if (!isDigit(c)) return false;
    }
    return dots == 3;
}

static void resolveBridgeAddr() {
    if (bridgeHost.length() == 0) {
        resolvedIP = "";
        return;
    }

    // 如果是 IP 地址，直接用
    if (looksLikeIP(bridgeHost)) {
        resolvedIP = bridgeHost;
        Serial.printf("Bridge: using static IP %s\n", resolvedIP.c_str());
        return;
    }

    // 先试 mDNS 解析（支持 vibepet.local 这类名字）
    if (MDNS.begin("vibepet-esp")) {
        IPAddress ip = MDNS.queryHost(bridgeHost);
        if (ip != INADDR_NONE) {
            resolvedIP = ip.toString();
            MDNS.end();
            Serial.printf("Bridge: mDNS resolved %s -> %s\n", bridgeHost.c_str(), resolvedIP.c_str());
            return;
        }
        MDNS.end();
    }

    // 再试 DNS 解析（通过路由器 DNS 解析主机名）
    IPAddress ip;
    if (WiFi.hostByName(bridgeHost.c_str(), ip)) {
        resolvedIP = ip.toString();
        Serial.printf("Bridge: DNS resolved %s -> %s\n", bridgeHost.c_str(), resolvedIP.c_str());
        return;
    }

    // 都失败，直接用原始值（让 HTTP 请求失败后重试）
    resolvedIP = bridgeHost;
    Serial.printf("Bridge: failed to resolve %s, using raw\n", bridgeHost.c_str());
}

void networkLoadConfig() {
    prefs.begin(PREF_NAMESPACE, true);
    wifiSSID = prefs.getString(PREF_KEY_SSID, "");
    wifiPass = prefs.getString(PREF_KEY_PASS, "");
    String host = prefs.getString(PREF_KEY_HOST, DEFAULT_BRIDGE_HOST);
    bridgePort = prefs.getUShort(PREF_KEY_PORT, DEFAULT_BRIDGE_PORT);
    currentBrightness = prefs.getInt(PREF_KEY_BRIGHT, BRIGHTNESS_DEFAULT);
    prefs.end();
    if (host.length() > 0) bridgeHost = host;
}

void networkSaveConfig(const char* ssid, const char* pass, const char* host, uint16_t port) {
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString(PREF_KEY_SSID, ssid);
    prefs.putString(PREF_KEY_PASS, pass);
    prefs.putString(PREF_KEY_HOST, host);
    prefs.putUShort(PREF_KEY_PORT, port);
    prefs.putInt(PREF_KEY_BRIGHT, currentBrightness);
    prefs.end();
    wifiSSID = ssid;
    wifiPass = pass;
    bridgeHost = host;
    bridgePort = port;
}

static void handleRoot() {
    server.send(200, "text/html", CONFIG_PAGE_HTML);
}

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String host = server.arg("host");
    uint16_t port = server.arg("port").toInt();

    if (host.length() == 0) host = DEFAULT_BRIDGE_HOST;
    if (port == 0) port = DEFAULT_BRIDGE_PORT;

    Serial.printf("Save: ssid=%s, host=%s, port=%d\n", ssid.c_str(), host.c_str(), port);
    networkSaveConfig(ssid.c_str(), pass.c_str(), host.c_str(), port);

    server.send(200, "text/html",
        "<html><body style='font-family:Arial;text-align:center;padding:40px;background:#1a1a2e;color:#eee'>"
        "<h1 style='color:#e94560'>Saved!</h1>"
        "<p>Restarting...</p>"
        "</body></html>");

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

    Serial.println("Config portal started at 192.168.4.1");
}

bool networkIsConnected() {
    return wifiConnected && WiFi.status() == WL_CONNECTED;
}

void networkInit() {
    networkLoadConfig();

    if (wifiSSID.length() == 0) {
        networkStartConfigPortal();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

    // 等待 WiFi 连接
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        reconnectAttempts = 0;
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
        Serial.printf("Connected to %s, IP: %s\n", wifiSSID.c_str(), WiFi.localIP().toString().c_str());

        // 解析桥接地址（IP 或主机名 -> IP）
        resolveBridgeAddr();
    } else {
        wifiConnected = false;
        WiFi.disconnect();
        networkStartConfigPortal();
    }
}

void networkPoll() {
    if (!wifiConnected) return;

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        isConnected = false;
        uiDirty = true;
        return;
    }

    // 每 10 轮重新解析主机名（应对电脑 IP 变化）
    resolveCounter++;
    if (resolveCounter >= 10) {
        resolveCounter = 0;
        if (!looksLikeIP(bridgeHost)) {
            resolveBridgeAddr();
        }
    }

    unsigned long now = millis();
    if (now - lastPollTime < POLL_INTERVAL_MS) return;
    lastPollTime = now;

    // 如果 resolvedIP 为空先解析
    if (resolvedIP.length() == 0) {
        resolveBridgeAddr();
    }

    HTTPClient http;
    String url = "http://" + resolvedIP + ":" + String(bridgePort) + "/api/device-snapshot";

    http.begin(url);
    http.setTimeout(3000);

    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        stateParse(payload.c_str());
        isConnected = true;
        reconnectAttempts = 0;
    } else if (httpCode > 0) {
        isConnected = false;
    } else {
        reconnectAttempts++;
        if (reconnectAttempts > 10) {
            isConnected = false;
            // 尝试重新解析主机名（可能 IP 变了）
            if (!looksLikeIP(bridgeHost)) {
                resolveBridgeAddr();
            }
        }
    }

    http.end();
}

void networkHandleClient() {
    if (configMode) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

String networkGetSSID() { return wifiSSID; }
String networkGetHost() { return bridgeHost; }
uint16_t networkGetPort() { return bridgePort; }
