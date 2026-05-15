#!/usr/bin/env bash

set -euo pipefail

APP_NAME="SmartHome.Hub"
LABEL="com.project-smarthome.hub"
INSTALL_DIR="/Applications/${APP_NAME}"
BIN_NAME="smarthome-hub"
CLI_LINK="/usr/local/bin/${BIN_NAME}"
APP_SUPPORT_DIR="/Library/Application Support/${APP_NAME}"
CACHE_DIR="/Library/Caches/${APP_NAME}"
LOG_DIR="/Library/Logs/${APP_NAME}"
PREF_FILE="/Library/Preferences/com.project-smarthome.SmartHomeHub.plist"
PLIST_PATH="/Library/LaunchAgents/${LABEL}.plist"
PKG_ID="com.project-smarthome.hub"

REMOVE_DATA=1
YES=0

usage() {
  cat <<EOF
SmartHome Hub macOS uninstaller

Usage:
  ./scripts/uninstall-macos.sh [options]

Options:
  --keep-data       Remove app files but keep Application Support data.
  -y, --yes         Do not ask for confirmation.
  -h, --help        Show this help.

Default behavior removes app files and data, including:
  ${APP_SUPPORT_DIR}

EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep-data)
      REMOVE_DATA=0
      shift
      ;;
    -y|--yes)
      YES=1
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
  echo "This uninstaller is for macOS only." >&2
  exit 1
fi

echo "SmartHome Hub uninstall"
echo ""
echo "Will remove:"
echo "  ${INSTALL_DIR}"
echo "  ${CLI_LINK}"
echo "  ${PLIST_PATH}"
if [[ "$REMOVE_DATA" == "1" ]]; then
  echo "  ${APP_SUPPORT_DIR}"
  echo "  ${CACHE_DIR}"
  echo "  ${LOG_DIR}"
  echo "  ${PREF_FILE}"
else
  echo ""
  echo "Will keep data:"
  echo "  ${APP_SUPPORT_DIR}"
fi
echo ""

if [[ "$YES" != "1" ]]; then
  read -r -p "Continue? [y/N] " reply
  case "$reply" in
    [Yy]|[Yy][Ee][Ss])
      ;;
    *)
      echo "Uninstall cancelled."
      exit 0
      ;;
  esac
fi

remove_path() {
  local path="$1"

  if [[ ! -e "$path" && ! -L "$path" ]]; then
    echo "skip: ${path}"
    return
  fi

  rm -rf "$path"
  echo "removed: ${path}"
}

if [[ -f "$PLIST_PATH" ]]; then
  launchctl bootout "gui/$(id -u)" "$PLIST_PATH" >/dev/null 2>&1 || true
fi

remove_path "$PLIST_PATH"
remove_path "$CLI_LINK"
remove_path "$INSTALL_DIR"

if [[ "$REMOVE_DATA" == "1" ]]; then
  remove_path "$APP_SUPPORT_DIR"
  remove_path "$CACHE_DIR"
  remove_path "$LOG_DIR"
  remove_path "$PREF_FILE"
fi

if command -v pkgutil >/dev/null 2>&1; then
  if pkgutil --pkg-info "$PKG_ID" >/dev/null 2>&1; then
    pkgutil --forget "$PKG_ID" >/dev/null 2>&1 || true
    echo "forgot package: ${PKG_ID}"
  fi
fi

echo ""
echo "SmartHome Hub uninstall complete."
