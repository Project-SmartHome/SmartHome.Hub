#pragma once

// Build-time identity and OTA settings.
//
// firmware/scripts/firmware_version.py injects SH_FIRMWARE_VERSION and
// SH_FIRMWARE_TARGET into every PlatformIO build (CI passes the release
// version; local builds get "0.0.0+dev"). The defaults below only apply to
// builds that bypass that script.

#ifndef SH_FIRMWARE_VERSION
#define SH_FIRMWARE_VERSION "0.0.0+dev"
#endif

// "<firmware>-<pio env>", e.g. "gateway-esp32dev" or "node-nodemcuv2". The
// release manifest is keyed by it, so a device only installs images built
// for its own board.
#ifndef SH_FIRMWARE_TARGET
#define SH_FIRMWARE_TARGET "unknown"
#endif

// Latest release's manifest. The "latest/download" URL always redirects to
// the newest published (non-draft, non-prerelease) GitHub Release.
#ifndef SH_OTA_MANIFEST_URL
#define SH_OTA_MANIFEST_URL \
    "https://github.com/Project-SmartHome/SmartHome.Hub/releases/latest/download/firmware-manifest.json"
#endif

// How often internet-connected devices look for a new release. ESP8266 room
// nodes must drop their MQTT session for each check (not enough RAM for two
// TLS sessions), so they check less often.
#ifndef SH_OTA_CHECK_INTERVAL_MINUTES
    #if defined(ESP8266)
        #define SH_OTA_CHECK_INTERVAL_MINUTES 60
    #else
        #define SH_OTA_CHECK_INTERVAL_MINUTES 15
    #endif
#endif

// Retry delay after a failed manifest download or install.
#ifndef SH_OTA_RETRY_MINUTES
#define SH_OTA_RETRY_MINUTES 5
#endif

// ESP32 only: a freshly installed image must prove itself healthy (reach the
// network/gateway) within this time, or the bootloader rolls back to the
// previous image.
#ifndef SH_OTA_HEALTH_TIMEOUT_MINUTES
#define SH_OTA_HEALTH_TIMEOUT_MINUTES 10
#endif

// Define SH_OTA_DISABLED (e.g. -DSH_OTA_DISABLED in build_flags) to build a
// firmware that never checks for or accepts updates.
