# 1. Product Requirements Document

## 1.1 Product summary

SmartHome Hub is a local-first home automation platform hosted on a Raspberry Pi or Linux mini PC. It coordinates ESP32-based sensor and actuator nodes over ESP-NOW through a dedicated ESP32 gateway connected to the hub over USB serial. A local Angular web application provides setup, monitoring, device control, and automation management.

Cloud services are optional extensions. Supabase can provide cloud identity/metadata or remote synchronization, and Ably can provide cloud pub/sub when the hub has internet access. Local control must continue when either service or the internet is unavailable.

## 1.2 Goals

1. Local operation is the default and remains functional offline.
2. ESP32 nodes do not join the household Wi-Fi network.
3. Nodes are discovered automatically and can be securely onboarded without manual MAC-address entry.
4. The hub owns device registry, automation rules, user permissions, and authoritative local state.
5. A gateway provides radio-to-USB transport only; it does not host business logic.
6. The platform supports multiple sensor and actuator types through capability-based device models.
7. Cloud connectivity is optional and must never be a prerequisite for local control.
8. Updates, backups, diagnostics, and recovery are repeatable and observable.

## 1.3 Non-goals for the first release

- A general-purpose native ESP-NOW mesh network.
- Dependence on cloud services for local switching or safety interlocks.
- Arbitrary third-party firmware execution.
- Guaranteed compatibility with every ESP32 variant before hardware qualification.
- Safety certification for mains electrical installations. Electrical installation and protection must be designed and checked by a qualified professional.

## 1.4 Primary users and use cases

- Home owner: sees system status, rooms, devices, and alerts.
- Administrator: provisions devices, manages users, configures integrations, and updates the hub.
- Household member: uses permitted controls and views permitted device status.

Core use cases:
- Open the local dashboard from a phone or browser on the home LAN.
- Discover a newly powered ESP32 node.
- Approve or securely pair a node.
- Assign the node to a room and identify its capabilities.
- View live sensor readings and device connectivity.
- Operate a relay or other actuator and see the execution result.
- Create an automation such as “when it is dark, turn on a light.”
- Continue local automations during an internet outage.
- Restore configuration and device registry after a hub replacement.

## 1.5 Functional requirements

### Devices and discovery
- The gateway listens for ESP-NOW discovery announcements.
- The hub validates the announcement and starts an onboarding workflow.
- Untrusted discovery packets must not grant control or registry access.
- New devices enter a pending/untrusted state until authenticated and approved according to the selected onboarding policy.
- The hub stores a stable device ID separately from the radio MAC address.
- A device can advertise firmware version, protocol version, hardware model, and supported capabilities.
- Administrators can rename, assign a room, disable, remove, or re-provision a device.

### Device capabilities
- Model capabilities explicitly: digital input, analog input, temperature, humidity, light level, flow, relay, switch, motor controller, etc.
- A device may expose multiple entities.
- Entities have stable IDs, units, value types, read/write permissions, and metadata.
- Unsupported or unknown capabilities must not crash the hub; preserve diagnostic information and mark them unsupported.

### Telemetry and state
- Accept telemetry with device ID, entity ID, value, unit/type, sequence number, and timestamp.
- Track last-seen time, link status, firmware version, and gateway association.
- Distinguish reported state from desired state.
- Record command request, delivery, device acknowledgement, and final execution result separately.

### Control
- Commands are authorized by the hub before transmission.
- Commands include a unique command ID, sequence, issue time, and expiry.
- Actuators should use safe boot defaults and local electrical interlocks.
- A transport-level send success is not proof that the actuator executed the command.
- The device must send an application-level acknowledgement/result.

### Automation
- Rules run locally on the hub.
- Triggers include telemetry changes, state transitions, schedules, and system events.
- Conditions and actions are explicit, validated, and auditable.
- Automation execution must be idempotent where practical and protect against loops.
- Safety-related control must not rely solely on a cloud round trip.

### Users and sessions
- Provide local user accounts and role-based permissions.
- Support secure password hashing, session expiry, logout, and credential changes.
- Require administrator authorization for pairing, user management, integrations, and firmware updates.
- Do not expose default credentials in production.

### Cloud
- Cloud features are optional and separately configurable.
- Queue eligible events while offline and retry when online.
- Avoid cloud-to-local command execution unless an explicit secure remote-control feature is designed and enabled.
- Local operation must not wait for cloud synchronization.

## 1.6 Non-functional requirements

- Local dashboard and APIs should remain available without internet.
- Backend services should restart automatically after host reboot.
- State changes and command outcomes should be observable through logs and diagnostics.
- Use bounded queues and backpressure to avoid unbounded memory growth.
- Persist critical configuration and event/outbox records.
- Use database migrations and tested backup/restore procedures.
- Support a clean separation between hardware transport and automation/business logic.
- Provide automated tests for protocol parsing, authorization, rules, and recovery.
- Define performance targets after measuring the selected hardware and RF environment.

## 1.7 Acceptance criteria

Initial end-to-end acceptance:
1. A Linux hub communicates with one ESP32 gateway over USB serial.
2. At least two nodes announce themselves and are discovered without entering household Wi-Fi credentials.
3. An unauthenticated node cannot issue accepted actuator commands.
4. An authorized node sends heartbeat and telemetry.
5. The dashboard shows device identity, room, capability, last-seen, and state.
6. An authorized user sends a relay command and sees a device-level result acknowledgement.
7. Local control and automations continue with internet disconnected.
8. Restarting the backend and gateway recovers device communication without corrupting the registry.
9. The design is load-tested with the target 10–15 nodes and the selected ESP32 variant.
