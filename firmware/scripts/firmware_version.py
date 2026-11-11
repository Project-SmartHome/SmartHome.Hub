# PlatformIO pre-script: stamps every build with its firmware version and
# OTA target, e.g. SH_FIRMWARE_VERSION="1.4.2", SH_FIRMWARE_TARGET="node-nodemcuv2".
#
# Version:
#   - CI release builds set SH_FIRMWARE_VERSION=X.Y.Z in the environment.
#   - Every other build is "0.0.0+dev", which is older than any release, so a
#     USB-flashed device moves onto the latest release at its first update
#     check. (Git tags are deliberately not used: local-only tags would make a
#     dev build look newer than the real releases and block its updates.)
#     To keep a dev build on a device, build it with OTA disabled:
#         PLATFORMIO_BUILD_FLAGS=-DSH_OTA_DISABLED pio run -t upload
#
# Target: "<custom_firmware_name>-<pio env>" (custom_firmware_name is set in
# each project's platformio.ini). The release manifest is keyed by it.
#
# SH_OTA_MANIFEST_URL in the environment overrides the manifest location
# (CI sets it from the repository name).

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

import os
import re

SEMVER = re.compile(r"^\d+\.\d+\.\d+$")


def quoted(value):
    return '\\"%s\\"' % value


version = os.environ.get("SH_FIRMWARE_VERSION", "").strip()
if version.startswith("v"):
    version = version[1:]
if version:
    if not SEMVER.match(version):
        raise SystemExit("SH_FIRMWARE_VERSION must be X.Y.Z, got %r" % version)
else:
    version = "0.0.0+dev"

if len(version) > 15:
    raise SystemExit("Firmware version %r is longer than 15 characters" % version)

name = env.GetProjectOption("custom_firmware_name")  # noqa: F821
target = "%s-%s" % (name, env["PIOENV"])  # noqa: F821
if len(target) > 23:
    raise SystemExit("OTA target %r is longer than 23 characters" % target)

defines = [
    ("SH_FIRMWARE_VERSION", quoted(version)),
    ("SH_FIRMWARE_TARGET", quoted(target)),
]
manifest_url = os.environ.get("SH_OTA_MANIFEST_URL", "").strip()
if manifest_url:
    defines.append(("SH_OTA_MANIFEST_URL", quoted(manifest_url)))

env.Append(CPPDEFINES=defines)  # noqa: F821
print("Firmware %s version %s" % (target, version))
