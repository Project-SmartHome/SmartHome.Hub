#!/usr/bin/env python3
"""Collects built firmware images into release assets plus firmware-manifest.json.

Run from the repository root after building every target with
SH_FIRMWARE_VERSION set, e.g.:

    python firmware/scripts/package_firmware.py \
        --version 1.4.2 --repo Project-SmartHome/SmartHome.Hub --out dist/firmware

Produces dist/firmware/<target>.bin for every target and a manifest that
devices fetch from releases/latest/download/firmware-manifest.json:

    {"schema": 1, "version": "1.4.2", "tag": "v1.4.2", "published": "...",
     "targets": {"gateway-esp32dev": {"version": "1.4.2", "url": "...",
                                      "size": 912345, "md5": "...", "sha256": "..."}}}
"""

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import shutil
import sys

FIRMWARE_ROOT = pathlib.Path(__file__).resolve().parent.parent

# (project directory, PROGNAME / custom_firmware_name, PlatformIO environments)
# Simulator environments (wokwi) are deliberately not published.
TARGETS = [
    ("gateway", "gateway", ["esp32dev"]),
    ("node", "node", ["esp32dev", "nodemcuv2", "esp8266"]),
    ("room-nodes", "room-node", ["esp32dev", "nodemcuv2", "esp8266"]),
]


def digest(path, algorithm):
    hasher = hashlib.new(algorithm)
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            hasher.update(block)
    return hasher.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", required=True, help="release version, X.Y.Z")
    parser.add_argument("--repo", required=True, help="GitHub owner/name")
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()

    version = args.version[1:] if args.version.startswith("v") else args.version
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        sys.exit("version must be X.Y.Z, got %r" % args.version)
    tag = "v" + version

    args.out.mkdir(parents=True, exist_ok=True)
    targets = {}
    missing = []

    for project, name, environments in TARGETS:
        for environment in environments:
            built = FIRMWARE_ROOT / project / ".pio" / "build" / environment / (name + ".bin")
            target = "%s-%s" % (name, environment)
            if not built.is_file():
                missing.append(str(built))
                continue

            asset = args.out / (target + ".bin")
            shutil.copyfile(built, asset)

            # The stamped version string (a NUL-terminated literal, not part of
            # a longer version) must be inside the image; otherwise a stale or
            # mis-stamped build would be published under this release.
            stamp = re.compile(rb"(?<![0-9A-Za-z.+])" + re.escape(version.encode()) + rb"\x00")
            if not stamp.search(asset.read_bytes()):
                sys.exit("%s does not contain version %s; was it built with "
                         "SH_FIRMWARE_VERSION=%s?" % (built, version, version))

            targets[target] = {
                "version": version,
                "url": "https://github.com/%s/releases/download/%s/%s" % (args.repo, tag, asset.name),
                "size": asset.stat().st_size,
                "md5": digest(asset, "md5"),
                "sha256": digest(asset, "sha256"),
            }
            print("%-24s %8d bytes  md5 %s" % (target, targets[target]["size"], targets[target]["md5"]))

    if missing:
        sys.exit("missing firmware images:\n  " + "\n  ".join(missing))

    manifest = {
        "schema": 1,
        "version": version,
        "tag": tag,
        "published": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "targets": targets,
    }
    manifest_path = args.out / "firmware-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print("Wrote %s (%d targets)" % (manifest_path, len(targets)))


if __name__ == "__main__":
    main()
