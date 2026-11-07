# 8. Security Design

## 8.1 Threat model

Protect against:
- Nearby unauthorized devices broadcasting fake discovery messages.
- MAC address spoofing.
- Replay of captured telemetry or commands.
- Unauthorized local users controlling actuators.
- Compromised or stolen nodes.
- Leaked cloud credentials.
- Malformed serial/radio frames causing crashes or resource exhaustion.
- Malicious or accidental unsafe automation.

## 8.2 Device identity and pairing

- MAC address is an address, not a trusted identity.
- Discovery announcements are untrusted input.
- Require explicit enrollment policy.
- Use unique per-device credentials.
- Use a supported authenticated provisioning/key-exchange mechanism for the selected chip/SDK.
- Protect the gateway's provisioning secrets and device key database.
- Support revocation and re-provisioning.
- Do not compile a shared fleet-wide secret into all devices.

## 8.3 Message protection

- Use ESP-NOW encrypted unicast where supported and correctly provisioned.
- If an application security layer is needed, use a reviewed authenticated-encryption design rather than custom crypto.
- Include authenticated sequence/session information to prevent replay.
- Validate destination, message type, length, version, and authorization.
- Rate-limit discovery and pairing attempts.
- Treat CRC as accidental-corruption detection only, not authentication.

## 8.4 Local application security

- Hash passwords using a modern password-hashing algorithm such as Argon2id or bcrypt with appropriate parameters.
- Use secure, HttpOnly, SameSite cookies for browser sessions where applicable.
- Protect state-changing requests against CSRF when cookie authentication is used.
- Enforce role/permission checks in backend services.
- Rate-limit login and pairing endpoints.
- Audit administrative actions.
- Use HTTPS on networks where practical; if plain HTTP is used on an isolated LAN during development, document the risk and do not expose it to untrusted networks.

## 8.5 Actuator safety

- Use hardware interlocks and appropriately rated contactors/relays for loads.
- ESP32 GPIO must not directly switch mains loads.
- Define safe output states on boot, reset, brownout, watchdog, and communication loss.
- Commands should set a desired state rather than toggle where possible.
- Apply local timeout/failsafe behavior for actuators that could cause damage.
- Keep emergency/manual isolation independent of software.
- Electrical design and installation must be reviewed by a qualified person.

## 8.6 Secrets

- No privileged cloud credentials in firmware or frontend.
- No secrets in source control, logs, crash reports, or support bundles.
- Use environment files with restrictive permissions or OS secret management.
- Provide redaction tests.
- Rotate compromised credentials and revoke affected devices.

## 8.7 Secure update path

- Download only from configured trusted release endpoints.
- Verify release signatures/checksums using a pinned trust root.
- Do not install unsigned artifacts in production.
- Keep previous known-good release for rollback.
- Record version, artifact hash, and update result.
