#pragma once

// Rollback protection for ESP32 OTA updates.
//
// The Arduino-ESP32 bootloader is built with app rollback enabled. After an
// OTA install the new image boots in PENDING_VERIFY state; if it resets
// before being marked valid, the bootloader returns to the previous image.
//
// Usage (ESP32 firmware):
//   1. In exactly one .cpp:  extern "C" bool verifyRollbackLater() { return true; }
//      so the core leaves the decision to the application.
//   2. setup():  ShOtaRollback::begin(SH_FIRMWARE_VERSION);
//   3. loop():   ShOtaRollback::loop(wifiUp);  // enforces the health timeout
//   4. Once the device proves it works (cloud/gateway reachable):
//                ShOtaRollback::markHealthy();
//   5. Before rebooting into a new image: ShOtaRollback::noteInstalled(version);
//
// A version that gets rolled back is remembered and skipped (isBad), so a
// broken release does not install/rollback in a loop. The next newer release
// is installed normally.
//
// ESP8266 has no bootloader rollback; every call is a no-op there.

#include <Arduino.h>
#include "OtaConfig.h"

#if defined(ESP32)
    #include <Preferences.h>
    #include <esp_ota_ops.h>
#endif

namespace ShOtaRollback {

#if defined(ESP32)

namespace detail {
constexpr const char* NAMESPACE = "sh-ota";

// Preferences logs an error for every missing key, which is the normal
// state here (no update has happened yet), so look before reading.
inline String readKey(Preferences& prefs, const char* key) {
    return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

inline void removeKey(Preferences& prefs, const char* key) {
    if (prefs.isKey(key)) prefs.remove(key);
}

inline char* badVersion() {
    static char version[16] = {};
    return version;
}

inline bool& pendingVerify() {
    static bool pending = false;
    return pending;
}
} // namespace detail

inline bool imagePendingVerify() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    return running != nullptr &&
           esp_ota_get_state_partition(running, &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

inline void begin(const char* runningVersion) {
    Preferences prefs;
    if (!prefs.begin(detail::NAMESPACE, false)) return;

    const String attempted = detail::readKey(prefs, "try");
    if (attempted.length() > 0 && attempted != runningVersion) {
        // We installed `attempted` but booted something else: the bootloader
        // rolled it back (or the install never activated).
        prefs.putString("bad", attempted);
        detail::removeKey(prefs, "try");
        Serial.printf("[OTA] Version %s was rolled back; it will not be installed again\n",
                      attempted.c_str());
    }

    const String bad = detail::readKey(prefs, "bad");
    strncpy(detail::badVersion(), bad.c_str(), 15);
    detail::badVersion()[15] = '\0';
    prefs.end();

    detail::pendingVerify() = imagePendingVerify();
    if (detail::pendingVerify()) {
        Serial.printf("[OTA] Running new firmware %s on probation; it must come online within %u min\n",
                      runningVersion, static_cast<unsigned>(SH_OTA_HEALTH_TIMEOUT_MINUTES));
    }
}

inline void markHealthy() {
    if (!detail::pendingVerify()) return;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        detail::pendingVerify() = false;
        Preferences prefs;
        if (prefs.begin(detail::NAMESPACE, false)) {
            detail::removeKey(prefs, "try");
            prefs.end();
        }
        Serial.println("[OTA] New firmware confirmed healthy");
    }
}

// `environmentReady` says whether the device currently has what it needs to
// prove itself (e.g. Wi-Fi is associated). Probation time only accrues
// while it is true, so a router outage right after an update does not get
// a good release rolled back (the old image could not do better).
inline void loop(bool environmentReady = true) {
    if (!detail::pendingVerify()) return;

    static uint32_t lastTick = millis();
    static uint32_t creditedMs = 0;
    const uint32_t now = millis();
    if (environmentReady) creditedMs += now - lastTick;
    lastTick = now;

    if (creditedMs >= SH_OTA_HEALTH_TIMEOUT_MINUTES * 60UL * 1000UL) {
        Serial.println("[OTA] New firmware never came online; rolling back");
        delay(100);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

// Call after Update.end() succeeds and before ESP.restart().
inline void noteInstalled(const char* version) {
    Preferences prefs;
    if (!prefs.begin(detail::NAMESPACE, false)) return;
    prefs.putString("try", version);
    prefs.end();
}

inline bool isBad(const char* version) {
    return version != nullptr && detail::badVersion()[0] != '\0' &&
           strcmp(detail::badVersion(), version) == 0;
}

inline bool onProbation() {
    return detail::pendingVerify();
}

#else  // ESP8266: no bootloader rollback

inline void begin(const char*) {}
inline void markHealthy() {}
inline void loop(bool = true) {}
inline void noteInstalled(const char*) {}
inline bool isBad(const char*) { return false; }
inline bool onProbation() { return false; }

#endif

} // namespace ShOtaRollback
