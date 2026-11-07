# 3. ESP-NOW Network Design

## 3.1 Selected communication model

- Node-to-gateway: ESP-NOW.
- Gateway-to-hub: USB serial (UART over USB bridge or native USB, depending on board).
- Hub-to-cloud: outbound IP connectivity, optional.
- Nodes do not associate with the household Wi-Fi access point.

**Terminology:** ESP-NOW uses the ESP32 Wi-Fi radio and 802.11 action frames. Therefore “no Wi-Fi” in this design means no household Wi-Fi association or internet access for nodes; it does not mean the radio technology is unrelated to Wi-Fi.

## 3.2 Topology

Use a star topology:

```text
          Gateway
        /    |    \
      N1     N2    N3 ... N15
```

Every node must be within a tested reliable direct link to a gateway. Do not assume nodes relay packets for one another. A relay/multi-hop scheme would be a separate protocol and reliability project.

## 3.3 Radio channel

- Select a fixed channel during commissioning.
- Configure gateway and nodes to the same channel.
- The gateway must not silently roam between channels.
- Record channel, country/regulatory configuration, board type, and firmware version.
- Test interference from nearby 2.4 GHz Wi-Fi and Bluetooth devices.
- If the gateway radio channel changes, define a controlled reconfiguration/recovery procedure.

If the gateway's ESP32 radio is configured in station mode, it can remain unassociated with an access point while using ESP-NOW. The Linux host's own Ethernet/Wi-Fi connection is independent.

## 3.4 Peer capacity and hardware qualification

ESP-NOW peer limits depend on chip family, ESP-IDF release, and configuration. Do not hard-code a universal peer count into the product requirements.

Before selecting hardware:
1. Identify the exact gateway chip and node chip variants.
2. Select an ESP-IDF release.
3. Check that release's ESP-NOW peer and encrypted-peer limits for that target.
4. Configure encrypted peer capacity for the intended deployment.
5. Verify 10–15 node behavior under simultaneous heartbeat and telemetry load.
6. Record tested maximums in a hardware compatibility matrix.

For an initial 10–15 node system, select a gateway/SDK configuration that supports the required peer count with headroom. If a chip's encrypted-peer limit is too low, choose another chip/configuration or explicitly design a secure application-layer session strategy; do not silently downgrade security.

## 3.5 Payload sizing

Keep the initial application message payload at or below 250 bytes for broad ESP-NOW v1 compatibility. Larger payload support varies by ESP-IDF version and target; do not rely on larger v2 frames until all selected devices are qualified.

Keep messages compact:
- Use numeric message and capability identifiers where appropriate.
- Avoid verbose JSON over the radio.
- Consider a compact binary encoding or CBOR with a strict schema.
- Fragmentation should be avoided initially; if later required, specify bounded reassembly, timeout, and integrity rules.

## 3.6 Discovery and onboarding

Discovery is not authentication.

1. Node boots and emits a small announcement containing protocol version, ephemeral boot/session identifier, model code, and supported onboarding method.
2. Gateway forwards the announcement.
3. Hub creates a pending candidate with rate limits and duplicate suppression.
4. User initiates pairing through the local UI or an explicit physical enrollment action is detected.
5. Hub and node perform an authenticated provisioning exchange.
6. A per-device key is provisioned or derived using the chosen secure onboarding method.
7. The hub binds the cryptographic identity to a stable logical device ID.
8. Subsequent control and telemetry are accepted only from authenticated devices.

Avoid using a MAC address as proof of identity. Broadcast announcements are unauthenticated unless separately protected; treat their content as untrusted.

## 3.7 Security options

Evaluate two implementation paths against the selected chip and ESP-IDF release:

- Native ESP-NOW encrypted unicast using per-peer Local Master Keys (LMKs), with a secure process for provisioning and protecting the PMK/LMKs.
- Espressif's maintained ESP-NOW component/provisioning approach where supported by the target, including its authenticated key exchange and proof-of-possession flow.

Choose one documented approach and test it end-to-end. Do not invent a custom cryptographic handshake. Never put the same static secret in every node firmware image. Use unique device credentials and protect provisioning secrets.

## 3.8 Reliability

- Use application-level ACKs; a radio send callback is not proof the application processed a command.
- Include message sequence numbers and duplicate suppression.
- Retry only bounded times with backoff and jitter.
- Commands have unique IDs and expiry times.
- Make actuator commands idempotent where possible (e.g. “set relay OFF” rather than “toggle relay”).
- Heartbeats update last-seen status.
- Telemetry may be coalesced or dropped under congestion according to per-entity policy; command results must not be silently discarded.
- Report retry counts, RSSI where available, send failures, queue depth, and gateway health.

## 3.9 RF qualification

Test at actual mounting locations:
- Line-of-sight and through-wall performance.
- Worst-case distance and obstacles.
- Interference at different times of day.
- Simultaneous telemetry from all nodes.
- Gateway reboot and node reboot.
- Packet loss, retry rate, and command completion latency.
- Antenna orientation and enclosure effects.

Set acceptance thresholds after measurement rather than assuming a universal range.
