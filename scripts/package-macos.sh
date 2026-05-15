#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/backend/build"
PKG_PATH="${BUILD_DIR}/SmartHome.Hub.pkg"
BIN_NAME="smarthome-hub"
INSTALL_ROOT="/Applications/SmartHome.Hub"
LAUNCH_AGENT_DIR="/Library/LaunchAgents"
APP_SUPPORT_DIR="/Library/Application Support/SmartHome.Hub"
PLIST_NAME="com.project-smarthome.hub.plist"
PKG_SCRIPTS_DIR="${SCRIPT_DIR}/pkg-scripts"
VERSION="${VERSION:-1.0.0}"

mkdir -p "${BUILD_DIR}"

find_binary() {
  local candidates=(
    "${ROOT_DIR}/backend/build/${BIN_NAME}"
    "${ROOT_DIR}/backend/${BIN_NAME}"
    "${ROOT_DIR}/backend/cmd/launcher/${BIN_NAME}"
    "${ROOT_DIR}/backend/cmd/launcher/SmartHome.Hub"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done

  return 1
}

SOURCE_BINARY=""
if [[ -n "${SOURCE_BINARY:-}" ]]; then
  SOURCE_BINARY="${SOURCE_BINARY}"
else
  SOURCE_BINARY="$(find_binary || true)"
fi

if [[ -z "$SOURCE_BINARY" ]]; then
  echo "No built binary found. Building backend..."
  (cd "${ROOT_DIR}/backend" && make build)
  SOURCE_BINARY="$(find_binary)"
fi

if [[ -z "$SOURCE_BINARY" || ! -f "$SOURCE_BINARY" ]]; then
  echo "Binary not found. Build backend first or pass SOURCE_BINARY." >&2
  exit 1
fi

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

mkdir -p "${TMP_ROOT}${INSTALL_ROOT}" "${TMP_ROOT}${LAUNCH_AGENT_DIR}" "${TMP_ROOT}${APP_SUPPORT_DIR}" "${TMP_ROOT}/usr/local/bin"
cp "$SOURCE_BINARY" "${TMP_ROOT}${INSTALL_ROOT}/${BIN_NAME}"
chmod 755 "${TMP_ROOT}${INSTALL_ROOT}/${BIN_NAME}"

cat > "${TMP_ROOT}${LAUNCH_AGENT_DIR}/${PLIST_NAME}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.project-smarthome.hub</string>
  <key>ProgramArguments</key>
  <array>
    <string>${INSTALL_ROOT}/${BIN_NAME}</string>
  </array>
  <key>WorkingDirectory</key>
  <string>${INSTALL_ROOT}</string>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>StandardOutPath</key>
  <string>/var/log/SmartHome.Hub.out.log</string>
  <key>StandardErrorPath</key>
  <string>/var/log/SmartHome.Hub.err.log</string>
</dict>
</plist>
EOF

ln -sfn "${INSTALL_ROOT}/${BIN_NAME}" "${TMP_ROOT}/usr/local/bin/${BIN_NAME}"

pkgbuild \
  --root "${TMP_ROOT}" \
  --identifier "com.project-smarthome.hub" \
  --version "${VERSION}" \
  --install-location / \
  --scripts "${PKG_SCRIPTS_DIR}" \
  "${PKG_PATH}"

echo "Built macOS package: ${PKG_PATH}"
