#!/usr/bin/env bash

set -euo pipefail

APP_NAME="SmartHome.Hub"
LABEL="com.project-smarthome.hub"
INSTALL_DIR="${HOME}/Applications/${APP_NAME}"
BIN_NAME="smarthome-hub"
BIN_PATH="${INSTALL_DIR}/${BIN_NAME}"
CLI_DIR="${HOME}/.local/bin"
CLI_LINK="${CLI_DIR}/${BIN_NAME}"
APP_SUPPORT_DIR="${HOME}/Library/Application Support/${APP_NAME}"
LAUNCH_AGENTS_DIR="${HOME}/Library/LaunchAgents"
PLIST_PATH="${LAUNCH_AGENTS_DIR}/${LABEL}.plist"

SOURCE_BINARY=""
INSTALL_SERVICE=1
START_SERVICE=1
BUILD_IF_MISSING=1

usage() {
  cat <<EOF
SmartHome Hub macOS installer

Usage:
  ./scripts/install-macos.sh [options]

Options:
  --binary PATH       Install this binary instead of auto-detecting one.
  --no-service       Do not install a LaunchAgent.
  --no-start         Install the LaunchAgent but do not start it now.
  --no-build         Do not build backend/build/smarthome-hub if no binary exists.
  -h, --help         Show this help.

Installs:
  App binary:         ${BIN_PATH}
  CLI symlink:        ${CLI_LINK}
  App data directory: ${APP_SUPPORT_DIR}
  LaunchAgent:        ${PLIST_PATH}

EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary)
      SOURCE_BINARY="${2:-}"
      shift 2
      ;;
    --no-service)
      INSTALL_SERVICE=0
      START_SERVICE=0
      shift
      ;;
    --no-start)
      START_SERVICE=0
      shift
      ;;
    --no-build)
      BUILD_IF_MISSING=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This installer is for macOS only." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

detect_binary() {
  local candidates=(
    "${ROOT_DIR}/backend/build/${BIN_NAME}"
    "${ROOT_DIR}/${BIN_NAME}"
    "${ROOT_DIR}/backend/${APP_NAME}"
    "${ROOT_DIR}/backend/cmd/launcher/${APP_NAME}"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done
}

if [[ -z "$SOURCE_BINARY" ]]; then
  SOURCE_BINARY="$(detect_binary || true)"
fi

if [[ -z "$SOURCE_BINARY" && "$BUILD_IF_MISSING" == "1" ]]; then
  echo "No built binary found. Building backend/build/${BIN_NAME}..."
  make -C "${ROOT_DIR}/backend" build
  SOURCE_BINARY="${ROOT_DIR}/backend/build/${BIN_NAME}"
fi

if [[ -z "$SOURCE_BINARY" || ! -f "$SOURCE_BINARY" ]]; then
  echo "No binary found. Run 'make -C backend build' or pass --binary PATH." >&2
  exit 1
fi

mkdir -p "$INSTALL_DIR" "$CLI_DIR" "$APP_SUPPORT_DIR"

if [[ -f "$BIN_PATH" ]]; then
  timestamp="$(date +%Y%m%d%H%M%S)"
  cp "$BIN_PATH" "${BIN_PATH}.bak.${timestamp}"
fi

cp "$SOURCE_BINARY" "$BIN_PATH"
chmod 755 "$BIN_PATH"
ln -sfn "$BIN_PATH" "$CLI_LINK"

echo "Installed binary: ${BIN_PATH}"
echo "Installed CLI link: ${CLI_LINK}"
echo "Application data: ${APP_SUPPORT_DIR}"

if [[ "$INSTALL_SERVICE" == "1" ]]; then
  mkdir -p "$LAUNCH_AGENTS_DIR"
  cat > "$PLIST_PATH" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${LABEL}</string>
  <key>ProgramArguments</key>
  <array>
    <string>${BIN_PATH}</string>
  </array>
  <key>WorkingDirectory</key>
  <string>${INSTALL_DIR}</string>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>StandardOutPath</key>
  <string>${APP_SUPPORT_DIR}/hub.log</string>
  <key>StandardErrorPath</key>
  <string>${APP_SUPPORT_DIR}/hub.err.log</string>
</dict>
</plist>
EOF

  echo "Installed LaunchAgent: ${PLIST_PATH}"

  if [[ "$START_SERVICE" == "1" ]]; then
    launchctl bootout "gui/$(id -u)" "$PLIST_PATH" >/dev/null 2>&1 || true
    launchctl bootstrap "gui/$(id -u)" "$PLIST_PATH"
    launchctl kickstart -k "gui/$(id -u)/${LABEL}"
    echo "Started LaunchAgent: ${LABEL}"
  fi
fi

echo ""
echo "SmartHome Hub installation complete."
echo "Open http://localhost:8080 after the service starts."
