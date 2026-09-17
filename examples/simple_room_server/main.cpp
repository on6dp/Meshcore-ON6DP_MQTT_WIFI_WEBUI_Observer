#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "MyMesh.h"

#ifdef ETHERNET_ENABLED
  #define ETHERNET_CLI_BANNER "MeshCore Room Server CLI"
  #include <helpers/nrf52/EthernetCLI.h>
#endif

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

#ifdef WIFI_SSID
#include <WiFi.h>
#include <Preferences.h>

static bool wifi_needs_reconnect = false;
static Preferences wifi_store;
static String wifi_ssid;
static String wifi_pwd;

// Mirrors MqttBridge's NVS pattern: runtime-configurable, falls back to the
// build-time -D WIFI_SSID/-D WIFI_PWD defaults if nothing stored yet.
static void loadWifiSettings() {
  wifi_store.begin("wificfg", false);
  wifi_ssid = wifi_store.getString("ssid", WIFI_SSID);
  wifi_pwd = wifi_store.getString("pwd", WIFI_PWD);
  wifi_store.end();
}

String getWifiSSID() {
  return wifi_ssid;
}

void setWifiSSID(const char* ssid) {
  wifi_ssid = ssid;
  wifi_store.begin("wificfg", false);
  wifi_store.putString("ssid", wifi_ssid);
  wifi_store.end();
  Serial.printf("WiFi: SSID updated to '%s', reconnecting...\n", wifi_ssid.c_str());
  WiFi.disconnect();
  delay(100);
  WiFi.begin(wifi_ssid.c_str(), wifi_pwd.c_str());
}

void setWifiPwd(const char* pwd) {
  wifi_pwd = pwd;
  wifi_store.begin("wificfg", false);
  wifi_store.putString("pwd", wifi_pwd);
  wifi_store.end();
  Serial.println("WiFi: password updated, reconnecting...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(wifi_ssid.c_str(), wifi_pwd.c_str());
}

void connectWifi() {
  loadWifiSettings();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // disable modem-sleep: default WiFi power-save causes
                          // brief radio sleep windows that silently kill remote
                          // TCP/MQTT connections even though WiFi stays "connected"

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.println("WiFi disconnected. Flagging for reconnect...");
      wifi_needs_reconnect = true;
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.print("WiFi connected, IP=");
      Serial.println(WiFi.localIP());
      wifi_needs_reconnect = false;
    }
  });

  WiFi.begin(wifi_ssid.c_str(), wifi_pwd.c_str());
}
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

#ifdef WITH_WEB_CONFIG
#include <WebServer.h>
WebServer webServer(80);

// Reuses the existing admin password (the same one used for 'set password' / mesh admin login)
// so there's only ever one password to manage.
static bool webAuthOk() {
  if (webServer.authenticate("admin", the_mesh.getNodePrefs()->password)) return true;
  webServer.requestAuthentication();
  return false;
}

// Runs a CLI command through the exact same handler used by Serial/USB, and
// returns the reply as a String. Command text is copied into a writable
// buffer since handleCommand() mutates it in place (prefix stripping etc.)
static String runCmd(const char *c) {
  char cmdBuf[64];
  strncpy(cmdBuf, c, sizeof(cmdBuf) - 1);
  cmdBuf[sizeof(cmdBuf) - 1] = 0;

  char reply[160];
  reply[0] = 0;
  the_mesh.handleCommand(0, cmdBuf, reply);
  return String(reply[0] ? reply : "-");
}

// Minimal JSON string escaping - handles the characters actually likely to
// appear in CLI replies (quotes, backslashes, newlines). Not a full JSON
// encoder, but sufficient for this firmware's own text output.
static String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') { out += "\\n"; continue; }
    if (c == '\r') continue;
    out += c;
  }
  return out;
}

