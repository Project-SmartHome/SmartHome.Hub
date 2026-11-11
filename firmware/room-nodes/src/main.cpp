// SmartHome room node.
// Joins Wi-Fi, connects to Ably's MQTT adapter over TLS and subscribes to
// user/<userId>/room/<roomId>. SmartHome.App publishes {"gpio":<pin>,"value":"on"|"off"}
// there when a button is toggled; the node switches the relay on that GPIO.
// Builds for ESP32, NodeMCU and generic ESP8266 boards.
//
// Updates itself from GitHub Releases (see ../OTA.md). Credentials come from
// secrets.h on a USB flash and are kept in flash for later OTA images.

#include <Arduino.h>
#include <time.h>

#if defined(ESP32)
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #define SH_BOARD_NAME "ESP32"
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <WiFiClientSecureBearSSL.h>
    #define SH_BOARD_NAME "ESP8266"
#else
    #error "Unsupported board: build for an ESP32 or ESP8266 target"
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <DeviceSettings.h>
#include <OtaConfig.h>
#include <OtaHttp.h>
#include <OtaRollback.h>

// Needed for the first USB flash of each board (copy secrets.example.h).
// OTA images are built without it and use the credentials saved in flash.
#if __has_include("secrets.h")
    #include "secrets.h"
#endif
#ifndef WIFI_SSID
    #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD ""
#endif
#ifndef ABLY_KEY_NAME
    #define ABLY_KEY_NAME ""
#endif
#ifndef ABLY_KEY_SECRET
    #define ABLY_KEY_SECRET ""
#endif
#ifndef SH_USER_ID
    #define SH_USER_ID ""
#endif
#ifndef SH_ROOM_ID
    #define SH_ROOM_ID ""
#endif

#include "AblyRootCA.h"

#if defined(ESP32)
// Leave the post-OTA "is this image good?" decision to ShOtaRollback: a new
// image is confirmed once it is subscribed to its room channel, or rolled
// back if it never gets there.
extern "C" bool verifyRollbackLater() {
    return true;
}

// Firmware downloads verify TLS certificates on the loop task; the 8 KB
// Arduino default is too tight for that (it overflowed on the gateway).
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
#endif

// Wokwi's simulated access point is open and fixed to channel 6; naming the
// channel skips the scan.
#if defined(SH_WOKWI)
    #undef WIFI_SSID
    #undef WIFI_PASSWORD
    #define WIFI_SSID     "Wokwi-GUEST"
    #define WIFI_PASSWORD ""
    #define WIFI_CHANNEL  6
#else
    #define WIFI_CHANNEL  0
#endif

// ---------------------------------------------------------------------------
// Relays wired to this board. A message is only acted on when its "gpio" is
// listed here, so nothing published to the channel can drive other pins
// (flash pins, boot straps, UART).
// Most relay modules switch on when their input is pulled LOW; set activeLow
// to false for modules that switch on HIGH.
// ---------------------------------------------------------------------------
struct Relay {
    uint8_t gpio;
    bool activeLow;
};

#if defined(ESP32)
// 8-channel relay board, CH1..CH8. All of these pins are free of boot-strap
// and flash duties on an ESP32-WROOM.
const Relay RELAYS[] = {
    {23, true},
    {22, true},
    {21, true},
    {19, true},
    {18, true},
    {25, true},
    {26, true},
    {27, true},
};
#else
// NodeMCU labels: D1 = 5, D2 = 4, D5 = 14, D6 = 12
const Relay RELAYS[] = {
    {5, true},
    {4, true},
    {14, true},
    {12, true},
};
#endif

const size_t RELAY_COUNT = sizeof(RELAYS) / sizeof(RELAYS[0]);

// Ably's MQTT adapter: TLS only on 8883, MQTT 3.1.1, keep-alive 15-60 s,
// QoS 0 subscriptions, no retained messages.
const char *MQTT_HOST = "main.mqtt.ably.net";
const uint16_t MQTT_PORT = 8883;
const uint16_t MQTT_KEEP_ALIVE_SECONDS = 30;

// user/<userId>/room/<roomId>, built from the stored settings in setup().
char roomTopic[160];

// Ably has no retained messages. Rewinding one message replays the last
// state published to the channel, so the relays catch up after a reboot
// (and after the short MQTT pause of an ESP8266 update check).
char roomSubscription[176];

const unsigned long MQTT_RETRY_MIN_MS = 2000;
const unsigned long MQTT_RETRY_MAX_MS = 60000;

#if defined(ESP32)
WiFiClientSecure tlsClient;
#else
BearSSL::WiFiClientSecure tlsClient;
BearSSL::X509List trustAnchors(ABLY_ROOT_CA);
#endif

