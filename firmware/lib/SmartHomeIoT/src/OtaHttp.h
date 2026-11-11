#pragma once

// HTTPS side of OTA: fetch the release manifest and install an image from a
// URL, for ESP32 and ESP8266. Used by the gateway (for itself, and to
// download node images it relays over ESP-NOW) and by room nodes.
//
// The release manifest (firmware/scripts/package_firmware.py) looks like:
//   {
//     "schema": 1, "version": "1.4.2",
//     "targets": {
//       "gateway-esp32dev": {"version": "1.4.2", "url": "https://...bin",
//                            "size": 912345, "md5": "<32 hex>", "sha256": "..."},
//       ...
//     }
//   }
//
// Requires a valid wall clock (NTP) for certificate validation.

#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP32)
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #include <HTTPClient.h>
    #include <Update.h>
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <WiFiClientSecureBearSSL.h>
    #include <ESP8266HTTPClient.h>
    #include <Updater.h>
#else
    #error "OTA requires an ESP32 or ESP8266 target"
#endif

#include "FirmwareVersion.h"
#include "OtaConfig.h"
#include "OtaProtocol.h"
#include "OtaRootCA.h"

namespace ShOta {

constexpr uint8_t MAX_REDIRECTS = 5;
constexpr uint16_t HTTP_TIMEOUT_MS = 15000;
constexpr int MAX_MANIFEST_BYTES = 16384;

struct Release {
    char target[SH_OTA_TARGET_LENGTH] = {};
    char version[SH_OTA_VERSION_LENGTH] = {};
    char url[256] = {};
    uint32_t size = 0;
    char md5[SH_OTA_MD5_LENGTH] = {};