// Shared design system: light/dark auto (prefers-color-scheme), cards, tabs,
// monospace terminal-style console. Self-contained, no external assets.
static const char WEB_STYLE[] =
  ":root{--bg:#f1f3ef;--card:#fff;--ink:#1c231d;--mut:#5d6b5f;--line:#dde3d9;"
  "--acc:#c47a1f;--acc-ink:#fff;--ok:#2f8a4e;--chip:#e9ede5;--shadow:0 1px 3px rgba(20,30,16,.08)}"
  "@media (prefers-color-scheme:dark){:root{--bg:#0e1510;--card:#182018;--ink:#e3ecdf;--mut:#8a9a86;"
  "--line:#26332a;--acc:#e0973a;--chip:#1f2921;--shadow:0 1px 3px rgba(0,0,0,.4)}}"
  "*{box-sizing:border-box;margin:0}"
  "body{font:15px/1.45 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);"
  "color:var(--ink);padding:0 0 24px}"
  "h1{font-size:17px;font-weight:650}"
  "h2{font-size:13px;font-weight:650;color:var(--mut);margin:0 0 10px}"
  "header{display:flex;align-items:center;gap:10px;padding:14px 16px;background:var(--card);"
  "border-bottom:1px solid var(--line);position:sticky;top:0;z-index:10}"
  ".hmeta div{font-size:12px;color:var(--mut)}"
  ".sig{flex-shrink:0;color:var(--acc)}"
  "main{max-width:640px;margin:0 auto;padding:16px}"
  ".card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px;"
  "box-shadow:var(--shadow);margin-bottom:14px}"
  ".tabs{display:flex;gap:4px;background:var(--chip);padding:4px;border-radius:9px;margin-bottom:14px}"
  ".tabs button{flex:1;border:0;background:none;color:var(--mut);font:inherit;font-size:13.5px;"
  "font-weight:600;padding:7px 4px;border-radius:6px;cursor:pointer}"
  ".tabs button.on{background:var(--card);color:var(--ink);box-shadow:var(--shadow)}"
  ".kv{font-size:13.5px;display:flex;justify-content:space-between;gap:12px;padding:7px 0;"
  "border-bottom:1px solid var(--line)}"
  ".kv:last-child{border-bottom:0}"
  ".kv i{font-style:normal;color:var(--mut)}"
  ".kv b{font-weight:600;font-family:ui-monospace,Menlo,Consolas,monospace}"
  "input[type=text]{width:100%;padding:9px 10px;font:15px ui-monospace,Menlo,Consolas,monospace;"
  "color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:7px;outline:none}"
  "input[type=text]:focus{border-color:var(--acc)}"
  "button.btn{display:inline-flex;align-items:center;justify-content:center;border:0;font:inherit;"
  "font-size:14px;font-weight:650;padding:9px 16px;border-radius:8px;cursor:pointer;"
  "background:var(--acc);color:var(--acc-ink);margin-top:8px}"
  ".term{background:#0d130d;color:#d3ddd0;border:1px solid #1f291f;border-radius:9px;padding:12px;"
  "font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px;white-space:pre-wrap;"
  "word-wrap:break-word;margin-bottom:12px;min-height:48px;max-height:320px;overflow-y:auto}"
  ".term .cmd:before{content:'> ';color:#e0973a}"
  ".term .cmd{color:#fff}"
  ".term .rep{color:#7fc98a;display:block;margin-top:4px}"
  "#cli-sugg{margin-top:4px}"
  ".sugg{padding:6px 8px;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px;"
  "background:var(--chip);border-radius:6px;margin-top:3px;cursor:pointer}"
  ".sugg:hover{background:var(--line)}"
  "a.back{color:var(--acc);font-size:13px;text-decoration:none}";

