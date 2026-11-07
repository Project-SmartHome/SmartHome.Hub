# 9. Deployment and Operations

## 9.1 Host

Initial target: Raspberry Pi or Linux mini PC. Confirm CPU architecture, RAM, storage endurance, USB port availability, and power quality before finalizing images and dependencies.

## 9.2 Services

Suggested logical services:
- `smarthome-backend`
- `smarthome-frontend` (may be served by backend/reverse proxy)
- `smarthome-gateway-bridge` (may run as a backend-managed component initially)
- Optional reverse proxy
- Optional backup/update agent

Start with a single backend process unless measured load or fault isolation justifies splitting services. Keep the serial port owned by exactly one process.

## 9.3 Serial device operations

- Identify gateway by stable udev path where possible, not a volatile `/dev/ttyUSB0` assumption.
- Configure permissions for a dedicated service account.
- Handle unplug/replug and gateway reboot.
- Reconnect with backoff.
- Expose serial port, firmware version, gateway ID, and last-seen status in diagnostics.
- Prevent two services from opening the same serial device concurrently.

## 9.4 Configuration

Separate:
- application configuration
- hardware/gateway configuration
- local secrets
- cloud integration secrets

Validate configuration at startup. Fail safely with a clear diagnostic if required configuration is invalid. Do not print secret values.

## 9.5 Updates

Recommended update sequence:
1. Check release metadata and compatibility.
2. Download artifact.
3. Verify signature/checksum.
4. Create backup.
5. Stage new release.
6. Apply database migration with a tested rollback strategy.
7. Restart service.
8. Run health and smoke tests.
9. Mark release successful or roll back to previous version.
10. Preserve logs and update audit record.

Never assume a database downgrade is safe. Prefer forward-compatible migrations and tested restore procedures.

## 9.6 Backup and restore

Back up:
- database
- configuration
- device registry and credential material (encrypted/protected)
- automation rules
- integration configuration
- release metadata

Store encrypted backups on separate storage where practical. Test restore on a clean environment. A backup is not proven until restore has been tested.

## 9.7 Monitoring

Track:
- backend process health
- database health
- gateway connected/disconnected
- serial reconnect count
- node last-seen and packet loss
- telemetry queue depth
- command success/failure/timeout
- automation failures
- cloud outbox depth and age
- disk space and backup age

## 9.8 Recovery behavior

- Gateway restart: reconnect serial, reinitialize radio, re-establish peer state, and resume discovery.
- Node restart: new boot ID, reannounce, authenticate, restore configured behavior.
- Backend restart: restore persisted registry and automation state; re-open gateway link.
- Internet outage: local control continues; cloud outbox retries later.
- Database corruption: stop writes, preserve evidence, restore last known-good backup.
