# Chantier : interface web avancée pour Heltec V4 (inspirée d'observer.gessaman.com)

## État actuel (mise à jour) — CSS retravaillé
Le style de la page a été retravaillé (palette "poste radio" — vert forêt/
ambre signal au lieu du bleu SaaS générique, petite icône antenne SVG dans
l'en-tête). Toujours 2 onglets (Status/Console) à ce stade.

## ✅ CHANTIER TERMINÉ (session du lendemain) — récapitulatif
Toutes les étapes prévues ont été complétées avec succès :

1. **WiFi modifiable à chaud** — même pattern NVS/`Preferences` que MQTT
   (`get/set wifi.ssid`, `set wifi.pwd`), fonctions `getWifiSSID()`/
   `setWifiSSID()`/`setWifiPwd()` dans `main.cpp`, déclarées `extern` dans
   `MyMesh.h` pour être appelables depuis `MyMesh.cpp` (CLI).
2. **5 onglets** — Radio / MQTT / WiFi / Stats / CLI (au lieu de
   Status/Console). Navigation JS généralisée (boucle sur un tableau de
   panneaux, plus de code dupliqué par onglet).
3. **Onglet WiFi fonctionnel** — affichage SSID/IP/RSSI en direct, deux
   formulaires dédiés (`/wifi/ssid`, `/wifi/pwd`) qui appellent les setters
   directement, sans syntaxe CLI à connaître.
4. **Autocomplétion CLI** — liste JS d'une quarantaine de commandes
   (identité, radio, comportement mesh, MQTT, WiFi, divers), filtrage en
   direct, suggestions cliquables.
5. **API JSON** — `/api/status` (GET, toutes les données) et `/api/cli`
   (POST, exécute une commande). Console CLI réécrite en JS pur (`fetch`),
   plus de rechargement de page — historique cumulatif comme un vrai
   terminal, Entrée pour envoyer.
6. **Rafraîchissement automatique** — `poll()` interroge `/api/status`
   toutes les 5 secondes et met à jour Radio/MQTT/WiFi/Stats en place
   (`setv()` cible chaque valeur par son `id`), sans jamais recharger la
   page ni perturber la console CLI en cours.

**Non fait, restant en option pour une prochaine fois** :
- ~~Scan WiFi~~ — **fait** : bouton "Scan networks" dans l'onglet WiFi,
  `WiFi.scanNetworks()` via `/api/wifiscan`, liste cliquable (SSID + RSSI)
  qui pré-remplit le champ SSID. Testé et confirmé fonctionnel (`on6dp`
  détecté deux fois, plus un réseau voisin).

**🎉 Chantier interface web avancée : entièrement terminé.** Les 6 points
du plan initial (WiFi NVS, 5 onglets, WiFi fonctionnel, autocomplétion,
API JSON + CLI async, rafraîchissement auto) + le scan WiFi sont tous
faits, testés et validés.

## Plan précisé pour les 5 onglets (inspiré de gessaman)
Structure cible confirmée : **Radio / MQTT / WiFi / Stats / CLI** (au lieu
des 2 onglets actuels Status/Console).

- **Radio, MQTT, Stats** : affichage seul, faisable directement avec les
  commandes CLI déjà existantes (`get radio`, `get tx`, `get mqtt.server`,
  etc.) — pas de nouveau travail côté firmware, juste réorganisation de la
  page HTML/JS.
- **CLI** : renommage de l'onglet Console actuel, + autocomplétion (liste
  JS des commandes connues, filtrage en tapant) — travail HTML/JS pur.
- **WiFi** : **distinction importante**. Afficher les infos (SSID actuel,
  IP, RSSI) est facile (déjà disponible). Mais **changer** le réseau WiFi
  depuis la page nécessite un vrai travail de fond au préalable — le WiFi
  est actuellement codé en dur au moment de la compilation
  (`-D WIFI_SSID`/`-D WIFI_PWD`), **pas stocké en NVS** comme le sont les
  réglages MQTT. Il faut d'abord appliquer le même pattern NVS/`Preferences`
  qu'on a fait pour MQTT (`get/set wifi.ssid`, `get/set wifi.pwd` en CLI,
  avec reconnexion à chaud) avant de pouvoir proposer un vrai formulaire de
  changement de réseau dans l'onglet WiFi.

**Ordre recommandé pour la prochaine session** :
1. Rendre le WiFi modifiable à chaud (NVS, comme MQTT) — le vrai
   prérequis technique
2. Restructurer la page en 5 onglets (Radio/MQTT/WiFi/Stats/CLI)
3. Ajouter l'autocomplétion CLI
4. Si le temps le permet : API JSON + JS pour mise à jour sans rechargement

