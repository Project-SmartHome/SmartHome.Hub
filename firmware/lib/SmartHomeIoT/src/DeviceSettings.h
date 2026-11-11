#pragma once

// Credentials kept in flash so they survive OTA updates.
//
// Release binaries are built in CI and published publicly, so they never
// contain Wi-Fi passwords or Ably keys. Instead:
//   - A build that has secrets.h (a local USB flash) passes those values to
//     load(); they are written to flash if they differ from what is stored.
//   - A build without secrets.h (every OTA image) passes empty values and
//     keeps using what is already stored.
// So: flash each device once over USB with its secrets.h; from then on it
// can take any number of OTA updates and keep its configuration.
//
// Storage: ESP32 NVS (Preferences, namespace "sh-settings"); ESP8266 a
// checksummed block in the emulated EEPROM sector. Both live outside the
// application partitions, so an OTA update does not touch them.

#include <Arduino.h>
#include <string.h>

#if defined(ESP32)
    #include <Preferences.h>
#elif defined(ESP8266)
    #include <EEPROM.h>
#endif

namespace ShSettings {

struct Settings {
    char wifiSsid[33] = {};
    char wifiPassword[65] = {};
    char ablyKeyName[64] = {};
    char ablyKeySecret[96] = {};
    char userId[65] = {};
    char roomId[65] = {};
};

enum class Source : uint8_t {
    Build,     // secrets.h compiled into this firmware (and now saved)
    Storage,   // previously saved values
    Missing    // nothing compiled in and nothing saved
};

namespace detail {

inline void copy(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;
    strncpy(destination, source == nullptr ? "" : source, capacity - 1);
    destination[capacity - 1] = '\0';
}

inline bool same(const Settings& a, const Settings& b) {
    return strcmp(a.wifiSsid, b.wifiSsid) == 0 &&
           strcmp(a.wifiPassword, b.wifiPassword) == 0 &&
           strcmp(a.ablyKeyName, b.ablyKeyName) == 0 &&
           strcmp(a.ablyKeySecret, b.ablyKeySecret) == 0 &&
           strcmp(a.userId, b.userId) == 0 &&
           strcmp(a.roomId, b.roomId) == 0;
}

#if defined(ESP32)

constexpr const char* NAMESPACE = "sh-settings";

// Missing keys are normal (first boot); Preferences logs an error for each
// one it is asked for, so check first.
inline void readKey(Preferences& prefs, const char* key, char* out, size_t capacity) {
    out[0] = '\0';
    if (prefs.isKey(key)) prefs.getString(key, out, capacity);
}

inline bool read(Settings& out) {
    Preferences prefs;
    // Read-write so a first boot creates the namespace instead of logging
    // "nvs_open failed: NOT_FOUND".
    if (!prefs.begin(NAMESPACE, false)) return false;
    out = Settings{};
    readKey(prefs, "ssid", out.wifiSsid, sizeof(out.wifiSsid));
    readKey(prefs, "pass", out.wifiPassword, sizeof(out.wifiPassword));
    readKey(prefs, "ably_name", out.ablyKeyName, sizeof(out.ablyKeyName));
    readKey(prefs, "ably_secret", out.ablyKeySecret, sizeof(out.ablyKeySecret));
    readKey(prefs, "user", out.userId, sizeof(out.userId));
    readKey(prefs, "room", out.roomId, sizeof(out.roomId));
    prefs.end();
    return out.wifiSsid[0] != '\0';
}

inline bool write(const Settings& settings) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;
    bool ok = true;
    // putString returns the stored length; an empty value returns 0.
    ok &= prefs.putString("ssid", settings.wifiSsid) == strlen(settings.wifiSsid);
    ok &= prefs.putString("pass", settings.wifiPassword) == strlen(settings.wifiPassword);
    ok &= prefs.putString("ably_name", settings.ablyKeyName) == strlen(settings.ablyKeyName);
    ok &= prefs.putString("ably_secret", settings.ablyKeySecret) == strlen(settings.ablyKeySecret);
    ok &= prefs.putString("user", settings.userId) == strlen(settings.userId);
    ok &= prefs.putString("room", settings.roomId) == strlen(settings.roomId);
    prefs.end();
    return ok;
}

#elif defined(ESP8266)

constexpr uint32_t MAGIC = 0x53485354;  // "SHST"
constexpr uint16_t LAYOUT = 1;
constexpr size_t EEPROM_BYTES = 512;

struct Stored {
    uint32_t magic;
    uint16_t layout;
    Settings settings;
    uint32_t checksum;
};
static_assert(sizeof(Stored) <= EEPROM_BYTES, "Settings do not fit in EEPROM block");

inline uint32_t checksum(const Settings& settings) {
    // FNV-1a over the raw settings bytes.
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&settings);
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < sizeof(settings); ++i) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

inline bool read(Settings& out) {
    Stored stored{};
    EEPROM.begin(EEPROM_BYTES);
    EEPROM.get(0, stored);
    EEPROM.end();
    if (stored.magic != MAGIC || stored.layout != LAYOUT ||
        stored.checksum != checksum(stored.settings)) {
        return false;
    }
    out = stored.settings;
    return out.wifiSsid[0] != '\0';
}

inline bool write(const Settings& settings) {
    Stored stored{};
    stored.magic = MAGIC;
    stored.layout = LAYOUT;
    stored.settings = settings;
    stored.checksum = checksum(settings);
    EEPROM.begin(EEPROM_BYTES);
    EEPROM.put(0, stored);
    const bool ok = EEPROM.commit();
    EEPROM.end();
    return ok;
}

#endif

} // namespace detail

// Builds a Settings from compile-time values (pass "" for anything unused).
inline Settings fromBuild(const char* ssid, const char* password,
                          const char* ablyKeyName, const char* ablyKeySecret,
                          const char* userId, const char* roomId) {
    Settings settings;
    detail::copy(settings.wifiSsid, sizeof(settings.wifiSsid), ssid);
    detail::copy(settings.wifiPassword, sizeof(settings.wifiPassword), password);
    detail::copy(settings.ablyKeyName, sizeof(settings.ablyKeyName), ablyKeyName);
    detail::copy(settings.ablyKeySecret, sizeof(settings.ablyKeySecret), ablyKeySecret);
    detail::copy(settings.userId, sizeof(settings.userId), userId);
    detail::copy(settings.roomId, sizeof(settings.roomId), roomId);
    return settings;
}

// Resolves the settings to use. `compiled` wins when it carries a Wi-Fi SSID
// (and is persisted if it changed); otherwise the stored values are used.
inline Source load(const Settings& compiled, Settings& out) {
    Settings stored;
    const bool haveStored = detail::read(stored);

    if (compiled.wifiSsid[0] != '\0') {
        out = compiled;
        if (!haveStored || !detail::same(stored, compiled)) {
            if (detail::write(compiled)) {
                Serial.println("[Settings] Saved credentials from this build to flash");
            } else {
                Serial.println("[Settings] Could not save credentials; OTA images will not have them");
            }
        }
        return Source::Build;
    }

    if (haveStored) {
        out = stored;
        return Source::Storage;
    }

    out = Settings{};
    return Source::Missing;
}

inline const char* sourceName(Source source) {
    switch (source) {
        case Source::Build: return "this build (secrets.h)";
        case Source::Storage: return "flash";
        case Source::Missing: return "nowhere";
    }
    return "?";
}

} // namespace ShSettings
