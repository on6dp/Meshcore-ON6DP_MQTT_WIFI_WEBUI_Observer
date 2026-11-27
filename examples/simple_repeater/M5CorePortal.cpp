#include "M5CorePortal.h"

#if defined(M5CORE_UI) && defined(WITH_M5_WIFI)

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>
#include <M5CoreWifi.h>
#include "MyMesh.h"

extern MyMesh the_mesh;

#ifndef M5_AP_NAME
  #define M5_AP_NAME "MeshCore-M5"
#endif
#define PORTAL_IDLE_TIMEOUT_MS   (10UL * 60 * 1000)
#define PORTAL_CLOSE_DELAY_MS    3000

static WebServer server(80);
static bool      running = false;
static char      ap_pwd[9] = "";
static uint32_t  last_request = 0;
static uint32_t  close_at = 0;          // 0 = stay open

static String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

static void pageHead(String& h, const char* title) {
  h += F("<!DOCTYPE html><html lang='fr'><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'><title>");
  h += title;
  h += F("</title><style>"
         "body{font-family:Arial,Helvetica,sans-serif;font-weight:bold;background:#10182b;color:#eef2f8;margin:0;padding:16px}"
         "h1{font-size:20px;margin:0 0 14px}"
         "fieldset{border:2px solid #3b6fd8;border-radius:14px;margin:0 0 16px;padding:12px 14px;background:#172342}"
         "legend{padding:0 8px;color:#8fb4ff}"
         "label{display:block;margin:10px 0 4px}"
         "input{width:100%;box-sizing:border-box;padding:10px;border:2px solid #3b6fd8;border-radius:10px;"
         "background:#0c1322;color:#fff;font-weight:bold;font-size:16px}"
         "button{width:100%;padding:14px;border:0;border-radius:12px;background:#2f6fe4;color:#fff;font-weight:bold;font-size:17px}"
         "p{margin:6px 0}.n{color:#9aa7bd;font-size:13px}"
         "</style></head><body>");
}

static void field(String& h, const char* label, const char* name, const String& value,
                  const char* type = "text", const char* hint = "") {
  h += F("<label>");
  h += label;
  h += F("</label><input type='");
  h += type;
  h += F("' name='");
  h += name;
  h += F("' value='");
  h += htmlEscape(value);
  h += F("' placeholder='");
  h += hint;
  h += F("' autocomplete='off' autocapitalize='none'>");
}

static void handleRoot() {
  last_request = millis();

  String h;
  h.reserve(4200);
  pageHead(h, "Configuration M5Stack");
  h += F("<h1>");
  h += htmlEscape(String(the_mesh.getNodePrefs()->node_name));
  h += F("</h1><form method='post' action='/save'>");

  h += F("<fieldset><legend>WiFi 1 (maison)</legend>");
  field(h, "Nom du r&eacute;seau", "s1", String(m5wifi::ssid(0)));
  field(h, "Mot de passe", "p1", String(""), "password", "inchang&eacute; si vide");
  h += F("</fieldset>");

  h += F("<fieldset><legend>WiFi 2 (t&eacute;l&eacute;phone, d&eacute;mo)</legend>");
  field(h, "Nom du r&eacute;seau", "s2", String(m5wifi::ssid(1)));
  field(h, "Mot de passe", "p2", String(""), "password", "inchang&eacute; si vide");
  h += F("</fieldset>");

#ifdef WITH_MQTT_BRIDGE
  MqttBridge& mqtt = the_mesh.getMqttBridge();
  h += F("<fieldset><legend>MQTT</legend>");
  field(h, "Serveur", "ms", mqtt.getServer());
  field(h, "Port", "mp", String(mqtt.getPort()), "number");
  field(h, "Topic", "mt", mqtt.getTopic());
  h += F("</fieldset>");
#endif

  h += F("<button type='submit'>Enregistrer</button>"
         "<p class='n'>Un nom de r&eacute;seau vide efface ce r&eacute;seau. "
         "Le Core ne fonctionne qu'en WiFi 2,4 GHz.</p></form></body></html>");

  server.send(200, "text/html; charset=utf-8", h);
}

