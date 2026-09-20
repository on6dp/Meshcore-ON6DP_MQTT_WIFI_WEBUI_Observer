# 🎙️ ON6DP Firmware — Customized MeshCore Room Server

🇫🇷 Version française | 🇬🇧 English version

This repository is a customized fork of MeshCore, maintained by ON6DP (Belgian amateur radio operator, Liège region). 
The original MeshCore project is untouched — this file only documents what was added on top of it.

🇧🇪 What this firmware adds to the base MeshCore Room Server

Flashed on a Heltec V4 (ESP32-S3, 868 MHz), this firmware turns a standard MeshCore Room Server into a remotely manageable, MQTT-connected observer, adding:

\- \*\* WiFi  Fully hot-swappable WiFi (NVS-backed) — no reflashing needed to change networks, including a network scan directly from the web page
\- \*\* Built-in MQTT bridge — publishes all observed mesh traffic to an MQTT broker (server/port/topic all hot-configurable), with:
&#x20;		Correct hex encoding of raw packets (compatible with CoreScope)
&#x20;		Periodic status heartbeat (origin, firmware, client_version), so the device shows up named in CoreScope's Observers tab
&#x20;		Automatic, periodic promotional post on the mesh network itself (BBS Room Server), inviting other operators to join the same broker

\- \*\* 5-tab web config page (Radio / MQTT / WiFi / Stats / CLI), inspired by gessaman's observer:

&#x20;		Fully async CLI console (JSON API, persistent history, no page reloads)
&#x20;		Autocomplete across ~40 CLI commands
&#x20;		Auto-refreshing display of all values

\- \*\* Enhanced OLED screen — continuously shows the IP address and firmware version, alongside the standard radio info

## 📚 Documentation
examples/simple_room_server/TUTO_FLASH_HELTEC_ON6DP.md — complete flashing tutorial, written for absolute beginners (Python, Git, PlatformIO Core, no VS Code required). French only for now.
examples/simple_room_server/COMMANDES_CLI_ON6DP.md — full CLI command reference. French only for now.

##🔌 Hardware compatibility

This firmware was built and tested specifically on the Heltec V4 (ESP32-S3). Portability to other boards depends on the chip family:

Other ESP32 boards (other Heltec models, T-Beam, etc.): the code (MqttBridge, WiFi/NVS handling, web page) relies on generic ESP32 libraries (WiFi.h, Preferences.h, WebServer.h), so it's portable in principle — but not usable out of the box: a new PlatformIO environment needs to be created, extending the target board's base definition instead of heltec_v4_oled.
Non-ESP32 boards (nRF52840 — SenseCAP T1000-E, ThinkNode M6, SenseCAP Solar Node P1-Pro...): not compatible, and this is a hardware limitation, not a code one — these chips have no WiFi radio, so none of this firmware's WiFi/MQTT/web features are physically possible on them. These boards remain usable as standard MeshCore Room Servers/Repeaters (LoRa only), just without these additions.
🛠️ Build environment

The main environment is heltec_v4_room_server_wifi_mqtt, defined in variants/heltec_v4/platformio.ini.

bash
git clone https://github.com/on6dp/Meshcore-ON6DP_MQTT_WIFI_WEBUI_Observer.git
cd Meshcore-ON6DP_MQTT_WIFI_WEBUI_Observer
pio run -e heltec_v4_room_server_wifi_mqtt -t upload --upload-port COMx

⚠️ On some PC/cable combinations, flashing may fail with No serial data received. Adding upload_flags = --no-stub and upload_speed = 115200 to the environment fixes this (already in place in this repo).

🙏 Credits

This firmware builds entirely on the work of the MeshCore project and its community. The additions documented here are local customizations, shared in the same open-source spirit as the original project.

73, Paul — ON6DP



# 🎙️ Firmware ON6DP — MeshCore Room Server personnalisé

