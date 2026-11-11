# Over-the-air firmware updates

Every release published by the **Release** workflow carries firmware for every
board. Devices install it automatically:

| Device | How it updates | Check interval |
|---|---|---|
| Gateway (ESP32) | Downloads its image from GitHub over HTTPS | 15 min |
| Room node (ESP32) | Downloads its image from GitHub over HTTPS | 15 min |
| Room node (ESP8266/NodeMCU) | Same, but pauses MQTT for each check (RAM) | 60 min |
| ESP-NOW node (ESP32/ESP8266) | Gateway downloads the image and streams it over ESP-NOW | when the gateway sees it |

Each check adds up to 2 minutes of random jitter, so a fleet does not hit GitHub all at once.

## Releasing

1. Merge to `main`.
2. Actions → **Release** → *Run workflow* (choose patch/minor/major).

The workflow computes the next `vX.Y.Z`, builds all firmware with that version
stamped in (`firmware.yml`), then creates the GitHub Release with these assets:

```
gateway-esp32dev.bin   node-esp32dev.bin   node-nodemcuv2.bin   node-esp8266.bin
room-node-esp32dev.bin room-node-nodemcuv2.bin room-node-esp8266.bin
firmware-manifest.json
```

Devices fetch
`https://github.com/<repo>/releases/latest/download/firmware-manifest.json`,
look up their own target (`<firmware>-<pio env>`), and install it if its version
is newer than theirs. Only the newest published release counts: drafts and
pre-releases are ignored.

## One-time setup per device

OTA images are built in CI and published publicly, so they contain **no Wi-Fi
or Ably credentials**. Each device needs one USB flash with its `secrets.h`:

| Firmware | Secrets file |
|---|---|
| gateway | `gateway/include/secrets.h` (from `secrets.example.h`) |
| room-nodes | `room-nodes/include/secrets.h` (from `secrets.example.h`) |
| node | none (ESP-NOW only) |

On boot the firmware saves those values to flash (ESP32 NVS / ESP8266 EEPROM
sector). Neither area is touched by an OTA update. Later OTA images, which have no
`secrets.h`, use the saved values. To change credentials, edit `secrets.h` and
flash over USB again.

Devices running firmware from before OTA support also need this one USB flash.
After that, every update is automatic.

## Versions

- Release builds: `X.Y.Z`, from the release workflow (`SH_FIRMWARE_VERSION`).
- Every local build: `0.0.0+dev`, which is older than any release, so a
  USB-flashed device moves to the latest release at its first check.
- To keep a local build on a device (development), build with OTA turned off:
  ```bash
  PLATFORMIO_BUILD_FLAGS=-DSH_OTA_DISABLED pio run -e esp32dev -t upload
  ```
  The Wokwi environments always have OTA turned off.

Only `MAJOR.MINOR.PATCH` is compared, and a device never downgrades.

## Safety

- **Integrity:** images are downloaded over TLS, trusting only the roots in
  `lib/SmartHomeIoT/src/OtaRootCA.h`, and checked against the manifest's
  size and MD5 before they are activated. On the ESP-NOW hop, the node checks
  the MD5 again.
- **Right image:** a device only installs its own target, so an ESP32 image
  can never be flashed onto an ESP8266. A node only accepts offers from the
  gateway it registered with.
- **Rollback (ESP32):** a new image is on probation until it proves it can
  reach the network. For the gateway that means the release server or Ably,
  for a room node the release server or its room channel, and for an ESP-NOW
  node its gateway. A crash or reset during probation boots the previous image.
  So does staying offline for 10 min while Wi-Fi is up. A rolled-back version is
  remembered and never retried; the next release installs normally. ESP8266
  has no bootloader rollback.
- Credentials and registration state survive updates.

## ESP-NOW relay details

- Nodes report `firmwareVersion` plus their target (frame payload) in
  heartbeats and registration requests.
- The gateway caches one node image at a time in LittleFS, using the `spiffs`
  data partition (about 1.4 MB). The partition is formatted on first use.
- Transfers are stop-and-wait with 200-byte chunks, resent after 250 ms without
  an ack. Expect roughly 1–3 minutes for a 290 KB ESP8266 node and longer for
  ESP32 nodes. Registration traffic pauses during the transfer.
- One node is updated at a time. After a failure, retries back off per node,
  from 2 min up to about 2 h. Transfers continue during Wi-Fi/Ably outages;
  only image downloads need internet.

Protocol: `lib/SmartHomeIoT/src/OtaProtocol.h`.

## Settings (build flags)

| Flag | Default |
|---|---|
| `SH_OTA_CHECK_INTERVAL_MINUTES` | 15 (ESP32), 60 (ESP8266) |
| `SH_OTA_RETRY_MINUTES` | 5 |
| `SH_OTA_HEALTH_TIMEOUT_MINUTES` | 10 |
| `SH_OTA_MANIFEST_URL` | latest release of `Project-SmartHome/SmartHome.Hub` (CI sets it from the repo) |
| `SH_OTA_DISABLED` | not set |

## Limits and follow-ups

- **ESP-NOW frames are not authenticated.** Anyone within radio range who
  spoofs the gateway's MAC could push firmware to a node (the MD5 only
  catches corruption). The existing registration frames have the same
  weakness. Signing images (for example Ed25519, verified on the node) or
  ESP-NOW encryption with a provisioned key would close this.
- ESP8266 boards install OTA into free sketch space, so an image must stay
  under about half of the 1 MB sketch area (currently 290–440 KB).
- If GitHub changes to a CA that is not in `OtaRootCA.h`, devices can no longer
  update and need a USB flash. Refresh that list in a release before then.

## Testing

Before cutting a release, flash one device of each type over USB, then watch the
serial log (115200 baud) after the release goes live:

```
[OTA] Checking https://github.com/.../firmware-manifest.json
[OTA] Updating gateway 0.0.0+dev -> 1.4.2
[OTA] Gateway update installed; rebooting
[OTA] Running new firmware 1.4.2 on probation; ...
[OTA] New firmware confirmed healthy
[NodeOTA] Offering node-nodemcuv2 1.4.2 (287696 bytes) to sh-246f28010203 ...
```
