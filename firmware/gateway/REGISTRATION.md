# Device registration bridge

The node sends a version-1 `NodeFrame` over ESP-NOW. The gateway replies to discovery,
then publishes registration requests to Ably MQTT. Discovery, `RegisterRequest` and
`Heartbeat` frames carry the node's `firmwareVersion`, and their payload holds its
firmware target (e.g. `node-nodemcuv2`); the gateway uses both to push firmware
updates to the node (see `../OTA.md`). The gateway returns `Registered`,
`RegisterPending`, or `RegisterRejected` to the node only after receiving the matching
cloud response (it sends `RegisterPending` while the cloud decision is outstanding).

## Ably channel contract

Each gateway is owned by one user (`SH_USER_ID` in `include/secrets.h`), and every
channel is scoped to that user. The gateway publishes JSON to
`smarthome/<userId>/device-registration/requests`:

```json
{
  "event": "device.register.request",
  "requestId": "sh-device-id",
  "deviceId": "sh-device-id",
  "mac": "AA:BB:CC:DD:EE:FF",
  "firmwareVersion": "0.1.0",
  "homeId": "",
  "roomId": "",
  "gatewayId": "gateway-AA-BB-CC-DD-EE-FF",
  "userId": "<userId>"
}
```

A backend/service must consume that channel, perform the actual device registration,
then publish to `smarthome/<userId>/device-registration/acks/<deviceId>`. Its JSON response is:

```json
{"requestId":"sh-device-id","status":"registered","homeId":"...","roomId":"..."}
```

`status` is `pending`, `registered`, or `rejected`. A later final response can be
published to the same acknowledgement channel using the same `requestId`. The stable
device ID is the idempotency key, so the backend should process retries idempotently.
Ably Pub/Sub transports these events; it does not itself persist/register devices.

## Gateway setup

Copy `include/secrets.example.h` to `include/secrets.h`, set Wi-Fi credentials and
`SH_USER_ID`, and flash over USB once; the values are saved to flash and kept across
OTA updates. Create a dedicated Ably API key with publish permission for
`smarthome/<userId>/device-registration/requests` and subscribe permission for
`smarthome/<userId>/device-registration/acks/*`. The device ID is `sh-` followed by the
node's MAC address, so it stays the same across reflashing and storage resets. Do not reuse the
room-node key, which should remain subscribe-only. The gateway joins Wi-Fi and follows
the access point's channel; the node scans ESP-NOW channels until it discovers it.