# 6. Angular Frontend Design

## 6.1 Principles

- Angular frontend is a local web client for the Python backend.
- It must not communicate directly with ESP32 devices.
- Local control and dashboards must work without internet.
- Use backend WebSocket/events for live updates.
- Keep authentication and authorization decisions on the backend.

## 6.2 Suggested application areas

1. **Overview** — hub health, gateway health, device counts, alerts, recent events.
2. **Rooms** — rooms and their devices/entities.
3. **Devices** — discovered, pending, approved, offline, disabled devices.
4. **Device detail** — identity, model, firmware, capabilities, telemetry, diagnostics.
5. **Controls** — safe controls based on entity capability.
6. **Automations** — rule list, editor, enable/disable, execution history.
7. **Users and access** — local users, roles, sessions.
8. **Integrations** — Supabase/Ably connection state and sync status.
9. **System** — gateway serial status, logs, backup, update, version.

## 6.3 Device onboarding UX

- Show newly discovered devices as pending candidates.
- Show only non-sensitive identity and capability information before approval.
- Require explicit administrator action or a documented physical pairing flow.
- Display pairing progress and failure reason.
- After approval, prompt to assign a room and friendly name.
- Provide a way to revoke/disable a device.

## 6.4 Live state

- Initial page load uses REST.
- WebSocket delivers subsequent state, command, gateway, and automation events.
- Reconnect with backoff.
- On reconnect, refresh authoritative state to avoid gaps.
- Show stale/offline state distinctly from a sensor reading of zero or false.

## 6.5 UI safety

- Distinguish “command sent”, “device acknowledged”, and “action completed”.
- Avoid ambiguous toggle actions for critical actuators.
- Confirm high-impact actions.
- Show interlock/fault reasons returned by the backend.
- Do not show a control as successful until a device-level result is received.

## 6.6 Frontend structure (suggested)

```text
frontend/
  src/
    app/
      core/             # auth, API client, websocket, guards
      shared/           # reusable components and pipes
      features/
        overview/
        rooms/
        devices/
        automations/
        users/
        integrations/
        system/
    environments/
```

Use Angular standalone components and lazy-loaded feature routes where appropriate. Final Angular version and UI library should be pinned during implementation.
