# Firmware ON6DP sur un HELTEC V4 868 MHz

## Que fait ce firmware ?

### 1.) Mesh radio LoRa
Rôle Room Server MeshCore complet : il reçoit, traite et relaie le trafic mesh de la région.
- Repeat activé : relaie le trafic des autres opérateurs
- Coordonnées GPS configurées (annonces localisées)
- BBS/Room Server : stockage et diffusion de messages

### 2.) WiFi
Connexion réseau entièrement modifiable à chaud, sans jamais reflasher.
- SSID/mot de passe stockés en NVS (persistants)
- Reconnexion automatique en cas de coupure
- Scan des réseaux disponibles depuis la page web

### 3.) Observateur MQTT
Publie tout le trafic mesh observé vers le broker MQTT, avec réglages modifiables à chaud (serveur/port/topic).
- MQTT/Broker : `mqtt.no-ip.org:1883`, topic `meshcore/heltec_v4/rx`
- Alimente CoreScope et MeshMap sur le VPS
- Délai de reconnexion optimisé (300ms) — bug de fond connu mais atténué

### 4.) Page web
Interface complète avec 5 onglets (inspirée du firmware de Gessaman) accessible sur le réseau local.
- Onglets : Radio / MQTT / WiFi / Stats / CLI
- Console CLI asynchrone avec historique et auto-complétion (~40 commandes)
- Rafraîchissement automatique de toutes les valeurs (toutes les 5s)

### 5.) Écran OLED
Affichage local direct sur l'appareil, indépendant du réseau.
- Statut et informations du nœud en direct
- Extinction automatique après 20s (réveil via bouton PRG)

### 6.) Sauvegarde du code
Tout le firmware personnalisé est versionné et sauvegardé.
- Dépôt Git local + historique complet des modifications
- Poussé sur GitHub privé (sauvegarde externe)

---

## Comment flasher un HELTEC V4 868 MHz avec le firmware de ON6DP

*(Ce tuto est rédigé pour installer le firmware à partir d'un PC sous Windows 11, même si tu n'as jamais compilé ni flashé quoi que ce soit avant.)*

