
#include "PreferencesStorage.h"

#include <string.h>

bool PreferencesStorage::begin() {
    return preferences.begin("smarthome", false);
}

bool PreferencesStorage::load(
    NodeConfiguration& config
) {

    size_t length = preferences.getBytesLength("config");

    if (length != sizeof(NodeConfiguration)) {
        return false;
    }

    return preferences.getBytes(
        "config",
        &config,
        sizeof(NodeConfiguration)
    ) == sizeof(NodeConfiguration);
}

bool PreferencesStorage::save(
    const NodeConfiguration& config
) {

    return preferences.putBytes(
        "config",
        &config,
        sizeof(NodeConfiguration)
    ) == sizeof(NodeConfiguration);
}
