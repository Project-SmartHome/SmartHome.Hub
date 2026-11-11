
#pragma once

#include <stdint.h>

enum class RegistrationState : uint8_t {
    Unknown,
    Pending,
    Registered,
    Rejected
};

enum class NodeEventType : uint8_t {
    GatewayDiscovered,
    RegistrationRequested,
    RegistrationApproved,
    RegistrationRejected,
    ConfigurationUpdated,
    HeartbeatReceived
};

struct NodeConfiguration {
    char deviceId[37];
    char homeId[37];
    char roomId[37];
    bool registered;
};

struct NodeEvent {
    NodeEventType type;
    char deviceId[37];
};

struct NodeInfo {
    char deviceId[37];
    char macAddress[18];
    char firmwareVersion[16];
};
