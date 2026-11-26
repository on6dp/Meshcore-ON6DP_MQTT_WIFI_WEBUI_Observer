#include "M5CoreLan.h"

#ifdef WITH_M5_LAN

#include <SPI.h>
#include <M5_Ethernet.h>
#include <Dns.h>
#include <esp_system.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include "M5CoreBoard.h"

extern AutoDiscoverRTCClock rtc_clock;

#ifndef ETH_DHCP_TIMEOUT_MS
  #define ETH_DHCP_TIMEOUT_MS   8000
#endif
#ifndef ETH_DHCP_RETRY_MS
  #define ETH_DHCP_RETRY_MS     30000
#endif
#ifndef ETH_TCP_TIMEOUT_MS
  #define ETH_TCP_TIMEOUT_MS    3000
#endif
#ifndef NTP_SERVER
  #define NTP_SERVER            "pool.ntp.org"
#endif
#ifndef NTP_RESYNC_SECS
  #define NTP_RESYNC_SECS       (6UL * 60 * 60)
#endif
#define NTP_LOCAL_PORT          2390
#define NTP_REPLY_TIMEOUT_MS    1500
#define NTP_RETRY_MS            60000

static uint8_t  mac[6];
static bool     hw_ok = false;
static bool     link_up = false;
static bool     ip_ok = false;
static bool     time_ok = false;
static uint32_t next_link_check = 0;
static uint32_t next_ip_attempt = 0;
static uint32_t next_ntp_at = 0;

// NTP exchange in progress
static EthernetUDP ntp_udp;
static bool     ntp_waiting = false;
static uint32_t ntp_sent_at = 0;

// ---- transport used by MqttBridge (see MQTT_NET_EXTERNAL) ----
Client& mqttNetClient() {
  static EthernetClient client;
  static bool configured = false;
  if (!configured) {
    client.setConnectionTimeout(ETH_TCP_TIMEOUT_MS);
    configured = true;
  }
  return client;
}

bool mqttNetReady() {
  return hw_ok && link_up && ip_ok;
}

