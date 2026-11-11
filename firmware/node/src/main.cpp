#include <Arduino.h>

#include <OtaConfig.h>
#include <OtaRollback.h>

#include "core/OtaReceiver.h"
#include "core/RegistrationManager.h"
#include "hal/HardwareFactory.h"

#if defined(ESP32)
// Leave the post-OTA "is this image good?" decision to ShOtaRollback: a new
// image is confirmed once the gateway answers, or rolled back if it never does.
extern "C" bool verifyRollbackLater() {
    return true;
}
#endif

// Hardware implementations
INodeTransport& transport = HardwareFactory::transport();
INodeStorage& storage = HardwareFactory::storage();

// Registration manager
RegistrationManager registrationManager(transport, storage);

// Firmware updates relayed by the gateway over ESP-NOW
OtaReceiver otaReceiver(transport, registrationManager);

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("SmartHome Node starting...");
    Serial.printf("Firmware %s (%s)\n", SH_FIRMWARE_VERSION, SH_FIRMWARE_TARGET);
    ShOtaRollback::begin(SH_FIRMWARE_VERSION);

    if (!registrationManager.begin())
    {
        Serial.println("Registration initialization failed");
    }
    otaReceiver.begin();
}

void loop()
{
    otaReceiver.update();

    // Registration traffic (and its channel scanning) would disrupt a
    // firmware transfer, so it waits until the transfer ends.
    if (!otaReceiver.active())
    {
        registrationManager.update();
    }

    if (registrationManager.gatewayHeard())
    {
        ShOtaRollback::markHealthy();
    }
    ShOtaRollback::loop();

    delay(otaReceiver.active() ? 1 : 10);
}
