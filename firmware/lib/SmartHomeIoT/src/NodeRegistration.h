
#pragma once

#include <Arduino.h>

#define SH_PROTOCOL_VERSION 1
#define SH_MAX_COMPONENTS 8
#define SH_ESPNOW_CHANNEL 6

#define SH_MSG_NODE_HELLO   1
#define SH_MSG_REGISTER_ACK 2

enum ComponentType : uint8_t {
    COMPONENT_UNKNOWN = 0,
    COMPONENT_WATER_FLOW = 1,
    COMPONENT_WATER_LEVEL = 2,
    COMPONENT_RELAY = 3,
    COMPONENT_LDR = 4,
    COMPONENT_TEMPERATURE = 5,
    COMPONENT_HUMIDITY = 6
};

enum ComponentDirection : uint8_t {
    DIRECTION_INPUT = 1,
    DIRECTION_OUTPUT = 2,
    DIRECTION_BIDIRECTIONAL = 3
};

struct __attribute__((packed)) NodeComponent {
    char id[12];
    uint8_t type;
    uint8_t direction;
};

struct __attribute__((packed)) NodeHello {
    uint8_t version;
    uint8_t messageType;
    char deviceName[24];

    uint8_t componentCount;
    NodeComponent components[SH_MAX_COMPONENTS];
};

struct __attribute__((packed)) RegisterAck {
    uint8_t version;
    uint8_t messageType;
    uint8_t accepted;
    char nodeId[24];
};
