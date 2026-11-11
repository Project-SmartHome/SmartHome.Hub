#pragma once

#include <stdint.h>
#include <stddef.h>
#include "node_types.h"
#include "../hal/INodeTransport.h"
#include "../hal/INodeStorage.h"

class RegistrationManager
{
public:
    RegistrationManager(
        INodeTransport& transport,
        INodeStorage& storage
    );

    bool begin();
    void requestRegistration(const NodeInfo& info);
    void update();

    void onRegistrationResponse(
        const char* status,
        const char* homeId,
        const char* roomId
    );

    RegistrationState getState() const;

    // Used by the OTA receiver: firmware is only accepted from the gateway
    // this node discovered, and OTA traffic counts as the gateway replying.
    bool hasGateway() const { return gatewayDiscovered; }
    const MacAddress& gatewayAddress() const { return gatewayMac; }
    void notifyGatewayActivity() { onGatewayReply(); }

    // True once the gateway has answered at least once since boot (used to
    // confirm a freshly installed image on ESP32).
    bool gatewayHeard() const { return heardFromGateway; }

private:
    static void receiveCallback(
        const MacAddress& sender,
        const NodeFrame& frame
    );

    void handleFrame(
        const MacAddress& mac,
        const NodeFrame& frame
    );

    void sendDiscovery();
    void sendRegistrationRequest();
    void sendHeartbeat();
    void sendToGateway(const NodeFrame& frame);
    void fillIdentity(NodeFrame& frame) const;
    bool noteUnansweredSend();
    void onGatewayReply();
    void restartDiscovery();

    bool loadConfiguration();
    bool saveConfiguration();
    void initializeIdentity();

    INodeTransport& transport;
    INodeStorage& storage;

    NodeConfiguration config{};
    RegistrationState state = RegistrationState::Unknown;

    MacAddress gatewayMac{};
    bool gatewayDiscovered = false;
    uint8_t scanChannel = 1;

    NodeInfo nodeInfo{};

    NodeFrame receivedFrame{};
    MacAddress receivedMac{};
    volatile bool frameReceived = false;

    uint16_t sequence = 0;
    uint32_t lastDiscoveryAt = 0;
    uint32_t lastChannelChangeAt = 0;
    uint32_t lastRegistrationAt = 0;
    uint32_t lastHeartbeatAt = 0;
    // Node-to-gateway frames are broadcast (unicast to the gateway is not
    // acknowledged on this hardware), so a reply from the gateway is the only
    // proof of delivery.
    bool awaitingReply = false;
    uint8_t unansweredSends = 0;
    bool heardFromGateway = false;

    static RegistrationManager* instance;
};