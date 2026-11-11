
#pragma once

#include "../INodeTransport.h"
#include "node_types.h"

class NodeIdentity {
public:
    static void generate(
        const MacAddress& mac,
        DeviceIdentity& identity,
        const char* firmwareVersion
    );
};
