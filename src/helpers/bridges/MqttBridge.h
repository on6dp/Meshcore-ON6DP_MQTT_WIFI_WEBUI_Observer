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
  #define MQTT_TOPIC "meshcore/rx"
#endif
#ifndef MQTT_CLIENT_ID
  #define MQTT_CLIENT_ID "meshcore-observer"
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
  String _server;
  int _port;
  String _topic;
  uint32_t _last_reconnect_attempt = 0;

  void loadSettings();
  bool ensureConnected();
  void publish(mesh::Packet *packet, const char *dir, float score, int rssi);
};

#endif