static const char *NAV_HTML =
  "<div class='tabs'>"
  "<button class='on' onclick=\"sw('radio')\" id='tb-radio'>Radio</button>"
  "<button onclick=\"sw('mqtt')\" id='tb-mqtt'>MQTT</button>"
  "<button onclick=\"sw('wifi')\" id='tb-wifi'>WiFi</button>"
  "<button onclick=\"sw('stats')\" id='tb-stats'>Stats</button>"
  "<button onclick=\"sw('cli')\" id='tb-cli'>CLI</button>"
  "</div>"
  "<script>"
  "var TABS=['radio','mqtt','wifi','stats','cli'];"
  "function sw(t){"
  "TABS.forEach(function(x){"
  "document.getElementById('p-'+x).style.display=(x==t)?'':'none';"
  "document.getElementById('tb-'+x).className=(x==t)?'on':'';"
  "});"
  "}</script>";

// Small inline antenna/signal glyph - no external assets, gives the header
// a radio-operator identity instead of a generic dashboard look.
static const char *SIG_ICON =
  "<svg class='sig' width='22' height='22' viewBox='0 0 24 24' fill='none' "
  "stroke='currentColor' stroke-width='2' stroke-linecap='round'>"
  "<path d='M12 2v8'/><circle cx='12' cy='14' r='3'/>"
  "<path d='M7 8a7 7 0 0 1 10 0' stroke-opacity='.55'/>"
  "<path d='M4 5a11 11 0 0 1 16 0' stroke-opacity='.3'/></svg>";

static void sendHead(String &html, const String &title) {
  html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
          "<title>" + title + "</title><style>" + String(WEB_STYLE) + "</style></head><body>"
          "<header>" + String(SIG_ICON) + "<div class='hmeta'><h1>" + String(the_mesh.getNodeName()) + "</h1>"
          "<div>" + String(the_mesh.getFirmwareVer()) + " &middot; " + String(the_mesh.getRole()) + "</div>"
          "</div></header><main>";
}

