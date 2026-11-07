# SmartHome Hub Documentation

This documentation package describes a local-first home automation platform built around a Linux hub, a Python backend, an Angular frontend, and ESP32 devices communicating with the hub through ESP-NOW.

## Product goals

- Operate locally on the home network without requiring cloud connectivity.
- Keep ESP32 sensor/actuator nodes off the household Wi-Fi network.
- Use one ESP32 gateway connected to the Linux hub over USB serial.
- Automatically discover and securely onboard supported ESP32 nodes.
- Keep automation, device registry, authorization, and application logic on the hub.
- Integrate with Supabase and Ably only when internet connectivity is available.
- Support safe updates, backups, health monitoring, and rollback.

## Documentation index

| Document | Purpose |
|---|---|
| [Product requirements](01-product-requirements.md) | Scope, requirements, user-facing behavior, acceptance criteria |
| [System architecture](02-system-architecture.md) | Components, boundaries, data flows, deployment |
| [ESP-NOW network design](03-esp-now-network-design.md) | Topology, discovery, pairing, reliability, radio constraints |
| [Device protocol](04-device-protocol.md) | Message envelope, message types, ACK/retry, serial framing |
| [Backend design](05-backend-design.md) | Python services, data model, APIs, automation |
| [Frontend design](06-frontend-design.md) | Angular application structure and screens |
| [Cloud integration](07-cloud-integration.md) | Supabase, Ably, offline-first synchronization |
| [Security design](08-security-design.md) | Threat model, onboarding, keys, local auth |
| [Deployment and operations](09-deployment-operations.md) | Linux deployment, updates, backup, recovery |
| [Repository structure](10-repository-structure.md) | Proposed monorepo layout |
| [Development roadmap](11-development-roadmap.md) | Phases, milestones, test strategy |
| [Architecture decisions](12-architecture-decisions.md) | Key decisions and open questions |

## Important architecture clarification

ESP-NOW uses the ESP32 Wi-Fi radio and 802.11 action frames, but ESP32 nodes do **not** need to associate with the household Wi-Fi access point or have internet access. The selected topology is a **star**, not a native mesh: nodes communicate directly with a gateway within reliable radio range. For larger coverage, add independently connected gateways or design and validate a deliberate relay scheme.

## Status and assumptions

This is a recreated baseline based on the decisions discussed so far. Items marked **TBD** require a deliberate decision or hardware validation before implementation. In particular, select the exact ESP32 chip/board family and ESP-IDF version before fixing peer limits, encryption configuration, and firmware build settings.

## Suggested first milestone

Bring up one gateway and two nodes; demonstrate discovery, authenticated onboarding, heartbeat, sensor telemetry, and a safe relay command with an application-level result acknowledgement. Then scale to the intended 10–15 nodes and test RF coverage and peer capacity.
