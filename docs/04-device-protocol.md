# 4. Device Protocol Specification (Initial Draft)

## 4.1 Design principles

- Versioned and compact.
- Strictly validated at every boundary.
- Authenticated after onboarding.
- Replay-resistant.
- Bounded message size and bounded queues.
- Commands have IDs, expiry, and explicit result messages.
- The protocol between Python and gateway is independent of the radio packet format.

The exact binary encoding is **TBD** after choosing ESP-IDF, chip family, and firmware implementation. Initial radio application payload target: no more than 250 bytes.

## 4.2 Logical message envelope

Every logical message should carry the equivalent of:

| Field | Purpose |
|---|---|
| protocol_version | Incompatible schema detection |
| message_type | Discovery, telemetry, command, ACK, etc. |
| sender_device_id | Stable provisioned identity where available |
| destination_id | Gateway or target device |
| boot_id | Distinguishes device restarts |
| sequence | Monotonic sequence within boot/session |
| message_id | Unique message/correlation identifier |
| timestamp_or_age | Optional; use monotonic age/expiry for command validity |
| payload_length | Bounds validation |
| payload | Message-specific data |
| authentication | MAC/AEAD tag or transport security mechanism |

Do not trust a sender-provided timestamp as the sole replay defense. Use authenticated sequence/session data and server-side replay windows.

## 4.3 Initial message types

| Message | Direction | Purpose |
|---|---|---|
| DISCOVER | Node → broadcast | Announces presence; untrusted |
| ANNOUNCE | Node → gateway | Model, protocol, capabilities summary |
| PAIR_START | Hub ↔ node via gateway | Begins secure onboarding |
| PAIR_CHALLENGE | Hub ↔ node | Authenticated provisioning challenge |
| PAIR_RESULT | Node → hub | Pairing completion/failure |
| HEARTBEAT | Node → hub | Liveness and health |
| TELEMETRY | Node → hub | Sensor/entity values |
| COMMAND | Hub → node | Requested state/action |
| COMMAND_ACK | Node → hub | Command accepted/rejected |
| COMMAND_RESULT | Node → hub | Execution result and resulting state |
| CONFIG_GET/SET | Hub ↔ node | Controlled configuration |
| OTA_CONTROL | Hub ↔ node | Optional future firmware update coordination |

## 4.4 Command lifecycle

Suggested states:
- `created`
- `authorized`
- `queued`
- `sent`
- `acknowledged`
- `executing`
- `succeeded`
- `failed`
- `expired`
- `cancelled`
- `unknown` (when outcome cannot be established)

A successful serial write or ESP-NOW send callback advances transport status only. The node must return a command-level ACK and, where applicable, a final execution result.

## 4.5 Retry and duplicate behavior

- Assign each command a globally unique command ID.
- Node stores a bounded recent-command cache.
- If a duplicate command ID arrives, return the prior result instead of repeating a non-idempotent action.
- Prefer desired-state commands such as `set_output=false` over `toggle`.
- Retry policy must be bounded and use backoff/jitter.
- On expiry, reject late commands.
- If the hub cannot determine whether an action executed, show `unknown` and reconcile from reported state; do not blindly repeat unsafe actions.

## 4.6 Heartbeats and offline state

- Heartbeat interval is configurable per device class.
- Backend calculates online/stale/offline based on last authenticated message and configured thresholds.
- A gateway disconnect is distinct from an individual node disconnect.
- Nodes should expose uptime, firmware version, supply/battery state where available, and fault flags.

## 4.7 Gateway serial framing

USB serial is a byte stream, not a packet transport. Define framing explicitly:
- Frame delimiter/encoding (e.g. COBS or SLIP), or length-prefixed framing with robust resynchronization.
- Protocol version.
- Frame length with a strict maximum.
- Message type and gateway identity.
- Payload.
- CRC for accidental corruption detection (not a replacement for cryptographic authentication).

Parser requirements:
- Handle partial reads and multiple frames in one read.
- Reject oversized or malformed frames.
- Recover after corrupted bytes.
- Apply read/write timeouts.
- Use bounded queues and backpressure.
- Include gateway boot ID and health messages.

## 4.8 Example conceptual telemetry

```json
{
  "type": "TELEMETRY",
  "protocol_version": 1,
  "device_id": "dev_01H...",
  "boot_id": "boot_...",
  "sequence": 1024,
  "entities": [
    {"entity_id": "light_level", "value": 183, "unit": "lx"}
  ]
}
```

This JSON is explanatory only. The radio wire format should be compact and must fit the selected payload budget.

## 4.9 Versioning

- Protocol versions are explicit.
- Unknown optional fields may be ignored only when safe.
- Unknown message types are rejected and logged.
- Breaking changes require a version increment and compatibility tests.
- Gateway and node firmware should report protocol compatibility during discovery.