void handleWebRoot() {
  if (!webAuthOk()) return;

  String html;
  sendHead(html, String(the_mesh.getNodeName()));
  html += String(NAV_HTML);

  // ---- Radio tab ----
  html += "<div id='p-radio'><div class='card'><h2>Radio &amp; mesh</h2>"
          "<div class='kv'><i>Repeat</i><b id='v-repeat'>" + runCmd("get repeat") + "</b></div>"
          "<div class='kv'><i>TX power</i><b id='v-tx'>" + runCmd("get tx") + " dBm</b></div>"
          "<div class='kv'><i>Frequency</i><b id='v-freq'>" + runCmd("get freq") + " MHz</b></div>"
          "<div class='kv'><i>Radio params</i><b id='v-radio'>" + runCmd("get radio") + "</b></div>"
          "<div class='kv'><i>Latitude</i><b id='v-lat'>" + runCmd("get lat") + "</b></div>"
          "<div class='kv'><i>Longitude</i><b id='v-lon'>" + runCmd("get lon") + "</b></div>"
          "</div></div>";

  // ---- MQTT tab ----
  html += "<div id='p-mqtt' style='display:none'>"
#ifdef WITH_MQTT_BRIDGE
          "<div class='card'><h2>MQTT observer</h2>"
          "<div class='kv'><i>Server</i><b id='v-mqtt-server'>" + runCmd("get mqtt.server") + "</b></div>"
          "<div class='kv'><i>Port</i><b id='v-mqtt-port'>" + runCmd("get mqtt.port") + "</b></div>"
          "<div class='kv'><i>Topic</i><b id='v-mqtt-topic'>" + runCmd("get mqtt.topic") + "</b></div>"
          "</div>"
#else
          "<div class='card'><h2>MQTT observer</h2><div class='kv'><i>Not enabled in this build</i></div></div>"
#endif
          "</div>";

  // ---- WiFi tab: live info + change forms (posts directly to the setters,
  // no CLI syntax needed from the person using it) ----
  html += "<div id='p-wifi' style='display:none'><div class='card'><h2>WiFi</h2>"
#ifdef WIFI_SSID
          "<div class='kv'><i>SSID</i><b id='v-wifi-ssid'>" + getWifiSSID() + "</b></div>"
          "<div class='kv'><i>IP address</i><b id='v-wifi-ip'>" + WiFi.localIP().toString() + "</b></div>"
          "<div class='kv'><i>Signal (RSSI)</i><b id='v-wifi-rssi'>" + String(WiFi.RSSI()) + " dBm</b></div>"
          "</div>"
          "<div class='card'><h2>Change network</h2>"
          "<button class='btn' type='button' onclick='doScan()' style='margin-bottom:10px'>Scan networks</button>"
          "<div id='scan-results'></div>"
          "<form method='POST' action='/wifi/ssid'>"
          "<input type='text' name='ssid' id='ssid-input' placeholder='New SSID' value='" + getWifiSSID() + "'>"
          "<br><button class='btn' type='submit'>Update SSID</button></form>"
          "<div style='height:10px'></div>"
          "<form method='POST' action='/wifi/pwd'>"
          "<input type='text' name='pwd' placeholder='New password'>"
          "<br><button class='btn' type='submit'>Update password</button></form>"
          "<div class='kv' style='margin-top:10px'><i>Note</i><b style='font-weight:500;font-family:inherit'>"
          "The device reconnects immediately after each change &mdash; if the new "
          "network is unreachable, use the CLI/serial console to set it back.</b></div>"
#else
          "<div class='kv'><i>Not enabled in this build</i></div></div>"
#endif
          "</div></div>";

  // ---- Stats tab (direct readings, no CLI round-trip needed) ----
  html += "<div id='p-stats' style='display:none'><div class='card'><h2>Device</h2>"
          "<div class='kv'><i>Free heap</i><b id='v-heap'>" + String(ESP.getFreeHeap()) + " bytes</b></div>"
          "<div class='kv'><i>Uptime</i><b id='v-uptime'>" + String(millis() / 1000) + " s</b></div>"
          "<div class='kv'><i>Firmware</i><b>" + String(the_mesh.getFirmwareVer()) + "</b></div>"
          "</div></div>";

  // ---- CLI tab: real async console (fetch to /api/cli, no page reload,
  // replies append to a growing log like a real terminal) ----
  html += "<div id='p-cli' style='display:none'><div class='card'>"
          "<h2>Command line</h2>"
          "<div class='term' id='cli-log'>Type any CLI command below (same ones as over USB serial). Press Enter to send.</div>"
          "<div style='position:relative'>"
          "<input type='text' id='cmdbox' autocomplete='off' placeholder=\"e.g. set repeat on\" "
          "oninput='cliSuggest(this.value)' onkeydown='if(event.key==\"Enter\"){event.preventDefault();sendCmd();}'>"
          "<div id='cli-sugg'></div></div>"
          "<button class='btn' onclick='sendCmd()'>Send</button>"
          "</div></div>"
          "<script>"
          "var CLI_CMDS=["
          "'get name','set name ','get lat','set lat ','get lon','set lon ',"
          "'get owner.info','set owner.info ','get public.key','get role',"
          "'get guest.password','set guest.password ',"
          "'get radio','set radio ','get freq','set freq ','get tx','set tx ',"
          "'get cad','set cad on','set cad off',"
          "'get radio.rxgain','set radio.rxgain ',"
          "'get radio.fem.rxgain','set radio.fem.rxgain ',"
          "'get radio.fem.txgain','set radio.fem.txgain ',"
          "'get extra.sf','set extra.sf ','get int.thresh','set int.thresh ',"
          "'get agc.reset.interval','set agc.reset.interval ',"
          "'get repeat','set repeat on','set repeat off',"
          "'get flood.max','set flood.max ',"
          "'get flood.max.advert','set flood.max.advert ',"
          "'get flood.max.unscoped','set flood.max.unscoped ',"
          "'get loop.detect','set loop.detect ',"
          "'get path.hash.mode','set path.hash.mode ',"
          "'get advert.interval','set advert.interval ',"
          "'get flood.advert.interval','set flood.advert.interval ',"
          "'get dutycycle','set dutycycle ','get af','set af ',"
          "'get rxdelay','set rxdelay ','get txdelay','set txdelay ',"
          "'get direct.txdelay','set direct.txdelay ',"
          "'get multi.acks','set multi.acks ',"
          "'get allow.read.only','set allow.read.only ',"
          "'get mqtt.server','set mqtt.server ',"
          "'get mqtt.port','set mqtt.port ',"
          "'get mqtt.topic','set mqtt.topic ',"
          "'get wifi.ssid','set wifi.ssid ','set wifi.pwd ',"
          "'get adc.multiplier','set adc.multiplier ','get bootloader.ver',"
          "'get acl','time ','room.post ','setperm '"
          "];"
          "function cliSuggest(v){"
          "var box=document.getElementById('cli-sugg');"
          "if(!v){box.innerHTML='';return;}"
          "var m=CLI_CMDS.filter(function(c){return c.indexOf(v)==0 && c!=v;}).slice(0,8);"
          "box.innerHTML=m.map(function(c){"
          "return \"<div class='sugg' onclick=\\\"pick('\"+c+\"')\\\">\"+c+\"</div>\";"
          "}).join('');"
          "}"
          "function pick(c){"
          "document.getElementById('cmdbox').value=c;"
          "document.getElementById('cli-sugg').innerHTML='';"
          "document.getElementById('cmdbox').focus();"
          "}"
          "function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;');}"
          "function sendCmd(){"
          "var box=document.getElementById('cmdbox');"
          "var cmd=box.value;"
          "if(!cmd)return;"
          "document.getElementById('cli-sugg').innerHTML='';"
          "fetch('/api/cli',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
          "body:'cmd='+encodeURIComponent(cmd)})"
          ".then(function(r){return r.json();})"
          ".then(function(d){"
          "var log=document.getElementById('cli-log');"
          "log.innerHTML+=\"<span class='cmd'>\"+esc(d.cmd)+\"</span>\";"
          "log.innerHTML+=\"<span class='rep'>\"+esc(d.reply)+\"</span>\";"
          "log.scrollTop=log.scrollHeight;"
          "box.value='';box.focus();"
          "}).catch(function(){"
          "document.getElementById('cli-log').innerHTML+=\"<span class='rep'>(request failed)</span>\";"
          "});"
          "}"
          "</script>";

  // ---- Auto-refresh: polls /api/status every 5s and patches the Radio/
  // MQTT/WiFi/Stats fields in place, no reload, no interruption of the CLI
  // log ----
  html += "<script>"
  "function setv(id,val){var e=document.getElementById(id);if(e)e.textContent=val;}"
  "function poll(){"
  "fetch('/api/status').then(function(r){return r.json();}).then(function(d){"
  "setv('v-repeat',d.radio.repeat);"
  "setv('v-tx',d.radio.tx+' dBm');"
  "setv('v-freq',d.radio.freq+' MHz');"
  "setv('v-radio',d.radio.radio);"
  "setv('v-lat',d.radio.lat);"
  "setv('v-lon',d.radio.lon);"
  "if(d.mqtt){"
  "setv('v-mqtt-server',d.mqtt.server);"
  "setv('v-mqtt-port',d.mqtt.port);"
  "setv('v-mqtt-topic',d.mqtt.topic);"
  "}"
  "if(d.wifi){"
  "setv('v-wifi-ssid',d.wifi.ssid);"
  "setv('v-wifi-ip',d.wifi.ip);"
  "setv('v-wifi-rssi',d.wifi.rssi+' dBm');"
  "}"
  "setv('v-heap',d.stats.heap+' bytes');"
  "setv('v-uptime',d.stats.uptime+' s');"
  "}).catch(function(){});"
  "}"
  "setInterval(poll,5000);"
  "function doScan(){"
  "var box=document.getElementById('scan-results');"
  "box.innerHTML='<div class=\"kv\"><i>Scanning&hellip; (a few seconds)</i></div>';"
  "fetch('/api/wifiscan').then(function(r){return r.json();}).then(function(list){"
  "if(!list.length){box.innerHTML='<div class=\"kv\"><i>No networks found</i></div>';return;}"
  "box.innerHTML=list.map(function(n){"
  "return \"<div class='sugg' onclick=\\\"pickSsid('\"+n.ssid.replace(/'/g,\"&#39;\")+\"')\\\">\"+"
  "n.ssid+\" <span style='opacity:.6'>(\"+n.rssi+\" dBm)</span></div>\";"
  "}).join('');"
  "}).catch(function(){box.innerHTML='<div class=\"kv\"><i>Scan failed</i></div>';});"
  "}"
  "function pickSsid(s){document.getElementById('ssid-input').value=s;}"
  "</script>";

  html += "</main></body></html>";
  webServer.send(200, "text/html", html);
}

