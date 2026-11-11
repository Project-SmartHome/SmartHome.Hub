#pragma once

// ESP-NOW firmware transfer between the gateway and ESP-NOW nodes.
// Pure C++ (no Arduino dependencies) so it is shared by the gateway, the
// node and host-side tests.
//
// OTA packets are told apart from registration NodeFrames by their magic
// (SH_OTA_MAGIC vs SH_PROTOCOL_MAGIC) and are variable-length:
// sizeof(OtaHeader) + header.length bytes.
//
// Direction and delivery: the gateway unicasts Offer/Data/Abort (MAC-layer
// acknowledged); the node broadcasts Accept/Reject/Ack/Complete, because
// unicast from node to gateway is not acknowledged on this hardware (see
// RegistrationManager::sendToGateway). Every message carries the session id,
// so stray broadcasts from other nodes or stale sessions are ignored.
//
// Flow (stop-and-wait, gateway drives retries):
//
//   gateway                         node
//   Offer(size, target, ver, md5) ->
//                                <- Accept(offset 0) | Reject(status)
//   Data(offset, bytes)          ->
//                                <- Ack(next expected offset)
//   ... repeat until next expected offset == size ...
//                                <- Complete(status)   (then the node reboots)
//
// A duplicate Data is acknowledged again without being rewritten; the node
// always reports the next offset it needs, so the gateway resumes from there.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SH_OTA_MAGIC 0x4F53            // "SO" little-endian; distinct from SH_PROTOCOL_MAGIC
#define SH_OTA_PROTOCOL_VERSION 1
#define SH_OTA_CHUNK_SIZE 200          // header + chunk stays under ESP-NOW's 250-byte limit
#define SH_OTA_TARGET_LENGTH 24
#define SH_OTA_VERSION_LENGTH 16
#define SH_OTA_MD5_LENGTH 33           // 32 hex digits + NUL

enum class OtaMessage : uint8_t {
    Offer = 1,     // gateway -> node: an image is available (data = OtaOffer)
    Accept = 2,    // node -> gateway: ready to receive from header.offset
    Reject = 3,    // node -> gateway: will not install (header.status says why)
    Data = 4,      // gateway -> node: image bytes at header.offset
    Ack = 5,       // node -> gateway: header.offset = next byte expected
    Complete = 6,  // node -> gateway: image fully received; status Ok = verified
    Abort = 7      // either way: cancel the session
};

enum class OtaStatus : uint8_t {
    Ok = 0,
    WrongTarget = 1,   // image is for a different board/firmware
    NotNewer = 2,      // node already runs this version or newer
    BadVersion = 3,    // this version previously failed its health check
    BeginFailed = 4,   // no room in the OTA partition / flash
    WriteFailed = 5,
    VerifyFailed = 6,  // MD5 mismatch or image rejected by the updater
    Busy = 7,
    Timeout = 8,
    Aborted = 9,
    InvalidOffer = 10
};

#pragma pack(push, 1)
struct OtaHeader {
    uint16_t magic;     // SH_OTA_MAGIC
    uint8_t version;    // SH_OTA_PROTOCOL_VERSION
    uint8_t type;       // OtaMessage
    uint32_t session;   // random per transfer, chosen by the gateway
    uint32_t offset;    // Data: position of the bytes; Accept/Ack: next byte expected
    uint16_t length;    // bytes used in OtaPacket::data
    uint8_t status;     // OtaStatus (Reject/Complete/Abort)
};

struct OtaOffer {
    uint32_t size;                           // image size in bytes
    char target[SH_OTA_TARGET_LENGTH];       // must equal the node's SH_FIRMWARE_TARGET
    char version[SH_OTA_VERSION_LENGTH];
    char md5[SH_OTA_MD5_LENGTH];             // lowercase hex
};

struct OtaPacket {
    OtaHeader header;
    uint8_t data[SH_OTA_CHUNK_SIZE];
};
#pragma pack(pop)

static_assert(sizeof(OtaPacket) <= 250, "OTA packet exceeds ESP-NOW payload limit");
static_assert(sizeof(OtaOffer) <= SH_OTA_CHUNK_SIZE, "OtaOffer must fit in one packet");

namespace ShOtaProtocol {

inline size_t packetSize(const OtaPacket& packet) {
    return sizeof(OtaHeader) + packet.header.length;
}

inline void initPacket(OtaPacket& packet, OtaMessage type, uint32_t session,
                       uint32_t offset = 0, OtaStatus status = OtaStatus::Ok) {
    memset(&packet, 0, sizeof(packet));
    packet.header.magic = SH_OTA_MAGIC;
    packet.header.version = SH_OTA_PROTOCOL_VERSION;
    packet.header.type = static_cast<uint8_t>(type);
    packet.header.session = session;
    packet.header.offset = offset;
    packet.header.status = static_cast<uint8_t>(status);
}

// True when the first bytes look like an OTA packet (cheap check used to
// route raw ESP-NOW frames before copying them).
inline bool isOtaPacket(const uint8_t* data, size_t length) {
    if (data == nullptr || length < sizeof(OtaHeader)) return false;
    uint16_t magic = 0;
    memcpy(&magic, data, sizeof(magic));
    return magic == SH_OTA_MAGIC;
}

// Copies and validates a received frame. Returns false for anything that is
// not a well-formed packet of this protocol version.
inline bool parsePacket(const uint8_t* data, size_t length, OtaPacket& out) {
    if (!isOtaPacket(data, length) || length > sizeof(OtaPacket)) return false;
    memset(&out, 0, sizeof(out));
    memcpy(&out, data, length);
    if (out.header.version != SH_OTA_PROTOCOL_VERSION) return false;
    if (out.header.length > SH_OTA_CHUNK_SIZE) return false;
    if (sizeof(OtaHeader) + out.header.length != length) return false;
    return true;
}

inline bool isMd5Hex(const char* text) {
    if (text == nullptr || strlen(text) != 32) return false;
    for (size_t i = 0; i < 32; ++i) {
        const char c = text[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

inline const char* statusName(OtaStatus status) {
    switch (status) {
        case OtaStatus::Ok: return "ok";
        case OtaStatus::WrongTarget: return "wrong target";
        case OtaStatus::NotNewer: return "not newer";
        case OtaStatus::BadVersion: return "version previously failed";
        case OtaStatus::BeginFailed: return "no space for image";
        case OtaStatus::WriteFailed: return "flash write failed";
        case OtaStatus::VerifyFailed: return "verification failed";
        case OtaStatus::Busy: return "busy";
        case OtaStatus::Timeout: return "timeout";
        case OtaStatus::Aborted: return "aborted";
        case OtaStatus::InvalidOffer: return "invalid offer";
    }
    return "unknown";
}

} // namespace ShOtaProtocol
