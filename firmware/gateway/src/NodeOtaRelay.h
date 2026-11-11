#pragma once

// Updates ESP-NOW nodes, which have no internet access of their own.
//
// The gateway learns each node's firmware target and version from its
// heartbeats / registration requests. When the release manifest has a newer
// image for a node's target, the gateway downloads that image once into
// LittleFS, then streams it to each node over ESP-NOW (see OtaProtocol.h).
// One node is updated at a time; failures back off per node.

#include <Arduino.h>
#include <FS.h>
#include <OtaHttp.h>
#include <OtaProtocol.h>

class NodeOtaRelay {
public:
    using SendFunction = bool (*)(const uint8_t* mac, const uint8_t* data, size_t length);

    static constexpr uint8_t MAX_NODES = 16;
    static constexpr uint8_t MAX_RELEASES = 6;

    void begin(SendFunction send);

    // Replaces the known node releases (called after each manifest check).
    void clearReleases();
    void addRelease(const ShOta::Release& release);

    // Records a node seen on ESP-NOW. Nodes that predate OTA report no target
    // and are ignored (they need one USB flash with OTA-capable firmware).
    void noteNode(const uint8_t* mac, const char* deviceId,
                  const char* target, const char* version);

    void onPacket(const uint8_t* mac, const OtaPacket& packet);

    // Drives the active transfer, or starts the next one. May block for a
    // while to download a node image, which only happens when networkReady.
    void loop(bool networkReady);

    bool busy() const { return phase != Phase::Idle; }
    void abortTransfer();

private:
    enum class Phase : uint8_t { Idle, Offering, Sending, Finishing };

    struct NodeRecord {
        bool used = false;
        uint8_t mac[6] = {};
        char deviceId[37] = {};
        char target[SH_OTA_TARGET_LENGTH] = {};
        char version[SH_OTA_VERSION_LENGTH] = {};
        char skipVersion[SH_OTA_VERSION_LENGTH] = {};  // never offer this version again
        uint32_t lastSeenAt = 0;
        uint32_t retryAt = 0;
        bool waitRetry = false;
        uint8_t failures = 0;
    };

    struct ReleaseSlot {
        ShOta::Release release;
        uint32_t retryAt = 0;      // after a failed download
        bool waitRetry = false;
    };

    struct CachedImage {
        bool valid = false;
        char target[SH_OTA_TARGET_LENGTH] = {};
        char version[SH_OTA_VERSION_LENGTH] = {};
        char md5[SH_OTA_MD5_LENGTH] = {};
        uint32_t size = 0;
    };

    NodeRecord* findNode(const uint8_t* mac);
    ReleaseSlot* findRelease(const char* target);
    bool nodeNeedsUpdate(const NodeRecord& node, uint32_t now);
    void scheduleNext(bool networkReady, uint32_t now);
    void start(NodeRecord& node, const ShOta::Release& release);
    void fail(OtaStatus status, bool skipVersion);
    void finish();

    bool ensureFilesystem();
    void loadCacheMeta();
    bool cacheMatches(const ShOta::Release& release) const;
    bool downloadImage(const ShOta::Release& release);

    void sendOffer();
    void sendChunk();
    void sendControl(OtaMessage type, OtaStatus status);
    bool transmit(const OtaPacket& packet);

    SendFunction send = nullptr;
    NodeRecord nodes[MAX_NODES];
    ReleaseSlot releases[MAX_RELEASES];
    CachedImage cache;
    bool filesystemReady = false;
    bool filesystemFailed = false;

    Phase phase = Phase::Idle;
    NodeRecord* node = nullptr;
    File image;
    uint32_t session = 0;
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t lastSendAt = 0;
    uint32_t startedAt = 0;
    uint32_t lastScheduleAt = 0;
    uint16_t attempts = 0;
    uint8_t lastDecile = 0;
    char version[SH_OTA_VERSION_LENGTH] = {};
};