void handleWifiSsid() {
#ifdef WIFI_SSID
  if (!webAuthOk()) return;
  String ssid = webServer.arg("ssid");
  setWifiSSID(ssid.c_str());

  String html;
  sendHead(html, "WiFi updated");
  html += "<div class='card'><h2>WiFi</h2><div class='kv'><i>SSID set to</i><b>" + ssid + "</b></div>"
          "<a class='back' href='/'>&laquo; Back</a></div></main></body></html>";
  webServer.send(200, "text/html", html);
#endif
}

void handleWifiPwd() {
#ifdef WIFI_SSID
  if (!webAuthOk()) return;
  String pwd = webServer.arg("pwd");
  setWifiPwd(pwd.c_str());

  String html;
  sendHead(html, "WiFi updated");
  html += "<div class='card'><h2>WiFi</h2><div class='kv'><i>Password updated</i></div>"
          "<a class='back' href='/'>&laquo; Back</a></div></main></body></html>";
  webServer.send(200, "text/html", html);
#endif
}

// ---- JSON API (used by the async CLI console; the other tabs remain
// server-rendered on page load for simplicity) ----
void handleApiStatus() {
  if (!webAuthOk()) return;
  String j = "{";
  j += "\"radio\":{";
  j += "\"repeat\":\"" + jsonEscape(runCmd("get repeat")) + "\",";
  j += "\"tx\":\"" + jsonEscape(runCmd("get tx")) + "\",";
  j += "\"freq\":\"" + jsonEscape(runCmd("get freq")) + "\",";
  j += "\"radio\":\"" + jsonEscape(runCmd("get radio")) + "\",";
  j += "\"lat\":\"" + jsonEscape(runCmd("get lat")) + "\",";
  j += "\"lon\":\"" + jsonEscape(runCmd("get lon")) + "\"";
  j += "},";
#ifdef WITH_MQTT_BRIDGE
  j += "\"mqtt\":{";
  j += "\"server\":\"" + jsonEscape(runCmd("get mqtt.server")) + "\",";
  j += "\"port\":\"" + jsonEscape(runCmd("get mqtt.port")) + "\",";
  j += "\"topic\":\"" + jsonEscape(runCmd("get mqtt.topic")) + "\"";
  j += "},";
#endif
#ifdef WIFI_SSID
  j += "\"wifi\":{";
  j += "\"ssid\":\"" + jsonEscape(getWifiSSID()) + "\",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI());
  j += "},";
#endif
  j += "\"stats\":{";
  j += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"uptime\":" + String(millis() / 1000) + ",";
  j += "\"fw\":\"" + jsonEscape(String(the_mesh.getFirmwareVer())) + "\"";
  j += "}";
  j += "}";
  webServer.send(200, "application/json", j);
}