    bool valid() const {
        return target[0] != '\0' && url[0] != '\0' && size > 0 &&
               ShFirmware::parseVersion(version).valid && ShOtaProtocol::isMd5Hex(md5);
    }
};

// TLS client trusting only the roots in OtaRootCA.h.
class SecureClient {
public:
    SecureClient() {
#if defined(ESP32)
        client.setCACert(SH_OTA_ROOT_CA);
#else
        client.setTrustAnchors(&anchors);
#endif
    }

#if defined(ESP32)
    WiFiClientSecure client;
#else
    BearSSL::X509List anchors{SH_OTA_ROOT_CA};
    BearSSL::WiFiClientSecure client;
#endif
};

// GETs `url`, following redirects by hand. (Arduino-ESP32 2.x reuses the
// open connection when it follows a redirect itself, which breaks GitHub's
// github.com -> release-assets.githubusercontent.com hop.) On success the
// response body is ready to read from `http`; the caller must http.end().
inline bool openUrl(HTTPClient& http, WiFiClient& client, const char* url, int& status) {
    static const char* HEADERS[] = {"Location"};
    String current = url;
    status = 0;

    for (uint8_t hop = 0; hop <= MAX_REDIRECTS; ++hop) {
        http.end();
        client.stop();
        if (!http.begin(client, current)) {
            status = -100;
            return false;
        }
        http.setReuse(false);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.setUserAgent("SmartHome-OTA/" SH_FIRMWARE_VERSION " (" SH_FIRMWARE_TARGET ")");
        http.collectHeaders(HEADERS, 1);

        status = http.GET();
        if (status == HTTP_CODE_OK) return true;

        const bool redirect = status == 301 || status == 302 || status == 303 ||
                              status == 307 || status == 308;
        if (!redirect) return false;

        String location = http.header("Location");
        if (location.length() == 0) return false;
        if (location.startsWith("/")) {
            // Relative redirect: keep scheme and host of the current URL.
            const int hostEnd = current.indexOf('/', current.indexOf("//") + 2);
            location = (hostEnd > 0 ? current.substring(0, hostEnd) : current) + location;
        }
        if (!location.startsWith("https://")) {
            status = -101;  // never downgrade to plain HTTP
            return false;
        }
        current = location;
    }

    status = -102;  // too many redirects
    http.end();
    return false;
}

// Downloads and parses the manifest. With `onlyTarget` set, only that
// target's entry is kept (saves RAM on ESP8266).
inline bool fetchManifest(const char* url, JsonDocument& manifest,
                          const char* onlyTarget, String& error) {
    SecureClient tls;
    HTTPClient http;
    int status = 0;
    if (!openUrl(http, tls.client, url, status)) {
        error = status == 404 ? "no firmware manifest in the latest release (404)"
                              : "HTTP " + String(status);
        http.end();
        return false;
    }

    const int size = http.getSize();
    if (size > MAX_MANIFEST_BYTES) {
        error = "manifest too large";
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();

    DeserializationError parseError;
    if (onlyTarget != nullptr) {
        JsonDocument filter;
        filter["version"] = true;
        filter["targets"][onlyTarget] = true;
        parseError = deserializeJson(manifest, body, DeserializationOption::Filter(filter));
    } else {
        parseError = deserializeJson(manifest, body);
    }
    if (parseError) {
        error = String("manifest JSON: ") + parseError.c_str();
        return false;
    }
    if (!manifest["targets"].is<JsonObjectConst>()) {
        error = "manifest has no targets";
        return false;
    }
    return true;
}

// Reads one manifest entry. Returns false if it is missing or malformed.
inline bool readRelease(const char* target, JsonVariantConst entry, Release& out) {
    out = Release{};
    if (target == nullptr || !entry.is<JsonObjectConst>()) return false;

    const char* version = entry["version"] | "";
    const char* url = entry["url"] | "";
    const char* md5 = entry["md5"] | "";
    const uint32_t size = entry["size"] | 0;

    if (strlen(target) >= sizeof(out.target) || strlen(version) >= sizeof(out.version) ||
        strlen(url) >= sizeof(out.url) || strlen(md5) >= sizeof(out.md5)) {
        return false;
    }
    strcpy(out.target, target);
    strcpy(out.version, version);
    strcpy(out.url, url);
    strcpy(out.md5, md5);
    for (char& c : out.md5) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    out.size = size;
    return out.valid();
}

// Convenience: looks up `target` in a parsed manifest.
inline bool findRelease(const JsonDocument& manifest, const char* target, Release& out) {
    return readRelease(target, manifest["targets"][target], out);
}

inline String updaterError() {
#if defined(ESP32)
    return Update.errorString();
#else
    return Update.getErrorString();
#endif
}

inline void abortUpdate() {
#if defined(ESP32)
    Update.abort();
#else
    // end() on an unfinished image discards it without activating anything.
    Update.end(false);
#endif
}

// Streams the image at release.url into the inactive OTA slot and verifies
// size and MD5. On success the new image boots on the next restart; the
// caller decides when to restart.
inline bool installFromUrl(const Release& release, String& error) {
    if (!release.valid()) {
        error = "invalid release entry";
        return false;
    }

    SecureClient tls;
    HTTPClient http;
    int status = 0;
    if (!openUrl(http, tls.client, release.url, status)) {
        error = "download failed: HTTP " + String(status);
        http.end();
        return false;
    }

    const int length = http.getSize();
    if (length <= 0 || static_cast<uint32_t>(length) != release.size) {
        error = "size mismatch: server " + String(length) + ", manifest " + String(release.size);
        http.end();
        return false;
    }

    if (!Update.begin(static_cast<size_t>(length))) {
        error = "cannot start update: " + updaterError();
        http.end();
        return false;
    }
    Update.setMD5(release.md5);

    static uint8_t lastDecile;
    lastDecile = 0;
    Update.onProgress([](size_t done, size_t total) {
        const uint8_t decile = total == 0 ? 0 : static_cast<uint8_t>(done * 10 / total);
        if (decile != lastDecile) {
            lastDecile = decile;
            Serial.printf("[OTA] %u%%\n", decile * 10);
        }
    });

    const size_t written = Update.writeStream(*http.getStreamPtr());
    http.end();
    Update.onProgress(nullptr);

    if (written != static_cast<size_t>(length)) {
        error = "stream ended at " + String(written) + "/" + String(length) + " bytes: " + updaterError();
        abortUpdate();
        return false;
    }
    if (!Update.end()) {
        error = "verification failed: " + updaterError();
        return false;
    }
    return true;
}

} // namespace ShOta
