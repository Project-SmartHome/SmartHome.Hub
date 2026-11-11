
#pragma once

// Thin portability layer over the two different ESP-NOW APIs:
//   - ESP32   (esp_now.h, ESP-IDF style, esp_err_t return codes)
//   - ESP8266 (espnow.h, NONOS SDK style, int return codes, roles)
// Also handles the receive-callback signature change in arduino-esp32 3.x.
//
// Supported boards: ESP32 dev boards, NodeMCU (v2/v3), generic ESP8266 (ESP-12E/F).

#include <Arduino.h>

#if defined(ESP32)
    #include <WiFi.h>
    #include <esp_now.h>
    #include <esp_wifi.h>
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <espnow.h>
#else
    #error "SmartHomeIoT ESP-NOW support requires an ESP32 or ESP8266 target"
#endif

namespace ShEspNow {

// Unified receive callback used by application code on every board.
using ReceiveCallback = void (*)(
    const uint8_t* mac,
    const uint8_t* data,
    int length
);

namespace detail {

inline ReceiveCallback& userCallback() {
    static ReceiveCallback callback = nullptr;
    return callback;
}

#if defined(ESP32)
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    inline void onReceive(
        const esp_now_recv_info_t* info,
        const uint8_t* data,
        int length
    ) {
        if (userCallback()) userCallback()(info->src_addr, data, length);
    }
  #else
    inline void onReceive(
        const uint8_t* mac,
        const uint8_t* data,
        int length
    ) {
        if (userCallback()) userCallback()(mac, data, length);
    }
  #endif
#elif defined(ESP8266)
    inline void onReceive(uint8_t* mac, uint8_t* data, uint8_t length) {
        if (userCallback()) userCallback()(mac, data, length);
    }
#endif

} // namespace detail

inline void setChannel(uint8_t channel) {
#if defined(ESP32)
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#elif defined(ESP8266)
    // The channel can only be changed reliably while promiscuous mode is on.
    wifi_promiscuous_enable(1);
    wifi_set_channel(channel);
    wifi_promiscuous_enable(0);
#endif
}

// Puts WiFi in station mode on the given channel and starts ESP-NOW.
inline bool begin(uint8_t channel) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    setChannel(channel);

#if defined(ESP32)
    return esp_now_init() == ESP_OK;
#elif defined(ESP8266)
    if (esp_now_init() != 0) return false;
    // COMBO = can both send and receive.
    return esp_now_set_self_role(ESP_NOW_ROLE_COMBO) == 0;
#endif
}

inline void onReceive(ReceiveCallback callback) {
    detail::userCallback() = callback;
    esp_now_register_recv_cb(detail::onReceive);
}

inline bool addPeer(const uint8_t* mac, uint8_t channel) {
#if defined(ESP32)
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    return esp_now_add_peer(&peer) == ESP_OK;
#elif defined(ESP8266)
    uint8_t* peerMac = const_cast<uint8_t*>(mac);
    if (esp_now_is_peer_exist(peerMac) > 0) return true;

    return esp_now_add_peer(
        peerMac, ESP_NOW_ROLE_COMBO, channel, nullptr, 0
    ) == 0;
#endif
}

inline bool send(const uint8_t* mac, const uint8_t* data, size_t length) {
#if defined(ESP32)
    return esp_now_send(mac, data, length) == ESP_OK;
#elif defined(ESP8266)
    return esp_now_send(
        const_cast<uint8_t*>(mac),
        const_cast<uint8_t*>(data),
        length
    ) == 0;
#endif
}

} // namespace ShEspNow
