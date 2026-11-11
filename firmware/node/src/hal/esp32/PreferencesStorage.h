
#pragma once

#include "../INodeStorage.h"
#include <Preferences.h>

class PreferencesStorage : public INodeStorage {
public:
    bool begin() override;

    bool load(
        NodeConfiguration& config
    ) override;

    bool save(
        const NodeConfiguration& config
    ) override;

private:
    Preferences preferences;
};
