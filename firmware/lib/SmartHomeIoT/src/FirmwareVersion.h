#pragma once

// Semantic-version parsing and ordering for firmware updates.
// Pure C++ with no Arduino dependencies, so it can be unit-tested on a host.
//
// Accepted forms: "1.2.3", "v1.2.3", "1.2.3+dev", "1.2.3-rc.1".
// Only MAJOR.MINOR.PATCH takes part in ordering; any "-..." or "+..." suffix
// is ignored. Local builds are stamped "0.0.0+dev", older than any release.

#include <stdint.h>

namespace ShFirmware {

struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    bool valid = false;
};

inline Version parseVersion(const char* text) {
    Version version;
    if (text == nullptr) return version;
    if (*text == 'v' || *text == 'V') ++text;

    uint32_t parts[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        if (*text < '0' || *text > '9') return version;
        uint32_t value = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + static_cast<uint32_t>(*text - '0');
            if (value > 999999) return version;
            ++text;
        }
        parts[i] = value;
        if (i < 2) {
            if (*text != '.') return version;
            ++text;
        }
    }
    if (*text != '\0' && *text != '-' && *text != '+') return version;

    version.major = parts[0];
    version.minor = parts[1];
    version.patch = parts[2];
    version.valid = true;
    return version;
}

// <0 when a < b, 0 when equal, >0 when a > b.
inline int compareVersions(const Version& a, const Version& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

// True when `candidate` is a valid version strictly newer than `current`.
// An unparsable `current` counts as 0.0.0, so such a device always updates.
inline bool isNewer(const char* candidate, const char* current) {
    const Version next = parseVersion(candidate);
    if (!next.valid) return false;
    return compareVersions(next, parseVersion(current)) > 0;
}

} // namespace ShFirmware
