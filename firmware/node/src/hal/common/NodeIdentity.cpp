
#include "NodeIdentity.h"
#include <stdio.h>
#include <string.h>

void NodeIdentity::generate(
    const MacAddress& mac,
    DeviceIdentity& identity,
    const char* firmwareVersion
) {
    const uint8_t* m = mac.bytes;

    // Stable UUID-like identifier derived from hardware MAC.
    snprintf(
        identity.deviceId,
        sizeof(identity.deviceId),
        "%02x%02x%02x%02x-%02x%02x-4%02x-a%02x-%02x%02x%02x%02x%02x%02x",
        m[0], m[1], m[2], m[3],
        m[4], m[5],
        m[0] & 0x0f,
        m[1] & 0x0f,
        m[0], m[1], m[2],
        m[3], m[4], m[5]
    );

    snprintf(
        identity.macAddress,
        sizeof(identity.macAddress),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        m[0], m[1], m[2],
        m[3], m[4], m[5]
    );

    strncpy(
        identity.firmwareVersion,
        firmwareVersion,
        sizeof(identity.firmwareVersion) - 1
    );

    identity.firmwareVersion[
        sizeof(identity.firmwareVersion) - 1
    ] = '\0';
}
