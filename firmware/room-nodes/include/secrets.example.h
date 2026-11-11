// Copy this file to secrets.h (git-ignored) and fill it in for each board.
//
// These values are compiled in only for the first USB flash. On boot they
// are saved to the board's flash, and OTA updates (which are built without
// secrets.h) keep using the saved values. To change them later, edit
// secrets.h and flash over USB again.
#pragma once

#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Ably API key "appId.keyId:keySecret", split at the colon.
// Give the board its own key that can only subscribe to this room's channel
// (capability: {"user/<userId>/room/<roomId>": ["subscribe"]}), so a leaked
// key cannot switch anything.
#define ABLY_KEY_NAME   "appId.keyId"
#define ABLY_KEY_SECRET "replace-with-key-secret"

// The relays listen on user/<SH_USER_ID>/room/<SH_ROOM_ID>.
#define SH_USER_ID "your-user-id"
#define SH_ROOM_ID "your-room-id"
