#include "MqttBridge.h"

#ifdef WITH_MQTT_BRIDGE

// Note: base64.hpp no longer needed here - the raw packet payload is now
// sent as hex, not base64 (see publish() below).

MqttBridge::MqttBridge(NodePrefs *prefs, mesh::PacketManager *mgr, mesh::RTCClock *rtc)
    : BridgeBase(prefs, mgr, rtc), _mqtt(_wifiClient), _node_prefs(prefs) {}

void MqttBridge::loadSettings() {
  _store.begin("mqttcfg", false);  // read-write namespace in NVS
  _server = _store.getString("server", MQTT_SERVER);
  _port = _store.getInt("port", MQTT_PORT);
  _topic = _store.getString("topic", MQTT_TOPIC);
  _store.end();
}

void MqttBridge::begin() {
  loadSettings();
  BRIDGE_DEBUG_PRINTLN("MqttBridge: init, broker=%s:%d topic=%s\n", _server.c_str(), _port, _topic.c_str());
  _wifiClient.setTimeout(30);  // seconds - ESP32 WiFiClient defaults to a 5s read timeout,
                                // which silently tears down the socket during quiet periods
                                // with no MQTT traffic (well before our 15s MQTT keepalive
                                // would even trigger a PING). This was causing the connection
                                // to drop roughly every 5 seconds.
  _mqtt.setBufferSize(1024);  // default PubSubClient buffer (256 bytes) is too small for our
                               // base64-encoded packet payloads and can corrupt the connection
  _mqtt.setServer(_server.c_str(), _port);
  // Note: WiFi.begin() itself must already have been called in main.cpp's setup(),
  // this bridge only manages the MQTT connection on top of an existing WiFi link.
  _initialized = true;
}

void MqttBridge::end() {
  _mqtt.disconnect();
  _initialized = false;
}

void MqttBridge::setServer(const String &host) {
  _server = host;
  _store.begin("mqttcfg", false);
  _store.putString("server", _server);
  _store.end();
  _mqtt.disconnect();
  _mqtt.setServer(_server.c_str(), _port);
  _last_reconnect_attempt = 0;  // force an immediate reconnect attempt
}

void MqttBridge::setPort(int port) {
  _port = port;
  _store.begin("mqttcfg", false);
  _store.putInt("port", _port);
  _store.end();
  _mqtt.disconnect();
  _mqtt.setServer(_server.c_str(), _port);
  _last_reconnect_attempt = 0;
}

void MqttBridge::setTopic(const String &topic) {
  _topic = topic;
  _store.begin("mqttcfg", false);
  _store.putString("topic", _topic);
  _store.end();
  // topic takes effect on next publish() call, no reconnect needed
}

bool MqttBridge::ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MqttBridge: WiFi not connected, skipping MQTT attempt");
    return false;
  }
  if (_mqtt.connected()) return true;

  uint32_t now = millis();
  if (now - _last_reconnect_attempt < 300) return false;  // throttle retries to every 300ms
                                                             // (was 5000ms - the root cause of the
                                                             // reconnect cycle is still unknown, but
                                                             // reconnects succeed instantly every time,
                                                             // so shortening this window minimizes the
                                                             // gap during which incoming mesh packets
                                                             // would be lost)
  _last_reconnect_attempt = now;

  Serial.printf("MqttBridge: connecting to %s:%d (state before=%d)...\n", _server.c_str(), _port, _mqtt.state());
  _wifiClient.stop();  // ensure any half-open socket from a previous failed attempt is
                        // fully released before opening a new one
  bool ok = _mqtt.connect(MQTT_CLIENT_ID);
  Serial.printf("MqttBridge: connect() returned %d, state after=%d, WiFi RSSI=%d, free heap=%u\n",
                (int)ok, _mqtt.state(), WiFi.RSSI(), ESP.getFreeHeap());
  return ok;
}

void MqttBridge::loop() {
  if (!_initialized) return;

  static bool was_connected = false;
  bool now_connected = _mqtt.connected();
  if (was_connected && !now_connected) {
    Serial.printf("MqttBridge: connection DROPPED at t=%lu ms, state=%d\n", millis(), _mqtt.state());
  }
  was_connected = now_connected;

  if (ensureConnected()) {
    _mqtt.loop();

    uint32_t now = millis();
    if (now - _last_status_publish > 60000) {  // every 60s
      _last_status_publish = now;
      publishStatus();
    }
  }
}