PubSubClient mqtt(tlsClient);

ShSettings::Settings settings;
char clientId[32];
bool clockSynced = false;
unsigned long lastMqttAttempt = 0;
unsigned long mqttRetryDelay = MQTT_RETRY_MIN_MS;
bool wifiWasConnected = false;
bool updateCheckScheduled = false;   // false = check as soon as online
unsigned long nextUpdateCheckAt = 0;

const Relay *findRelay(int gpio) {
    for (size_t i = 0; i < RELAY_COUNT; i++) {
        if (RELAYS[i].gpio == gpio) {
            return &RELAYS[i];
        }
    }

    return nullptr;
}

void setRelay(const Relay &relay, bool on) {
    digitalWrite(relay.gpio, (on != relay.activeLow) ? HIGH : LOW);
}

void onMessage(char *topic, byte *payload, unsigned int length) {

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("[MQTT] Ignoring malformed message on %s: %s\n", topic, error.c_str());
        return;
    }

    JsonVariant gpio = doc["gpio"];
    const char *value = doc["value"];

    if (!gpio.is<int>() || value == nullptr) {
        Serial.printf("[MQTT] Ignoring message on %s without gpio/value\n", topic);
        return;
    }

    bool on;
    if (strcasecmp(value, "on") == 0) {
        on = true;
    } else if (strcasecmp(value, "off") == 0) {
        on = false;
    } else {
        Serial.printf("[MQTT] Ignoring unknown value \"%s\"\n", value);
        return;
    }

    const Relay *relay = findRelay(gpio.as<int>());
    if (relay == nullptr) {
        Serial.printf("[RELAY] GPIO %d is not a relay on this board; add it to RELAYS\n", gpio.as<int>());
        return;
    }

    setRelay(*relay, on);
    Serial.printf("[RELAY] GPIO %d -> %s\n", relay->gpio, on ? "ON" : "OFF");
}

// Certificate validation needs the real date. Waits up to 15 s for NTP.
bool syncClock() {

    configTime(0, 0, "pool.ntp.org", "time.google.com");

    const time_t validAfter = 1700000000; // Nov 2023
    unsigned long start = millis();

    while (time(nullptr) < validAfter) {
        if (millis() - start > 15000) {
            return false;
        }
        delay(250);
    }

    Serial.println("[TIME] Clock synced");
    return true;
}

void connectMqtt() {

    Serial.printf("[MQTT] Connecting to %s:%u as %s\n", MQTT_HOST, MQTT_PORT, clientId);

    if (!mqtt.connect(clientId, settings.ablyKeyName, settings.ablyKeySecret)) {
        mqttRetryDelay = min(mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);

        // -2 is a TCP/TLS failure; 4 and 5 mean the key was rejected.
        Serial.printf("[MQTT] Connect failed (state %d), retrying in %lus\n",
                      mqtt.state(), mqttRetryDelay / 1000);
        return;
    }

    mqttRetryDelay = MQTT_RETRY_MIN_MS;

    if (mqtt.subscribe(roomSubscription, 0)) {
        Serial.printf("[MQTT] Connected, listening on %s\n", roomTopic);
        ShOtaRollback::markHealthy();
    } else {
        Serial.println("[MQTT] Subscribe failed, reconnecting");
        mqtt.disconnect();
    }
}

void scheduleUpdateCheck(unsigned long delayMs) {
    // Up to 2 minutes of jitter so a fleet does not hit GitHub in lockstep.
    nextUpdateCheckAt = millis() + delayMs + (unsigned long)random(120000);
    updateCheckScheduled = true;
}