## Contexte
Suite au projet Heltec V4 (Room Server + WiFi + Observer MQTT + Repeat + page
web simple), l'objectif est de se rapprocher de la qualité d'interface du
firmware "observer" d'agessaman (https://observer.gessaman.com), sans
forcément tout reproduire à l'identique.

Référence exacte visée : la page de config du firmware
`observer-heltec-v4/room-server/v1.17.1.3` sur observer.gessaman.com — cartes,
onglets (Radio/MQTT/WiFi/Stats/CLI), thème clair/sombre auto, terminal CLI
avec autocomplétion, tout piloté par une API JSON.

## État actuel (fin de session) — déjà en place et fonctionnel
- Firmware Heltec V4 : Room Server + WiFi (SSID `on6dp`) + Observer MQTT +
  Repeat activé par défaut
- Page web de config actuelle (`http://192.168.50.217/`) : 2 onglets
  (Statut / Console), thème clair/sombre auto, cartes, style inspiré de
  gessaman en CSS pur — mais fonctionnement **synchrone** (formulaire POST +
  rechargement de page), pas d'API JSON
- MQTT désormais **modifiable à chaud** (pas juste compile-time) :
  - `get/set mqtt.server`, `get/set mqtt.port`, `get/set mqtt.topic`
  - Stockage en NVS (`Preferences` library), namespace `mqttcfg`
  - Reconnexion automatique au changement, pas de reboot nécessaire
- Coordonnées GPS configurées (`set lat`, `set lon`)
- Fichiers concernés : `src/helpers/bridges/MqttBridge.h/.cpp`,
  `examples/simple_room_server/MyMesh.cpp` (bloc `mqtt.*` dans
  `handleCommand()`), `examples/simple_room_server/main.cpp` (page web)

## Ce qui manque pour se rapprocher de gessaman

### 1. Réglages runtime généralisés (actuellement MQTT seulement)
Étendre le pattern NVS déjà utilisé pour MQTT à d'autres réglages
actuellement figés à la compilation :
- WiFi SSID/password (actuellement `-D WIFI_SSID`/`-D WIFI_PWD` en dur)
- Permettrait un vrai changement de réseau WiFi depuis la page web, sans
  reflasher

### 2. Vraie API JSON + JS côté client
Remplacer le formulaire HTML classique (rechargement de page) par :
- Endpoints JSON (`/api/status`, `/api/config`, `/api/cli`...)
- JavaScript côté navigateur pour mise à jour dynamique sans rechargement
- Gessaman utilise un système asynchrone avec `reqid` + polling (le firmware
  traite les commandes en file d'attente) — on pourrait rester plus simple
  avec de l'AJAX synchrone classique (`fetch()` + réponse immédiate), plus
  simple à implémenter sur notre architecture actuelle

### 3. Scan WiFi
`WiFi.scanNetworks()` existe nativement dans l'API Arduino ESP32 — faisable
sans dépendance externe. Permettrait de choisir son réseau WiFi depuis une
liste plutôt que de taper le SSID à la main.

### 4. Design visuel complet
Le CSS de la page gessaman (variables de couleur, cartes, onglets, dark mode)
est un fichier autonome réutilisable presque tel quel — la partie la plus
facile à reprendre en premier si on veut un résultat visuel proche
rapidement, indépendamment des points 1-3.

### 5. Fonctionnalités non prioritaires (probablement hors scope)
- Les 30+ presets MQTT communautaires (analyzer-us, meshtexas, etc.) — pas
  pertinent pour un usage perso avec un seul broker
- Authentification par session/cookies (on garde HTTP Basic Auth, suffisant
  pour un usage sur réseau local de confiance)
- Graphiques sparkline (heap, bruit radio) — cosmétique, à voir en dernier

## Suggestion d'ordre de travail pour la prochaine session
1. Reprendre le CSS de gessaman tel quel (rapide, gain visuel immédiat)
2. Étendre le stockage NVS au WiFi (SSID/password modifiables à chaud)
3. Ajouter un scan WiFi basique
4. Si le temps le permet : passer à une API JSON + JS pour une UX plus fluide

## Référence Heltec V4 (pour rappel)
- Environnement PlatformIO : `heltec_v4_room_server_wifi_mqtt`
- WiFi : SSID `on6dp`
- MQTT (modifiable à chaud) : `mqtt.no-ip.org:1883`, topic
  `meshcore/heltec_v4/rx`
- Page web : `http://192.168.50.194/` (auth admin) — IP dynamique, peut
  changer après un reboot ; envisager une réservation DHCP sur la box pour
  la fixer (MAC : `10:bd:a3:5c:0c:bc`)
- Sauvegardes binaires : `C:\Backups\heltec_v4_room_server_wifi_mqtt\`