namespace m5lan {

bool hasHardware() { return hw_ok; }
bool linkUp()      { return hw_ok && link_up; }
bool hasIP()       { return hw_ok && link_up && ip_ok; }
bool timeSynced()  { return time_ok; }

IPAddress localIP() {
  return hasIP() ? Ethernet.localIP() : IPAddress(0, 0, 0, 0);
}

const char* statusText() {
  if (!hw_ok) return "module absent";
  if (!link_up) return "cable debranche";
  if (!ip_ok) return "attente IP";
  return "connecte";
}

void begin() {
  esp_read_mac(mac, ESP_MAC_ETH);   // unique, derived from the ESP32 factory MAC

  // the reset pulse was already done by M5CoreBoard::begin()
  Ethernet.init(P_ETH_CS);
  hw_ok = (Ethernet.hardwareStatus() != EthernetNoHardware);
  if (!hw_ok) {
    Serial.printf("ETH: no W5500 found (CS=%d) - check the jumper caps of the LAN module\n", P_ETH_CS);
    return;
  }
  Serial.printf("ETH: W5500 found, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool acquireAddress() {
#ifdef ETH_STATIC_IP
  IPAddress ip, gw, dns, mask(255, 255, 255, 0);
  if (!ip.fromString(ETH_STATIC_IP)) {
    Serial.println("ETH: ETH_STATIC_IP is not a valid address");
    return false;
  }
  gw = ip; gw[3] = 1;
  #ifdef ETH_GATEWAY
    gw.fromString(ETH_GATEWAY);
  #endif
  dns = gw;
  #ifdef ETH_DNS
    dns.fromString(ETH_DNS);
  #endif
  #ifdef ETH_SUBNET
    mask.fromString(ETH_SUBNET);
  #endif
  Ethernet.begin(mac, ip, dns, gw, mask);
  return true;
#else
  // blocks the main loop while waiting for the DHCP server (at most ETH_DHCP_TIMEOUT_MS)
  return Ethernet.begin(mac, ETH_DHCP_TIMEOUT_MS, 2000) == 1;
#endif
}

static void ntpStart() {
  IPAddress server;
  DNSClient dns;
  dns.begin(Ethernet.dnsServerIP());
  if (dns.getHostByName(NTP_SERVER, server, 2000) != 1) {
    Serial.println("NTP: DNS lookup failed");
    next_ntp_at = millis() + NTP_RETRY_MS;
    return;
  }

  uint8_t pkt[48];
  memset(pkt, 0, sizeof(pkt));
  pkt[0] = 0x23;   // LI=0, version 4, mode 3 (client)

  if (!ntp_udp.begin(NTP_LOCAL_PORT)) {
    next_ntp_at = millis() + NTP_RETRY_MS;
    return;
  }
  if (ntp_udp.beginPacket(server, 123) && ntp_udp.write(pkt, sizeof(pkt)) == sizeof(pkt) && ntp_udp.endPacket()) {
    ntp_waiting = true;
    ntp_sent_at = millis();
  } else {
    ntp_udp.stop();
    next_ntp_at = millis() + NTP_RETRY_MS;
  }
}

static void ntpPoll() {
  if (ntp_udp.parsePacket() >= 48) {
    uint8_t pkt[48];
    ntp_udp.read(pkt, sizeof(pkt));
    ntp_udp.stop();
    ntp_waiting = false;

    uint32_t secs1900 = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) | ((uint32_t)pkt[42] << 8) | pkt[43];
    if (secs1900 > 2208988800UL + 1700000000UL) {   // sanity check: later than Nov 2023
      uint32_t epoch = secs1900 - 2208988800UL;
      rtc_clock.setCurrentTime(epoch);
      time_ok = true;
      next_ntp_at = millis() + NTP_RESYNC_SECS * 1000UL;
      Serial.printf("NTP: clock set, epoch=%u\n", (unsigned)epoch);
    } else {
      next_ntp_at = millis() + NTP_RETRY_MS;
    }
  } else if (millis() - ntp_sent_at > NTP_REPLY_TIMEOUT_MS) {
    ntp_udp.stop();
    ntp_waiting = false;
    next_ntp_at = millis() + NTP_RETRY_MS;
    Serial.println("NTP: no reply");
  }
}

void loop() {
  if (!hw_ok) return;
  uint32_t now = millis();

  if ((int32_t)(now - next_link_check) >= 0) {
    next_link_check = now + 1000;

    bool up = (Ethernet.linkStatus() == LinkON);
    if (up != link_up) {
      link_up = up;
      Serial.printf("ETH: link %s\n", up ? "UP" : "DOWN");
      if (!up) {
        ip_ok = false;
        if (ntp_waiting) { ntp_udp.stop(); ntp_waiting = false; }
      } else {
        next_ip_attempt = now + 500;   // let the link settle
      }
    }

  #ifndef ETH_STATIC_IP
    if (ip_ok) {
      Ethernet.maintain();   // DHCP lease renewal
    }
  #endif
  }

  if (link_up && !ip_ok && (int32_t)(now - next_ip_attempt) >= 0) {
    if (acquireAddress()) {
      ip_ok = true;
      IPAddress ip = Ethernet.localIP();
      Serial.printf("ETH: IP %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
      next_ntp_at = millis() + 2000;
    } else {
      Serial.println("ETH: no DHCP answer, will retry");
      next_ip_attempt = millis() + ETH_DHCP_RETRY_MS;
    }
  }

  if (ip_ok) {
    if (ntp_waiting) {
      ntpPoll();
    } else if ((int32_t)(millis() - next_ntp_at) >= 0) {
      ntpStart();
    }
  }
}

}  // namespace m5lan

#else   // WITH_M5_LAN not defined: keep the API available, always "absent"

namespace m5lan {
  void begin() {}
  void loop() {}
  bool hasHardware() { return false; }
  bool linkUp() { return false; }
  bool hasIP() { return false; }
  bool timeSynced() { return false; }
  IPAddress localIP() { return IPAddress(0, 0, 0, 0); }
  const char* statusText() { return "desactive"; }
}

#endif