// Looks for a newer release for this board and installs it (then reboots).
void checkForUpdates() {
#if defined(SH_OTA_DISABLED)
    updateCheckScheduled = true;
    nextUpdateCheckAt = millis() + 0x7FFFFFFFUL;
#else
#if defined(ESP8266)
    // Not enough RAM for a second TLS session next to MQTT. The relays hold
    // their state, and the rewind on resubscribe replays the last command.
    if (mqtt.connected()) {
        mqtt.disconnect();
        tlsClient.stop();
    }
#endif

    Serial.printf("[OTA] Checking %s\n", SH_OTA_MANIFEST_URL);
    JsonDocument manifest;
    String error;
    if (!ShOta::fetchManifest(SH_OTA_MANIFEST_URL, manifest, SH_FIRMWARE_TARGET, error)) {
        Serial.printf("[OTA] Update check failed: %s\n", error.c_str());
        scheduleUpdateCheck(SH_OTA_RETRY_MINUTES * 60000UL);
        return;
    }
    // Reaching the release server proves this image can take the next update.
    ShOtaRollback::markHealthy();
    scheduleUpdateCheck(SH_OTA_CHECK_INTERVAL_MINUTES * 60000UL);

    ShOta::Release release;
    if (!ShOta::findRelease(manifest, SH_FIRMWARE_TARGET, release)) {
        Serial.printf("[OTA] Release %s has no image for %s\n",
                      manifest["version"] | "?", SH_FIRMWARE_TARGET);
        return;
    }
    if (!ShFirmware::isNewer(release.version, SH_FIRMWARE_VERSION)) {
        Serial.printf("[OTA] Up to date (%s, latest %s)\n", SH_FIRMWARE_VERSION, release.version);
        return;
    }
    if (ShOtaRollback::isBad(release.version)) {
        Serial.printf("[OTA] Skipping %s: it was rolled back before\n", release.version);
        return;
    }

    Serial.printf("[OTA] Updating %s -> %s\n", SH_FIRMWARE_VERSION, release.version);
    if (mqtt.connected()) {
        mqtt.disconnect();
        tlsClient.stop();
    }
    String installError;
    if (!ShOta::installFromUrl(release, installError)) {
        Serial.printf("[OTA] Update failed: %s\n", installError.c_str());
        scheduleUpdateCheck(SH_OTA_RETRY_MINUTES * 60000UL);
        return;
    }
    ShOtaRollback::noteInstalled(release.version);
    Serial.println("[OTA] Update installed; rebooting");
    delay(500);
    ESP.restart();
#endif
}

void setup() {

    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("SmartHome " SH_BOARD_NAME " Room Node Starting");
    Serial.printf("[OTA] Firmware %s (%s)\n", SH_FIRMWARE_VERSION, SH_FIRMWARE_TARGET);
    ShOtaRollback::begin(SH_FIRMWARE_VERSION);

    const ShSettings::Source source = ShSettings::load(
        ShSettings::fromBuild(WIFI_SSID, WIFI_PASSWORD, ABLY_KEY_NAME, ABLY_KEY_SECRET,
                              SH_USER_ID, SH_ROOM_ID),
        settings);
    Serial.printf("[SETTINGS] Credentials from %s\n", ShSettings::sourceName(source));
    if (source == ShSettings::Source::Missing) {
        Serial.println("[SETTINGS] Flash once over USB with include/secrets.h to provision this board");
    }
    snprintf(roomTopic, sizeof(roomTopic), "user/%s/room/%s", settings.userId, settings.roomId);
    snprintf(roomSubscription, sizeof(roomSubscription), "[?rewind=1]%s", roomTopic);

    // Every relay starts off until the channel says otherwise.
    for (size_t i = 0; i < RELAY_COUNT; i++) {
        pinMode(RELAYS[i].gpio, OUTPUT);
        setRelay(RELAYS[i], false);
    }

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(clientId, sizeof(clientId), "room-node-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#if defined(ESP32)
    tlsClient.setCACert(ABLY_ROOT_CA);
#else
    tlsClient.setTrustAnchors(&trustAnchors);
#endif

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setKeepAlive(MQTT_KEEP_ALIVE_SECONDS);
    mqtt.setBufferSize(512);
    mqtt.setCallback(onMessage);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.wifiSsid, settings.wifiPassword, WIFI_CHANNEL);

    Serial.printf("[WIFI] Connecting to %s\n", settings.wifiSsid);
}

void loop() {

    ShOtaRollback::loop(WiFi.status() == WL_CONNECTED);

    if (WiFi.status() != WL_CONNECTED) {
        if (wifiWasConnected) {
            Serial.println("[WIFI] Disconnected, waiting for reconnect");
            wifiWasConnected = false;
        }

        delay(100);
        return;
    }

    if (!wifiWasConnected) {
        Serial.print("[WIFI] Connected, IP ");
        Serial.println(WiFi.localIP());
        wifiWasConnected = true;
    }

    if (!clockSynced) {
        clockSynced = syncClock();
        if (!clockSynced) {
            Serial.println("[TIME] NTP sync timed out, retrying");
            return;
        }
    }

    // Independent of MQTT, so a release that cannot reach Ably can still be
    // replaced by the next one.
    if (!updateCheckScheduled || (long)(millis() - nextUpdateCheckAt) >= 0) {
        checkForUpdates();
    }

    if (!mqtt.connected()) {
        if (lastMqttAttempt == 0 || millis() - lastMqttAttempt >= mqttRetryDelay) {
            lastMqttAttempt = millis();
            connectMqtt();
        }

        delay(10);
        return;
    }

    mqtt.loop();
    delay(10);
}
