
#include "EepromStorage.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>
#include <string.h>

#define EEPROM_SIZE 256
#define STORAGE_MAGIC 0x53484E31

#pragma pack(push, 1)

struct StoredConfiguration {
    uint32_t magic;
    NodeConfiguration config;
    uint32_t checksum;
};

#pragma pack(pop)

static uint32_t calculateChecksum(
    const uint8_t* data,
    size_t length
) {
    uint32_t hash = 2166136261UL;

    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619UL;
    }

    return hash;
}

bool EepromStorage::begin() {
    EEPROM.begin(EEPROM_SIZE);
    return true;
}

bool EepromStorage::load(
    NodeConfiguration& config
) {

    StoredConfiguration stored{};

    EEPROM.get(0, stored);

    if (stored.magic != STORAGE_MAGIC) {
        return false;
    }

    uint32_t checksum = calculateChecksum(
        reinterpret_cast<const uint8_t*>(&stored),
        offsetof(StoredConfiguration, checksum)
    );

    if (checksum != stored.checksum) {
        return false;
    }

    config = stored.config;

    return true;
}

bool EepromStorage::save(
    const NodeConfiguration& config
) {

    StoredConfiguration stored{};

    stored.magic = STORAGE_MAGIC;
    stored.config = config;

    stored.checksum = calculateChecksum(
        reinterpret_cast<const uint8_t*>(&stored),
        offsetof(StoredConfiguration, checksum)
    );

    EEPROM.put(0, stored);

    return EEPROM.commit();
}
