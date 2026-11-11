
#include "HardwareFactory.h"

#if defined(SH_NODE_ESP8266)

#include "esp8266/EspNowTransport.h"
#include "esp8266/EepromStorage.h"
#include "esp8266/Gpio.h"

static EspNowTransport nodeTransport;
static EepromStorage nodeStorage;
static Gpio nodeGpio;

#elif defined(SH_NODE_ESP32)

#include "esp32/EspNowTransport.h"
#include "esp32/PreferencesStorage.h"
#include "esp32/Gpio.h"

static EspNowTransport nodeTransport;
static PreferencesStorage nodeStorage;
static Gpio nodeGpio;

#else

#error "Select SH_NODE_ESP8266 or SH_NODE_ESP32"

#endif

void HardwareFactory::begin() {

    nodeStorage.begin();

    nodeTransport.begin(1);
}

INodeTransport& HardwareFactory::transport() {
    return nodeTransport;
}

INodeStorage& HardwareFactory::storage() {
    return nodeStorage;
}

INodeGpio& HardwareFactory::gpio() {
    return nodeGpio;
}

void HardwareFactory::update() {
    nodeTransport.update();
}
