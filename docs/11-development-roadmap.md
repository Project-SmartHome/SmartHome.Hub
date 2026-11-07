# 11. Development Roadmap

## Phase 0 — Decisions and hardware qualification
- Select exact gateway and node ESP32 variants.
- Select ESP-IDF version and build system.
- Confirm USB serial interface and stable device path.
- Validate ESP-NOW peer/encrypted-peer limits for selected targets.
- Choose channel strategy and provisioning/security approach.
- Define initial sensor and actuator board prototypes.

**Exit:** hardware bill of materials and compatibility matrix approved.

## Phase 1 — Repository and engineering foundation
- Create repository structure.
- Add Python project configuration, linting, formatting, typing, and tests.
- Add Angular workspace and build pipeline.
- Add firmware build pipelines.
- Add documentation and architecture decision records.
- Establish secret scanning and release signing.

**Exit:** clean CI builds and tests all project components.

## Phase 2 — Gateway serial link
- Implement gateway USB serial framing.
- Implement Python serial connection manager.
- Add reconnect, timeouts, bounded queues, and health reporting.
- Build a simulator/fake gateway for backend tests.

**Exit:** backend and gateway exchange framed test messages reliably.

## Phase 3 — ESP-NOW foundation
- Implement node and gateway radio initialization.
- Fix channel configuration.
- Implement compact discovery and heartbeat.
- Add protocol version and sequence handling.
- Test one node, then multiple nodes.

**Exit:** gateway receives messages from the target node count under controlled conditions.

## Phase 4 — Secure onboarding
- Implement selected authenticated provisioning method.
- Add pending device workflow and approval UI.
- Store per-device credentials securely.
- Add revocation and re-provisioning.
- Test spoofed discovery and replay rejection.

**Exit:** only authenticated, approved devices can send trusted telemetry or receive commands.

## Phase 5 — Registry, capabilities, and telemetry
- Implement device, room, entity, and capability models.
- Persist telemetry and current state.
- Add online/stale/offline calculation.
- Add local WebSocket events.

**Exit:** dashboard displays live device state and diagnostics.

## Phase 6 — Commands and safety
- Implement command authorization and lifecycle.
- Add node ACK/result messages.
- Add idempotency, expiry, duplicate suppression, and retry policy.
- Implement safe boot and communication-loss behavior.
- Test relay outputs using safe low-voltage loads before any mains installation.

**Exit:** UI command results reflect actual device execution result.

## Phase 7 — Angular application
- Build overview, rooms, devices, onboarding, and device detail.
- Add command controls and live status.
- Add auth and role-aware navigation.
- Add responsive mobile layout.

**Exit:** primary home-control workflow works entirely on the LAN.

## Phase 8 — Automation engine
- Implement triggers, conditions, actions, and run history.
- Add loop prevention and rate limits.
- Add dry-run/simulation and error reporting.

**Exit:** representative automations run with internet disconnected.

## Phase 9 — Cloud integrations
- Add optional Supabase adapter.
- Add optional Ably adapter.
- Add durable outbox and retry.
- Define data ownership and conflict policies.
- Test offline/online transitions.

**Exit:** cloud outages do not impair local operation.

## Phase 10 — Deployment, backup, and updates
- Create systemd or container deployment.
- Add configuration validation and secret handling.
- Add signed release verification.
- Add backup/restore and rollback.
- Add health checks and update audit logs.

**Exit:** install, upgrade, rollback, and restore are repeatable.

## Phase 11 — Qualification and release
- Run RF coverage and interference tests.
- Stress test 10–15 nodes or the selected target.
- Test gateway/hub/node restart and power-loss scenarios.
- Test malformed frames, replay, unauthorized users, and cloud outages.
- Document supported hardware and known limitations.

**Exit:** release checklist and acceptance criteria pass.

## Definition of done for a feature

- Requirements and API/protocol contracts updated.
- Unit and integration tests pass.
- Failure and offline behavior documented.
- Logs do not expose secrets.
- UI communicates true command/state status.
- Migration and rollback implications considered.
- Hardware behavior tested where relevant.
