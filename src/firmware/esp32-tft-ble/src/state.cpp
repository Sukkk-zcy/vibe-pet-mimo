#include "state.h"
#include "config.h"
#include <ArduinoJson.h>

VibePetPacket pet;
bool isConnected = false;
int currentBrightness = BRIGHTNESS_DEFAULT;

void stateInit() {
    stateReset();
}

void stateReset() {
    pet.state = STATE_IDLE;
    pet.stateLabel = "";
    pet.agent = "MiMoCode";
    pet.event = "";
    pet.title = "";
    pet.output = "";
    pet.personaSlug = "lulu";
    pet.personaName = "Lulu";
    pet.personaKind = "";
    pet.activeCount = 0;
    pet.receivedAt = 0;
    isConnected = false;
}

static String cleanText(const String& input, uint8_t maxLen) {
    String out = input;
    out.trim();
    out.replace("\n", " ");
    out.replace("\r", " ");
    while (out.indexOf("  ") >= 0) out.replace("  ", " ");
    if (out.length() > maxLen) {
        out = out.substring(0, maxLen - 3) + "...";
    }
    return out;
}

static String normalizeState(const String& state) {
    String s = state;
    s.trim();
    s.toLowerCase();
    if (s == "permission" || s == "codex-permission") return STATE_NOTIFICATION;
    if (s == STATE_IDLE || s == STATE_THINKING || s == STATE_WORKING ||
        s == STATE_TYPING || s == STATE_BUILDING || s == STATE_JUGGLING ||
        s == STATE_ATTENTION || s == STATE_NOTIFICATION || s == STATE_ERROR ||
        s == STATE_SWEEPING || s == STATE_SLEEPING) {
        return s;
    }
    return STATE_IDLE;
}

bool stateParse(const char* json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) return false;

    if (doc["im"].is<const char*>()) return false;

    String state = STATE_IDLE;
    String agent = "MiMoCode";
    String event = "";
    String title = "";
    String output = "";
    String personaName = "Lulu";
    String personaSlug = "lulu";
    int activeCount = 0;

    if (doc["aggregate"].is<JsonObject>()) {
        JsonObject agg = doc["aggregate"];
        state = agg["s"] | agg["state"] | STATE_IDLE;
        agent = agg["a"] | agg["agentName"] | agg["agent"] | "MiMoCode";
        event = agg["e"] | agg["event"] | "";
        title = agg["m"] | agg["title"] | "";
        if (agg["o"].is<const char*>()) output = agg["o"].as<String>();
        else if (agg["output"].is<const char*>()) output = agg["output"].as<String>();
        activeCount = agg["n"] | agg["activeCount"] | 0;

        if (agg["d"].is<const char*>()) personaName = agg["d"].as<String>();
        if (agg["p"].is<const char*>()) personaSlug = agg["p"].as<String>();
    } else if (doc["pets"].is<JsonArray>()) {
        JsonArray pets = doc["pets"].as<JsonArray>();
        bool found = false;
        for (JsonObject p : pets) {
            String s = p["state"] | STATE_IDLE;
            if (s != STATE_IDLE && s != STATE_SLEEPING) {
                state = s;
                agent = p["agentName"] | p["agentId"] | "MiMoCode";
                title = p["title"] | "";
                activeCount = 1;
                if (p["persona"].is<JsonObject>()) {
                    JsonObject persona = p["persona"];
                    personaName = persona["displayName"] | "Lulu";
                    personaSlug = persona["slug"] | "lulu";
                }
                found = true;
                break;
            }
        }
        if (!found && pets.size() > 0) {
            JsonObject p = pets[0];
            state = p["state"] | STATE_IDLE;
            agent = p["agentName"] | p["agentId"] | "MiMoCode";
            title = p["title"] | "";
            activeCount = 1;
            if (p["persona"].is<JsonObject>()) {
                JsonObject persona = p["persona"];
                personaName = persona["displayName"] | "Lulu";
                personaSlug = persona["slug"] | "lulu";
            }
        }
    } else {
        state = doc["s"] | doc["state"] | STATE_IDLE;
        agent = doc["a"] | doc["agentName"] | doc["agent"] | "MiMoCode";
        event = doc["e"] | doc["event"] | "";
        title = doc["m"] | doc["title"] | "";
        if (doc["o"].is<const char*>()) output = doc["o"].as<String>();
        else if (doc["output"].is<const char*>()) output = doc["output"].as<String>();
        activeCount = doc["n"] | doc["activeCount"] | 0;
        if (doc["d"].is<const char*>()) personaName = doc["d"].as<String>();
        if (doc["p"].is<const char*>()) personaSlug = doc["p"].as<String>();
    }

    state = normalizeState(state);

    Serial.print("Parsed: s=");
    Serial.print(state);
    Serial.print(" o=");
    Serial.println(output);

    if (pet.state != state || pet.agent != agent || pet.title != title ||
        pet.output != output || pet.personaName != personaName) {
        pet.state = state;
        pet.agent = cleanText(agent, 20);
        pet.event = cleanText(event, 24);
        pet.title = cleanText(title, 32);
        pet.output = cleanText(output, 80);
        pet.personaName = cleanText(personaName, 18);
        pet.personaSlug = personaSlug;
        pet.activeCount = activeCount;
        pet.receivedAt = millis();
    }

    return true;
}
