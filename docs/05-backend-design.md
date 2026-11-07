# 5. Python Backend Design

## 5.1 Technology baseline

- Python 3.12+ (pin a tested version for deployment).
- FastAPI and Uvicorn for HTTP/WebSocket.
- Pydantic for request and message validation.
- SQLAlchemy 2.x and Alembic for persistence/migrations.
- SQLite for a single-hub initial deployment; evaluate PostgreSQL only when justified.
- `pyserial` or `pyserial-asyncio` for USB serial integration.
- `pytest` and async test tooling.
- Structured logging with rotation and redaction.

Pin dependencies and test the selected versions together. Do not expose database or serial access directly to route handlers.

## 5.2 Service boundaries

- `auth`: local accounts, password hashes, sessions, roles.
- `devices`: registry, onboarding, device metadata.
- `capabilities`: supported entity types and schemas.
- `gateway`: serial connection lifecycle and health.
- `transports`: transport abstraction and ESP-NOW protocol handling.
- `telemetry`: ingestion, validation, state updates, history.
- `commands`: authorization, queueing, delivery, result tracking.
- `automation`: triggers, conditions, actions, execution history.
- `integrations`: Supabase and Ably adapters.
- `outbox`: durable cloud event queue and retry.
- `system`: health, diagnostics, update state, backup metadata.

## 5.3 Transport abstraction

Define a transport interface with operations equivalent to:
- start/stop
- discover
- send command
- receive validated message stream
- report transport health

Keep gateway serial framing separate from ESP-NOW message semantics. This permits a future gateway implementation without rewriting automation or device registry code.

## 5.4 API outline

### Authentication
- `POST /api/v1/auth/login`
- `POST /api/v1/auth/logout`
- `GET /api/v1/auth/me`
- `POST /api/v1/auth/password`

### Devices
- `GET /api/v1/devices`
- `GET /api/v1/devices/{device_id}`
- `GET /api/v1/devices/pending`
- `POST /api/v1/devices/{device_id}/approve`
- `PATCH /api/v1/devices/{device_id}`
- `POST /api/v1/devices/{device_id}/disable`
- `DELETE /api/v1/devices/{device_id}`

### Entities and state
- `GET /api/v1/entities`
- `GET /api/v1/entities/{entity_id}`
- `GET /api/v1/entities/{entity_id}/history`
- `GET /api/v1/events` (WebSocket or streaming endpoint)

### Commands
- `POST /api/v1/devices/{device_id}/commands`
- `GET /api/v1/commands/{command_id}`

### Rooms and automations
- `GET/POST /api/v1/rooms`
- `PATCH/DELETE /api/v1/rooms/{room_id}`
- `GET/POST /api/v1/automations`
- `PATCH/DELETE /api/v1/automations/{automation_id}`
- `POST /api/v1/automations/{automation_id}/enable`
- `POST /api/v1/automations/{automation_id}/disable`

### System
- `GET /api/v1/health`
- `GET /api/v1/system/gateway`
- `GET /api/v1/system/diagnostics`

All routes require explicit authorization policy. This outline is not a final OpenAPI contract.

## 5.5 Initial data model

Core tables/entities:
- `users`, `sessions`, `roles`, `user_roles`
- `devices`, `device_credentials`, `device_capabilities`
- `rooms`, `entities`
- `entity_state`, `telemetry_events`
- `commands`, `command_events`
- `automations`, `automation_runs`
- `gateways`, `gateway_events`
- `integration_config` (secrets stored separately/securely)
- `cloud_outbox`, `audit_events`

Use stable UUID/ULID-style IDs. Store MAC address as a transport attribute, not as the sole device identity. Keep credentials out of ordinary device metadata APIs.

## 5.6 Command service rules

- Authenticate user/session.
- Authorize role and entity permission.
- Validate capability and requested value.
- Apply safety/interlock policy.
- Persist command before sending.
- Send through gateway adapter.
- Track delivery and node result separately.
- Publish state/result to local subscribers.
- Audit the actor, target, request, and outcome.

## 5.7 Automation engine

Run locally in the backend. Separate:
- event ingestion
- rule evaluation
- action execution
- run history

Prevent feedback loops using event origin/correlation IDs, execution limits, and idempotency. Add rule simulation/dry-run before enabling high-impact automations.

## 5.8 Database and retention

- Use Alembic migrations.
- Persist configuration, credentials metadata, commands, audit events, and outbox records.
- Make telemetry retention configurable.
- Avoid writing every high-frequency sample indefinitely to SQLite.
- Consider rollups and bounded retention.
- Back up SQLite consistently (SQLite backup API or a controlled quiesced backup), not by copying an actively changing database file blindly.

## 5.9 Testing

- Unit tests for validation, authorization, state transitions, and rules.
- Protocol parser fuzz/negative tests.
- Fake gateway for integration tests.
- Hardware-in-loop tests for ESP-NOW.
- Migration and backup/restore tests.
- Offline tests with cloud integrations disabled or unreachable.