void handleApiCli() {
  if (!webAuthOk()) return;
  String cmdStr = webServer.arg("cmd");
  String reply = runCmd(cmdStr.c_str());
  String j = "{\"cmd\":\"" + jsonEscape(cmdStr) + "\",\"reply\":\"" + jsonEscape(reply) + "\"}";
  webServer.send(200, "application/json", j);
}

#ifdef WIFI_SSID
// Blocking scan (typically 2-4s) - acceptable since this is a manual,
// occasional action triggered by a button, not something polled
// periodically. Briefly interrupts the current WiFi connection while
// scanning other channels, which is normal ESP32 behaviour.
void handleApiWifiScan() {
  if (!webAuthOk()) return;
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  j += "]";
  WiFi.scanDelete();
  webServer.send(200, "application/json", j);
}
#endif

void handleWebCmd() {
  if (!webAuthOk()) return;

  String cmdStr = webServer.arg("cmd");
  String reply = runCmd(cmdStr.c_str());

  String html;
  sendHead(html, "Result");
  html += "<div class='card'><h2>Command console</h2>"
          "<div class='term'><span class='cmd'>" + cmdStr + "</span>"
          "<span class='rep'>" + reply + "</span></div>"
          "<a class='back' href='/'>&laquo; Back to status</a></div>"
          "</main></body></html>";
  webServer.send(200, "text/html", html);
}

