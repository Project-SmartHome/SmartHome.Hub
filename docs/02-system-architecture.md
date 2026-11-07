# 2. System Architecture

## 2.1 Logical architecture

```text
                 Internet (optional)
                +--------------------+
                | Supabase / Ably    |
                +---------+----------+
                          |
                 outbound secure sync
                          |
+---------------------------------------------------+
| Linux host: Raspberry Pi / mini PC                |
|                                                   |
|  +---------------- Angular local web application+ |
|  | Dashboard / Setup / Devices / Automations     | |
|  +------------------------+----------------------+ |
|                           | HTTPS / WebSocket      |
|  +------------------------v----------------------+ |
|  | Python backend (FastAPI)                      | |
|  | Auth | Registry | Rules | State | API         | |
|  | Telemetry | Command service | Cloud outbox   | |
|  +------------------------+----------------------+ |
|                           | USB serial             |
+---------------------------|-----------------------+
                            |
                   +--------v---------+
                   | ESP32 gateway    |
                   | ESP-NOW + serial |
                   +--------+---------+
                            )) radio
                 +----------+-----------+
                 |          |           |
             +---v---+  +---v---+   +---v---+
             | Node 1|  | Node 2|   | Node N|
             |sensor |  |relay  |   |mixed  |
             +-------+  +-------+   +-------+
```

## 2.2 Component responsibilities

### Linux hub
- Hosts the backend, database, frontend, and operational services.
- Owns users, permissions, device registry, rooms, entities, automations, and audit history.
- Validates every command and decides whether it is permitted.
- Maintains local state and local automation execution.
- Manages cloud synchronization and remote integrations.

### ESP32 gateway
- Runs ESP-NOW radio and USB serial transport.
- Receives node frames and forwards them to the backend.
- Sends backend frames to the addressed node.
- Maintains the radio peer table and reports radio/gateway health.
- Implements only transport-level behavior needed for reliable forwarding.
- Does not decide automation outcomes or user permissions.

### ESP32 nodes
- Read attached sensors and operate attached actuators.
- Advertise discovery and identity information.
- Send heartbeat and telemetry.
- Validate authenticated commands and local safety constraints.
- Return application-level command results.
- Continue to enforce safe local defaults if gateway communication is lost.

### Python backend
- Exposes REST and WebSocket APIs.
- Implements device onboarding, registry, capability model, state, command lifecycle, automation, auth, and cloud sync.
- Abstracts gateway transport behind interfaces to permit future transport changes.

### Angular frontend
- Communicates only with backend APIs.
- Provides local-first user workflows.
- Does not connect directly to ESP-NOW or store device secrets.

## 2.3 Data flow

### Discovery
1. Node boots and sends a compact discovery announcement.
2. Gateway receives the frame and forwards it over serial.
3. Backend validates protocol shape and rate limits the sender.
4. Backend records a pending candidate.
5. Administrator or approved policy completes secure onboarding.
6. Backend creates a device and capability records.

### Telemetry
1. Node sends telemetry with device identity, sequence, and entity values.
2. Gateway forwards the frame.
3. Backend authenticates sender, validates sequence and schema, and updates state.
4. Backend persists selected history and publishes a local WebSocket event.
5. If enabled and online, an outbox worker synchronizes eligible data to cloud services.

### Command
1. User or local automation requests an action.
2. Backend authorizes the actor and validates capability, value, and safety policy.
3. Backend creates a command record with unique ID and expiry.
4. Gateway transmits to the target node.
5. Node validates authenticity, expiry, sequence, and local interlocks.
6. Node returns accepted/rejected and execution result.
7. Backend updates command status and broadcasts the result to the UI.

## 2.4 Deployment baseline

Recommended initial deployment:
- Linux host with stable power and storage.
- Python backend managed by systemd or a container runtime.
- SQLite for initial single-hub deployment; PostgreSQL can be considered if multi-user/concurrency requirements justify it.
- Angular static build served by the backend or a local reverse proxy.
- One USB-connected ESP32 gateway.
- ESP32 nodes powered according to their sensor/actuator requirements.

Keep the database, configuration, backups, and logs on persistent storage. Avoid relying on ephemeral container filesystems.

## 2.5 Availability boundaries

- Internet loss: local UI, local auth, device control, and local automation continue.
- Cloud outage: cloud outbox accumulates bounded/retryable records; local features continue.
- Gateway loss: backend remains available, marks radio devices stale/offline, and reports gateway failure.
- Hub loss: nodes must enter safe device-specific behavior; do not assume the hub is available for safety.
- Node loss: only that node becomes unavailable; other nodes continue.

## 2.6 Scaling and coverage

ESP-NOW is not a built-in mesh. A single gateway star is appropriate only when all nodes have reliable direct radio coverage. Validate coverage in the actual building and installation locations. If needed, deploy multiple gateways attached to the same hub, each with a unique gateway identity and radio/channel plan. Multi-gateway routing and device ownership must be designed explicitly.
