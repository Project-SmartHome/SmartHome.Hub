#pragma once

// Receives firmware from the gateway over ESP-NOW (see OtaProtocol.h) and
// installs it with the platform Updater, then reboots into it.
//
// Safety checks before anything is written:
//   - the offer must come from the gateway this node discovered,
//   - its target must equal this build's SH_FIRMWARE_TARGET (board + firmware),
//   - its version must be newer than the running one and not previously
//     rolled back (ESP32),
//   - the image's MD5 is verified by the Updater before it is activated.

#include <Arduino.h>
#include <OtaProtocol.h>

#include "RegistrationManager.h"
#include "../hal/INodeTransport.h"

class OtaReceiver {
public:
    OtaReceiver(INodeTransport& transport, RegistrationManager& registration);

    void begin();
    void update();

    // While true, registration traffic (discovery/heartbeats) is paused.
    bool active() const { return running; }

private:
    static void rawCallback(const MacAddress& sender, const uint8_t* data, size_t length);

    void handlePacket(const MacAddress& sender, const OtaPacket& packet);
    void handleOffer(const OtaPacket& packet);
    void handleData(const OtaPacket& packet);
    void finishImage();
    void stop(OtaStatus status, bool notifyGateway);
    void reply(OtaMessage type, uint32_t sessionId, OtaStatus status, uint32_t offset);

    INodeTransport& transport;
    RegistrationManager& registration;

    // Single-slot hand-off from the radio callback to update(). The gateway
    // sends one packet at a time and retransmits, so a dropped packet only
    // costs a resend.
    OtaPacket incoming{};
    MacAddress incomingFrom{};
    volatile bool incomingReady = false;

    bool running = false;
    uint32_t session = 0;
    uint32_t imageSize = 0;
    uint32_t received = 0;
    uint32_t lastPacketAt = 0;
    uint8_t lastDecile = 0;
    char version[SH_OTA_VERSION_LENGTH] = {};

    static OtaReceiver* instance;
};