static void handleSave() {
  last_request = millis();

  if (server.hasArg("s1")) {
    String v = server.arg("s1"); v.trim();
    if (v != String(m5wifi::ssid(0))) m5wifi::setSsid(0, v.c_str());
  }
  if (server.hasArg("p1") && server.arg("p1").length() > 0) {
    m5wifi::setPassword(0, server.arg("p1").c_str());
  }
  if (server.hasArg("s2")) {
    String v = server.arg("s2"); v.trim();
    if (v != String(m5wifi::ssid(1))) m5wifi::setSsid(1, v.c_str());
  }
  if (server.hasArg("p2") && server.arg("p2").length() > 0) {
    m5wifi::setPassword(1, server.arg("p2").c_str());
  }

#ifdef WITH_MQTT_BRIDGE
  MqttBridge& mqtt = the_mesh.getMqttBridge();
  if (server.hasArg("ms")) {
    String v = server.arg("ms"); v.trim();
    if (v.length() > 0 && v != mqtt.getServer()) mqtt.setServer(v);
  }
  if (server.hasArg("mp")) {
    int port = server.arg("mp").toInt();
    if (port > 0 && port < 65536 && port != mqtt.getPort()) mqtt.setPort(port);
  }
  if (server.hasArg("mt")) {
    String v = server.arg("mt"); v.trim();
    if (v.length() > 0 && v != mqtt.getTopic()) mqtt.setTopic(v);
  }
#endif

  String h;
  h.reserve(1400);
  pageHead(h, "Enregistr&eacute;");
  h += F("<h1>R&eacute;glages enregistr&eacute;s</h1>"
         "<p>Le mode configuration se ferme dans quelques secondes.</p>"
         "<p>Le Core va maintenant chercher les r&eacute;seaux WiFi enregistr&eacute;s.</p>"
         "</body></html>");
  server.send(200, "text/html; charset=utf-8", h);

  close_at = millis() + PORTAL_CLOSE_DELAY_MS;   // leave time for the answer to be sent
  if (close_at == 0) close_at = 1;
}

static void handleOther() {   // anything else (including captive-portal probes) goes to the form
  last_request = millis();
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

namespace m5portal {

bool active() { return running; }
const char* apName() { return M5_AP_NAME; }
const char* apPassword() { return ap_pwd; }

bool start() {
  if (running) return true;

  // a new 8-digit password for every session; it is only ever shown on the Core's screen
  snprintf(ap_pwd, sizeof(ap_pwd), "%08u", (unsigned)(esp_random() % 100000000UL));

  // Note: setSsid()/setPassword() stop the station side, which is already off in this mode.
  if (!m5wifi::startAccessPoint(M5_AP_NAME, ap_pwd)) return false;

  static bool routes_set = false;
  if (!routes_set) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleOther);
    routes_set = true;
  }
  server.begin();

  running = true;
  close_at = 0;
  last_request = millis();
  return true;
}

void stop() {
  if (!running) return;
  server.stop();
  m5wifi::stopAccessPoint();
  running = false;
  close_at = 0;
}

void loop() {
  if (!running) return;
  server.handleClient();

  uint32_t now = millis();
  if (close_at != 0 && (int32_t)(now - close_at) >= 0) {
    stop();
  } else if (now - last_request > PORTAL_IDLE_TIMEOUT_MS) {
    stop();
  }
}

}  // namespace m5portal

#else   // configuration mode not available in this build

namespace m5portal {
  bool start() { return false; }
  void stop() {}
  void loop() {}
  bool active() { return false; }
  const char* apName() { return ""; }
  const char* apPassword() { return ""; }
}

#endif
