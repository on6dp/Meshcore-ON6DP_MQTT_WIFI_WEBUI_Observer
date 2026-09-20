#pragma once

#include "helpers/bridges/BridgeBase.h"

#ifdef WITH_MQTT_BRIDGE

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

#ifndef MQTT_SERVER
  #error "MQTT_SERVER must be defined, e.g. -D MQTT_SERVER='\"192.168.1.10\"'"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif
#ifndef MQTT_TOPIC
  #define MQTT_TOPIC "meshcore/BE/meshcore-observer/packets"
#endif
#ifndef MQTT_CLIENT_ID
  #define MQTT_CLIENT_ID "meshcore-observer"
#endif
#ifndef MQTT_STATUS_REGION
  #define MQTT_STATUS_REGION "BE"
#endif
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "v1.17.3"
#endif
// Custom label sent as the "firmware" field to CoreScope - distinct from
// FIRMWARE_VERSION (which stays the real underlying MeshCore build version,
// used elsewhere for protocol compatibility). Override via build_flags,
// e.g. -D MQTT_FIRMWARE_LABEL='"ON6DP-Custom Firmware 1.17.3"'
#ifndef MQTT_FIRMWARE_LABEL
  #define MQTT_FIRMWARE_LABEL FIRMWARE_VERSION
#endif

/**
 * @brief Bridge implementation that publishes observed mesh packets to an MQTT
 *        broker over WiFi.
 *
 * This is an OBSERVER bridge, not a two-way relay like RS232Bridge/ESPNowBridge:
 * onPacketReceived() is intentionally a no-op, so nothing coming from MQTT is
 * ever injected back onto the LoRa mesh. It only publishes JSON summaries
 * (type, route, payload length, SNR, RSSI, score, packet hash) plus the
 * base64-encoded raw packet bytes, for both RX and TX events.
 *
 * MQTT_SERVER / MQTT_PORT / MQTT_TOPIC (build flags) are only the *defaults*
 * used on first boot. From then on, the active values live in NVS (via the
 * Preferences library) and can be changed at runtime with:
 *   set mqtt.server <host>
 *   set mqtt.port <port>
 *   set mqtt.topic <topic>
 * A change takes effect immediately (the bridge reconnects), no reboot needed.
 */
class MqttBridge : public BridgeBase {
public:
  MqttBridge(NodePrefs *prefs, mesh::PacketManager *mgr, mesh::RTCClock *rtc);

  void begin() override;
  void end() override;
  void loop() override;

  // Required by AbstractBridge - used for TX events (called from logTx).
  void sendPacket(mesh::Packet *packet) override;

  // Observer-only: never re-injects packets into the mesh.
  void onPacketReceived(mesh::Packet *packet) override;

  // Extra (non-virtual) method for detailed RX logging, called from logRx,
  // since RX has metadata (SNR/RSSI/score) that TX does not.
  void publishRx(mesh::Packet *packet, int len, float score, int rssi);

  // Runtime-configurable settings (persisted to NVS, applied immediately).
  String getServer() const { return _server; }
  int getPort() const { return _port; }
  String getTopic() const { return _topic; }
  void setServer(const String &host);
  void setPort(int port);
  void setTopic(const String &topic);

private:
  WiFiClient _wifiClient;
  PubSubClient _mqtt;
  Preferences _store;
  NodePrefs *_node_prefs;
  String _server;
  int _port;
  String _topic;
  uint32_t _last_reconnect_attempt = 0;
  uint32_t _last_status_publish = 0;

  void loadSettings();
  bool ensureConnected();
  void publish(mesh::Packet *packet, const char *dir, float score, int rssi);
  // Publishes a "presence" message on meshcore/<region>/<client_id>/status,
  // in the same style used by KiekR and other observers - this is what
  // makes this device show up as a named entry in CoreScope's Observers
  // tab, instead of just contributing raw packet data anonymously.
  void publishStatus();
};

#endif
