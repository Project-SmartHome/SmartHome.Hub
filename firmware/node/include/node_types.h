
#pragma once

#include <stdint.h>

#define SH_PROTOCOL_MAGIC 0x5348
#define SH_PROTOCOL_VERSION 1
#define SH_MAX_PAYLOAD 32

enum class MessageType : uint8_t {
    Discovery = 1,
    GatewayHello = 2,
    RegisterRequest = 3,
    RegisterPending = 4,
    Registered = 5,
    RegisterRejected = 6,
    ConfigUpdate = 7,
    Heartbeat = 8,
    HeartbeatAck = 9,
    Command = 10,
    Telemetry = 11
};

enum class RegistrationState : uint8_t {
    Unknown,
    Pending,
    Registered,
    Rejected
};

#pragma pack(push, 1)

struct NodeFrame {
    uint16_t magic;
    uint8_t protocolVersion;
    uint8_t messageType;
    uint16_t sequence;

    char deviceId[37];
    char homeId[37];
    char roomId[37];

    char firmwareVersion[16];
    char macAddress[18];

    uint8_t payloadLength;
    uint8_t payload[SH_MAX_PAYLOAD];
};

#pragma pack(pop)

struct NodeConfiguration {
    char deviceId[37];
    char macAddress[18];
    char firmwareVersion[16];
    char homeId[37];
    char roomId[37];

    bool registered;
    uint32_t configVersion;
};

struct DeviceIdentity {
    char deviceId[37];
    char macAddress[18];
    char firmwareVersion[16];
};

struct MacAddress {
    uint8_t bytes[6];
};

struct NodeInfo
{
    char deviceId[37] = {0};
    char macAddress[18] = {0};
    char firmwareVersion[16] = {0};

    char homeId[37] = {0};
    char roomId[37] = {0};

    bool registered = false;
};