# 12. Architecture Decision Records

## ADR-001: Local-first hub

**Status:** Accepted

**Decision:** The Linux hub is the local authority for device registry, local authentication, automation, and immediate device control.

**Reason:** Local home automation must continue when the internet or cloud services are unavailable.

**Consequences:** Cloud integrations are optional adapters and must not be dependencies of the local control path.

## ADR-002: Python backend and Angular frontend

**Status:** Accepted

**Decision:** Use Python (FastAPI baseline) for the backend and Angular for the browser application.

**Reason:** This is the selected application stack for the current project direction.

**Consequences:** Keep hardware communication behind Python service interfaces and expose stable HTTP/WebSocket APIs to Angular.

## ADR-003: ESP-NOW for ESP32 node communication

**Status:** Accepted

**Decision:** ESP32 nodes communicate wirelessly with a dedicated ESP32 gateway using ESP-NOW. Nodes do not join the household Wi-Fi access point.

**Reason:** The nodes need a local wireless link without requiring each node to connect to the home router or internet.

**Consequences and constraints:**
- ESP-NOW uses the ESP32 Wi-Fi radio.
- The initial topology is star, not native mesh.
- Gateway and nodes must use a compatible channel.
- Peer and encrypted-peer limits must be qualified for the exact chip/SDK.
- Initial payload target is <=250 bytes.
- Discovery is untrusted until secure onboarding completes.
- Application-level ACKs and bounded retries are required.

## ADR-004: Dedicated ESP32 gateway connected over USB serial

**Status:** Accepted

**Decision:** Use a dedicated ESP32 as the radio gateway and connect it to the Linux hub over USB serial.

**Reason:** The Linux host is not assumed to have native ESP-NOW capability.

**Consequences:** The gateway handles transport only. The Python backend owns application behavior, authorization, registry, and automation.

## ADR-005: Optional Supabase and Ably

**Status:** Accepted with implementation details TBD

**Decision:** Use Supabase and Ably as optional cloud integrations.

**Reason:** Cloud features can extend the system without making them mandatory for local operation.

**Consequences:** Local auth and local persistence remain available. Cloud synchronization uses a durable outbox and explicit conflict policies.

## ADR-006: Initial database

**Status:** Proposed

**Decision:** Start with SQLite for a single-hub installation.

**Reason:** It is simple to deploy and supports a local single-node system.

**Consequences:** Use migrations, backups, retention controls, and load testing. Revisit PostgreSQL if concurrent workloads or multi-hub requirements demand it.

## Open decisions

1. Exact ESP32 gateway and node board/chip variants.
2. ESP-IDF version and build tooling.
3. Secure provisioning implementation and key lifecycle.
4. Fixed radio channel and commissioning procedure.
5. Serial framing format and binary wire schema.
6. Initial device capability catalog and entity naming convention.
7. Local authentication/session strategy and TLS/reverse-proxy setup.
8. Supabase data model, if cloud sync is enabled.
9. Ably channel naming, authorization, and event selection.
10. Deployment choice: systemd, Podman, or another supported runtime.
11. Backup destination, retention, and encryption approach.