Ce dépôt est un \*\*fork personnalisé\*\* de \[MeshCore](https://github.com/meshcore-dev/MeshCore),maintenu par \*\*ON6DP\*\* (radioamateur belge, région Liège). 
Le projet MeshCore original reste inchangé — ce fichier documente uniquement \*\*ce qui a été ajouté\*\* par rapport à l'original.


\## 🇧🇪 Ce que ce firmware ajoute au Room Server MeshCore de base:

Flashé sur un \*\*Heltec V4 (ESP32-S3, 868 MHz)\*\*, ce firmware transforme un Room Server MeshCore standard en \*\*observateur MQTT connecté et administrable à distance\*\*, avec :

\- \*\*WiFi entièrement modifiable à chaud\*\* (NVS) — plus besoin de reflasher pour changer de réseau, y compris un scan des réseaux disponibles directement depuis la page web
\- \*\*Bridge MQTT intégré\*\* — publie tout le trafic mesh observé vers un broker MQTT (serveur/port/topic modifiables à chaud), avec :

&#x20; 		- Encodage hexadécimal correct des paquets (compatible \[CoreScope](https://github.com/Kpa-clawbot/CoreScope))
&#x20; 		- Message de statut périodique (`origin`, `firmware`, `client\_version`), pour apparaître nommé dans l'onglet Observers de CoreScope
&#x20; 		- Publication automatique et périodique d'un message promotionnel sur le réseau mesh (BBS Room Server), pour inviter d'autres opérateurs à rejoindre le même broker

\- \*\*Page web de configuration à 5 onglets\*\* (Radio / MQTT / WiFi / Stats / CLI), inspirée de \[gessaman's observer](https://observer.gessaman.com) :

&#x20; 		- Console CLI asynchrone (API JSON, historique persistant, pas de rechargement de page)
&#x20; 		- Autocomplétion sur \~40 commandes CLI
&#x20; 		- Rafraîchissement automatique de toutes les valeurs affichées

\- \*\*Écran OLED enrichi\*\* — affiche en continu l'adresse IP et la version du firmware, en plus des infos radio standard

## 📚 Documentation

\- \[`examples/simple\_room\_server/TUTO\_FLASH\_HELTEC\_ON6DP.md`](examples/simple\_room\_server/TUTO\_FLASH\_HELTEC\_ON6DP.md)
&#x09;— tutoriel complet pour flasher ce firmware, pensé pour des débutants complets (Python, Git, PlatformIO Core, sans VS Code requis)

\- \[`examples/simple\_room\_server/COMMANDES\_CLI\_ON6DP.md`](examples/simple\_room\_server/COMMANDES\_CLI\_ON6DP.md)
&#x20; 	— référence complète des commandes CLI disponibles


## 🔌 Compatibilité matérielle

Ce firmware a été développé et testé **spécifiquement sur Heltec V4**(ESP32-S3). Sa portabilité vers d'autres cartes dépend du **type de puce** :

- **Autres cartes ESP32** (autres modèles Heltec, T-Beam, etc.) : le code   (`MqttBridge`, gestion WiFi/NVS, page web) repose sur des bibliothèques ESP32 **génériques** (`WiFi.h`,
   `Preferences.h`, `WebServer.h`), donc **portable en principe** — mais pas prêt à l'emploi tel quel : il faut créer un nouvel environnement dans `platformio.ini`, qui étend la
   définition de la carte cible au lieu de `heltec_v4_oled`.

- **Cartes non-ESP32** (nRF52840 — SenseCAP T1000-E, ThinkNode M6, SenseCAP Solar Node P1-Pro...) : **incompatible**, et ce n'est pas une limite du code mais du matériel.
   Ces puces **n'ont pas de WiFi**, donc aucune des fonctionnalités WiFi/MQTT/page web de ce firmware n'est physiquement possible dessus. Ces cartes restent utilisables en Room
   Server/Repeater MeshCore standard (LoRa pur), juste sans ces ajouts.

## 🛠️ Environnement de compilation

L'environnement principal est `heltec\_v4\_room\_server\_wifi\_mqtt`, défini dans \[`variants/heltec\_v4/platformio.ini`](variants/heltec\_v4/platformio.ini).

```bash

git clone https://github.com/on6dp/Meshcore-ON6DP\_MQTT\_WIFI\_WEBUI\_Observer.git
cd Meshcore-ON6DP\_MQTT\_WIFI\_WEBUI\_Observer
pio run -e heltec\_v4\_room\_server\_wifi\_mqtt -t upload --upload-port COMx

```
⚠️ Sur certains PC/câbles, le flash peut échouer avec `No serial data received`. Ajouter `upload\_flags = --no-stub` et `u1pload\_speed = 115200` à l'environnement résout ce problème (déjà en place dans ce dépôt).


\## 🙏 Crédits



Ce firmware s'appuie entièrement sur le travail du projet \[\*\*MeshCore\*\*](https://github.com/meshcore-dev/MeshCore) et de sa communauté. Les ajouts documentés ici sont des personnalisations locales, partagées dans l'esprit open-source du projet d'origine.



\---



\*73, Paul — ON6DP\*