void MqttBridge::publishStatus() {
  char topic[80];
  snprintf(topic, sizeof(topic), "meshcore/%s/%s/status", MQTT_STATUS_REGION, MQTT_CLIENT_ID);

  // "firmware" and "client_version" are two distinct fields CoreScope reads
  // separately (confirmed in its v2.1.1 release notes) - firmware is the
  // actual MeshCore build version, client_version is more of a label for
  // the bridge/app publishing this status.
  char json[260];
  snprintf(json, sizeof(json),
           "{\"status\":\"online\",\"origin\":\"%s\",\"origin_id\":\"%s\","
           "\"firmware\":\"%s\",\"client_version\":\"on6dp-heltec-v4\"}",
           _node_prefs->node_name, MQTT_CLIENT_ID, MQTT_FIRMWARE_LABEL);

  bool ok = _mqtt.publish(topic, json);
  Serial.printf("MqttBridge: status published to %s, ok=%d\n", topic, (int)ok);
}

static void toHexHash(const uint8_t *hash, int len, char *out) {
  static const char *hexd = "0123456789ABCDEF";
  for (int i = 0; i < len; i++) {
    out[i * 2] = hexd[(hash[i] >> 4) & 0xF];
    out[i * 2 + 1] = hexd[hash[i] & 0xF];
  }
  out[len * 2] = 0;
}

void MqttBridge::publish(mesh::Packet *packet, const char *dir, float score, int rssi) {
  if (!ensureConnected()) return;

  uint8_t raw[MAX_TRANS_UNIT + 1];
  int raw_len = packet->writeTo(raw);
  if (raw_len <= 0) return;

  // CoreScope's ingestor expects the raw packet bytes as hex, not base64
  // (confirmed via its own "decode error: invalid hex" log message) -
  // toHexHash() below works for any byte array, not just hashes.
  char *raw_hex = (char *)malloc(raw_len * 2 + 1);
  if (!raw_hex) return;
  toHexHash(raw, raw_len, raw_hex);

  uint8_t hash[MAX_HASH_SIZE];
  packet->calculatePacketHash(hash);
  char hash_hex[MAX_HASH_SIZE * 2 + 1];
  toHexHash(hash, MAX_HASH_SIZE, hash_hex);

  // JSON buffer sized generously for a hex payload (2 chars/byte) up to MAX_TRANS_UNIT+1 bytes
  unsigned int json_size = (raw_len * 2) + 256;
  char *json = (char *)malloc(json_size);
  if (json) {
    snprintf(json, json_size,
             "{\"dir\":\"%s\",\"type\":%d,\"route\":\"%s\",\"payload_len\":%d,"
             "\"snr\":%d,\"rssi\":%d,\"score\":%d,\"hash\":\"%s\",\"raw\":\"%s\"}",
             dir, packet->getPayloadType(), packet->isRouteDirect() ? "D" : "F", packet->payload_len,
             (int)packet->getSNR(), rssi, (int)(score * 1000), hash_hex, raw_hex);

    bool ok = _mqtt.publish(_topic.c_str(), json);
    Serial.printf("MqttBridge: publish %s at t=%lu ms, json_len=%d, ok=%d, state=%d\n",
                  dir, millis(), (int)strlen(json), (int)ok, _mqtt.state());
    free(json);
  }
  free(raw_hex);
}

void MqttBridge::publishRx(mesh::Packet *packet, int len, float score, int rssi) {
  if (!_initialized) return;
  publish(packet, "rx", score, rssi);
}

void MqttBridge::sendPacket(mesh::Packet *packet) {
  // Called from logTx hook (observer mode: we log TX too, we don't dedupe/forward like RS232Bridge does)
  if (!_initialized) return;
  publish(packet, "tx", 0.0f, 0);
}

void MqttBridge::onPacketReceived(mesh::Packet *packet) {
  // Intentionally empty: this bridge is receive-to-MQTT only, it never
  // injects anything back onto the LoRa mesh.
}

#endif
