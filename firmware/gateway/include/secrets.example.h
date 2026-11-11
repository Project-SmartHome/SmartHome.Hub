#pragma once

// Copy to secrets.h and configure a dedicated gateway key.
// Used for the first USB flash: the values are then saved to the gateway's
// flash and kept by OTA updates (which are built without secrets.h). To
// change them later, edit secrets.h and flash over USB again.
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// The user who owns this gateway. Registration requests are published to
// smarthome/<SH_USER_ID>/device-registration/requests and acknowledgements
// are read from smarthome/<SH_USER_ID>/device-registration/acks/<deviceId>.
// Must not contain '/', ':', '+', '#', '*', '[', ']' or spaces.
#define SH_USER_ID "your-user-id"

// Use an Ably key permitted to publish to
// smarthome/<SH_USER_ID>/device-registration/requests and subscribe to
// smarthome/<SH_USER_ID>/device-registration/acks/*.
#define ABLY_KEY_NAME   "appId.keyId"
#define ABLY_KEY_SECRET "replace-with-key-secret"