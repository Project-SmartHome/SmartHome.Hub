# 10. Proposed Repository Structure

```text
SmartHome.Hub/
├── README.md
├── docs/
│   ├── 01-product-requirements.md
│   ├── 02-system-architecture.md
│   ├── 03-esp-now-network-design.md
│   ├── 04-device-protocol.md
│   ├── 05-backend-design.md
│   ├── 06-frontend-design.md
│   ├── 07-cloud-integration.md
│   ├── 08-security-design.md
│   ├── 09-deployment-operations.md
│   ├── 10-repository-structure.md
│   ├── 11-development-roadmap.md
│   └── 12-architecture-decisions.md
├── backend/
│   ├── app/
│   │   ├── api/
│   │   ├── auth/
│   │   ├── devices/
│   │   ├── capabilities/
│   │   ├── gateway/
│   │   │   ├── serial_gateway.py
│   │   │   └── gateway_manager.py
│   │   ├── transports/
│   │   │   └── esp_now/
│   │   │       ├── transport.py
│   │   │       ├── packet_parser.py
│   │   │       └── protocol.py
│   │   ├── telemetry/
│   │   ├── commands/
│   │   ├── automation/
│   │   ├── integrations/
│   │   ├── outbox/
│   │   ├── persistence/
│   │   └── system/
│   ├── migrations/
│   ├── tests/
│   ├── pyproject.toml
│   └── alembic.ini
├── frontend/
│   ├── src/app/core/
│   ├── src/app/shared/
│   ├── src/app/features/
│   └── package.json
├── firmware/
│   ├── esp32-gateway/
│   ├── esp32-node/
│   ├── espnow-common/
│   └── protocol/
├── deployment/
│   ├── systemd/
│   ├── containers/
│   ├── udev/
│   ├── scripts/
│   └── config/
├── tests/
│   ├── integration/
│   ├── hardware-in-loop/
│   └── performance/
└── .github/
    └── workflows/
```

## Boundary rules

- `backend/app/gateway` owns USB serial lifecycle and gateway health.
- `backend/app/transports/esp_now` owns protocol parsing and transport mapping, not automation policy.
- `devices` owns registry and onboarding state.
- `commands` owns authorization and command lifecycle.
- `automation` calls the command service; it must not write GPIO/radio frames directly.
- Firmware shares protocol definitions only through a deliberately versioned source/generation process; avoid copying inconsistent schemas by hand.
- Frontend calls backend APIs only.
- Secrets, local databases, generated firmware binaries, and runtime logs must be excluded from Git.
