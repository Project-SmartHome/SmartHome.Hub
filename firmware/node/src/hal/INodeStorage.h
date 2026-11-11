
#pragma once

#include "node_types.h"

class INodeStorage {
public:
    virtual bool begin() = 0;

    virtual bool load(
        NodeConfiguration& config
    ) = 0;

    virtual bool save(
        const NodeConfiguration& config
    ) = 0;

    virtual ~INodeStorage() = default;
};
