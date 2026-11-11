
#pragma once

#include "../INodeStorage.h"

class EepromStorage : public INodeStorage {
public:
    bool begin() override;

    bool load(
        NodeConfiguration& config
    ) override;

    bool save(
        const NodeConfiguration& config
    ) override;
};
