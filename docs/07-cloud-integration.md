# 7. Cloud Integration: Supabase and Ably

## 7.1 Cloud is optional

The hub is the local authority for immediate device control, automations, and local user experience. Supabase and Ably are optional cloud extensions. No local action should wait for a cloud response.

## 7.2 Supabase responsibilities

Potential uses:
- Cloud identity or account linking.
- Optional cloud backup of selected metadata.
- Remote access authorization support, if a secure remote-access design is added.
- Cross-installation metadata synchronization, if required.

Do not make Supabase Auth the only authentication path for local operation. Keep local authentication/session management available when the internet is down.

## 7.3 Ably responsibilities

Potential uses:
- Publish selected device status or events to cloud subscribers.
- Remote notifications.
- Cross-device/cloud application updates.

Ably should not be required for ESP32-to-hub messaging. Nodes communicate locally with the gateway regardless of Ably availability.

## 7.4 Outbox and synchronization

- Persist cloud-eligible events in a durable outbox.
- Assign unique event IDs and idempotency keys.
- Retry with exponential backoff and jitter.
- Track attempts, next retry time, and terminal failure.
- Bound queue size and define retention/backpressure policy.
- Do not upload secrets or unnecessary high-frequency telemetry.
- Define which side wins for each synchronized field; avoid vague “last write wins” for safety-critical state.

## 7.5 Connectivity states

Expose:
- `online`
- `degraded`
- `offline`
- `sync_pending`
- `sync_error`

The UI should clearly distinguish local hub health from cloud connectivity.

## 7.6 Secret handling

- Keep Supabase service-role keys and Ably API keys only on the hub.
- Never embed privileged keys in Angular bundles or ESP32 firmware.
- Prefer least-privilege credentials and rotation.
- Redact tokens from logs and diagnostics.
- Store secrets in protected OS files or a suitable secret store with restrictive permissions.
