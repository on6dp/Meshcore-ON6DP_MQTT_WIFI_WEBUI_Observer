#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <target.h>  // declares board, radio_driver, radio_init() (same as the real firmware)

#ifndef WIFI_SSID
  #error "WIFI_SSID must be defined via build_flags"
#endif
#ifndef MQTT_SERVER
  #error "MQTT_SERVER must be defined via build_flags"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

uint32_t last_reconnect_attempt = 0;

// Identical to MqttBridge::ensureConnected(), for a fair comparison.
bool ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Isolated: WiFi not connected, skipping MQTT attempt");
    return false;
  }
  if (mqtt.connected()) return true;

  uint32_t now = millis();
  if (now - last_reconnect_attempt < 5000) return false;
  last_reconnect_attempt = now;

  Serial.printf("Isolated: connecting to %s:%d (state before=%d)...\n", MQTT_SERVER, MQTT_PORT, mqtt.state());
  bool ok = mqtt.connect("heltec_v4_isolated_test");
  Serial.printf("Isolated: connect() returned %d, state after=%d, WiFi RSSI=%d, free heap=%u\n",
                (int)ok, mqtt.state(), WiFi.RSSI(), ESP.getFreeHeap());
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Isolated MQTT + LoRa radio init test (no Dispatcher/RoomServer) ===");

  // --- Bring up the LoRa radio exactly like the real firmware does, but
  //     do NOT process/dispatch any packets afterwards. This isolates
  //     whether merely having the radio active (listening) affects MQTT. ---
  board.begin();
  if (!radio_init()) {
    Serial.println("Radio init FAILED - continuing anyway (MQTT test still valid without it)");
  } else {
    Serial.println("Radio init OK - LoRa radio is now active/listening in the background");
  }

  // --- WiFi + MQTT, identical to the previous isolated test ---
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  Serial.print("Connecting WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED to connect within 20s");
  }

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  Serial.println("Entering loop()...\n");
}

void loop() {
  static bool was_connected = false;
  bool now_connected = mqtt.connected();
  if (was_connected && !now_connected) {
    Serial.printf("Isolated: connection DROPPED at t=%lu ms, state=%d\n", millis(), mqtt.state());
  }
  was_connected = now_connected;

  if (ensureConnected()) {
    mqtt.loop();
  }

  // NOTE: deliberately NOT calling any Dispatcher/checkRecv()/mesh loop here -
  // the radio is powered up and listening at the hardware level (via
  // radio_init()), but nothing reads or processes any received packets.
}