### 1.) Installer Python
Télécharger et installer Python depuis [python.org](https://www.python.org/downloads/windows/) si ce n'est pas déjà fait sur ce PC (vérifie avec `python --version` dans un terminal).
C'est nécessaire car PlatformIO s'installe via l'outil `pip`, qui vient avec Python.

### 2.) Installer Git
Télécharger et installer Git depuis [git-scm.com](https://git-scm.com/).
C'est nécessaire pour récupérer (cloner) le dépôt GitHub sur le nouveau PC.

### 3.) Installer PlatformIO Core
Dans un terminal (PowerShell ou CMD en administrateur) :
```
pip install platformio
```
C'est l'outil qui sert à compiler et flasher le firmware sur le Heltec V4 (fonctionne en ligne de commande pure, sans avoir besoin de VS Code).
Vérifie l'installation avec :
```
pio --version
```

### 4.) Cloner le dépôt GitHub ON6DP
Dans un terminal PowerShell :
```
git clone https://github.com/on6dp/Meshcore-ON6DP_MQTT_WIFI_WEBUI_Observer.git
cd Meshcore-ON6DP_MQTT_WIFI_WEBUI_Observer
```
On récupère ainsi tout le code source et la configuration des environnements (dont `heltec_v4_room_server_wifi_mqtt_ami`).

---

### ⚠️ IMPORTANT — Lis ceci avant de continuer si c'est pour une AUTRE personne que Tilto

Le dépôt contient déjà un réglage tout prêt, nommé `heltec_v4_room_server_wifi_mqtt_ami`, **réservé à Tilto**. Il contient une "étiquette" unique (un identifiant) qui permet à son appareil de ne jamais se mélanger avec celui de ON6DP sur le réseau.

**Si tu flashes ce firmware pour une TROISIÈME personne** (ni ON6DP, ni Tilto), il faut lui créer sa propre étiquette unique — sinon son appareil et celui de Tilto vont "se battre" en boucle sur le réseau et aucun des deux ne fonctionnera correctement.

**Voici comment faire, très simplement, sans rien comprendre au code :**

1. Ouvre le fichier `variants\heltec_v4\platformio.ini` avec le Bloc-notes Windows (clic droit sur le fichier → Ouvrir avec → Bloc-notes)
2. Trouve tout en bas de ce fichier ce bloc de texte (celui de Tilto) :
   ```
   [env:heltec_v4_room_server_wifi_mqtt_ami]
   extends = heltec_v4_oled
   build_flags =
     ${heltec_v4_oled.build_flags}
     -D DISPLAY_CLASS=SSD1306Display
     -D MESH_PACKET_LOGGING=1
     -D WIFI_SSID='"on6dp"'
     -D WIFI_PWD='"On6dp_Op0p"'
     -D WITH_MQTT_BRIDGE
     -D MQTT_SERVER='"mqtt.no-ip.org"'
     -D MQTT_PORT=1883
     -D MQTT_TOPIC='"meshcore/heltec_v4_ami/rx"'
     -D MQTT_CLIENT_ID='"heltec_v4_ami_observer"'
     -D WITH_WEB_CONFIG
   build_src_filter = ${heltec_v4_oled.build_src_filter}
     +<helpers/bridges/MqttBridge.cpp>
     +<helpers/ui/SSD1306Display.cpp>
     +<helpers/esp32/*.cpp>
     +<../examples/simple_room_server/*.cpp>
   lib_deps =
     ${heltec_v4_oled.lib_deps}
     densaugeo/base64 @ ~1.4.0
     knolleary/PubSubClient @ ^2.8
   ```
3. **Sélectionne tout ce bloc, copie-le (Ctrl+C), colle-le juste en dessous (Ctrl+V)** — tu as maintenant deux fois le même bloc
4. Dans la **copie du bas** uniquement, remplace `ami` par un mot différent — par exemple `ami2` — à **exactement 3 endroits** (utilise Ctrl+H, "Rechercher/Remplacer", en le faisant une seule fois dans la copie du bas) :
   - `[env:heltec_v4_room_server_wifi_mqtt_ami2]`
   - `-D MQTT_TOPIC='"meshcore/heltec_v4_ami2/rx"'`
   - `-D MQTT_CLIENT_ID='"heltec_v4_ami2_observer"'`
5. Enregistre le fichier (Ctrl+S)
6. Utilise ce **nouveau nom** (`heltec_v4_room_server_wifi_mqtt_ami2`) à la place de `_ami` dans toutes les commandes `pio run -e ...` qui suivent dans ce tuto

Répète cette astuce (`ami3`, `ami4`...) à chaque nouvelle personne à qui tu veux flasher ce firmware.

---

### 5.) Branchement
Connecter le Heltec V4 en USB sur le PC.
Ouvrir le Gestionnaire de périphériques Windows, section « Ports (COM & LPT) », et noter le numéro de port qui apparaît (ex: COM3, COM5...).

### 6.) Compiler et flasher
Depuis le dossier cloné, en mode terminal :
```
pio run -e heltec_v4_room_server_wifi_mqtt_ami -t upload --upload-port COMx
```
*(remplacer `COMx` par le vrai port COM trouvé, et `_ami` par le bon nom d'environnement si ce n'est pas pour Tilto — voir l'encadré ci-dessus)*

La toute première compilation sur ce PC prendra plusieurs minutes (téléchargement des outils/bibliothèques par PlatformIO).
**NB : on a besoin d'une bonne connexion Internet à ce moment-là uniquement.**

### 7.) Vérifier le démarrage
Ouvrir le moniteur série :
```
pio device monitor -p COMx -b 115200
```
Laisser tourner pendant 30 secondes pour confirmer que ça démarre bien : connexion WiFi réussie, puis tentative de connexion MQTT avec l'identifiant configuré (ex: `heltec_v4_ami_observer`).

### 8.) Configurer son identité propre
Via la console CLI (l'onglet "CLI" de la page web) ou le moniteur série, donner un nom et des coordonnées propres à ce nœud :
```
set name <Nom du node>
set lat  <Coordonnées de latitude en décimal>
set lon  <Coordonnées de longitude en décimal>
```
Sinon le nœud pourrait prêter à confusion avec d'autres sur les cartes/dashboards de CoreScope ou MeshMap (`http://meshcore.no-ip.org` ou `http://meshcore.no-ip.org:8080`).

### 9.) Configurer le réseau WiFi à domicile
Dans l'onglet **WiFi** de la page web, utiliser le bouton **« Scan networks »** pour connaître les réseaux WiFi disponibles.
Sélectionner le réseau adéquat (ça remplit automatiquement le champ SSID), puis taper le mot de passe correspondant dans le champ prévu. Ce sera stocké en mémoire (NVS) et l'appareil s'y connectera automatiquement, même après un redémarrage.

### 10.) Activer la fonction "Repeat"
Si tu veux que le Heltec V4 relaie aussi le trafic mesh d'autres opérateurs, dans la console CLI :
```
set repeat on
```
puis vérifie que la commande est bien passée :
```
get repeat
```

### 11.) Vérifier sur CoreScope
Une fois tout configuré, vérifie sur `http://meshcore.no-ip.org` que l'identifiant de ce nœud (ex: `heltec_v4_ami_observer`) apparaît bien comme client **distinct**, sans conflit avec un autre appareil déjà connu.

### 12.) Page web
Dans un navigateur, indiquer l'adresse IP du Heltec, donnée par le DHCP du réseau WiFi local à la maison/au travail (cette adresse est aussi affichée au démarrage dans le moniteur série).
Les 5 onglets sont disponibles sur cette page.
Le réseau WiFi peut être changé si le Heltec est déplacé ailleurs plus tard, ou si la box internet change — il n'y aura plus jamais besoin de reflasher le firmware pour ça.

---

Bon amusement !

73, Paul, ON6DP