void startWebConfig() {
  webServer.on("/", handleWebRoot);
  webServer.on("/cmd", HTTP_POST, handleWebCmd);
  webServer.on("/api/status", handleApiStatus);
  webServer.on("/api/cli", HTTP_POST, handleApiCli);
#ifdef WIFI_SSID
  webServer.on("/wifi/ssid", HTTP_POST, handleWifiSsid);
  webServer.on("/wifi/pwd", HTTP_POST, handleWifiPwd);
  webServer.on("/api/wifiscan", handleApiWifiScan);
#endif
  webServer.begin();
  Serial.println("Web config server started on port 80");
}
#endif

void halt() {
  while (1) ;
}

static char command[MAX_POST_TEXT_LEN+1];
#ifdef ETHERNET_ENABLED
static char ethernet_command[MAX_POST_TEXT_LEN+1];
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef WIFI_SSID
  connectWifi();
#endif

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) { halt(); }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Room ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;
#ifdef ETHERNET_ENABLED
  ethernet_command[0] = 0;
#endif

  sensors.begin();

  the_mesh.begin(fs);

#if defined(WITH_WEB_CONFIG) && !defined(DEBUG_SKIP_WEBSERVER)
  startWebConfig();
#endif

#if defined(DISPLAY_CLASS) && !defined(DEBUG_SKIP_UITASK)
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

#ifdef ETHERNET_ENABLED
  ethernet_start_task();
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();
}

void loop() {
#if !defined(DEBUG_SKIP_SERIALREAD)
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
    }
    Serial.print(c);
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    reply[0] = 0;
#ifdef ETHERNET_ENABLED
    if (!ethernet_handle_command(command, reply)) {
      the_mesh.handleCommand(0, command, reply);
    }
#else
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
#endif
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }
#endif

#ifdef ETHERNET_ENABLED
  ethernet_loop_maintain();
  if (ethernet_read_line(ethernet_command, sizeof(ethernet_command))) {
    char reply[160];
    reply[0] = 0;
    if (!ethernet_handle_command(ethernet_command, reply)) {
      the_mesh.handleCommand(0, ethernet_command, reply);
    }
    ethernet_send_reply(reply);
    ethernet_command[0] = 0;
  }
#endif

#if defined(WITH_WEB_CONFIG) && !defined(DEBUG_SKIP_WEBSERVER)
  webServer.handleClient();
#endif

  the_mesh.loop();
#if !defined(DEBUG_SKIP_SENSORS)
  sensors.loop();
#endif
#if defined(DISPLAY_CLASS) && !defined(DEBUG_SKIP_UITASK)
  ui_task.loop();
#endif
#if !defined(DEBUG_SKIP_RTCTICK)
  rtc_clock.tick();
#endif
#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif
}
