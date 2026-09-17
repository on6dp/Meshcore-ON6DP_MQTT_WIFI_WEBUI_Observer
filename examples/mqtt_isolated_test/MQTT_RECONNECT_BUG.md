# Bug : reconnexion MQTT en boucle toutes les ~5 secondes (Heltec V4)

## Symptôme
Le Heltec V4 (firmware Room Server + WiFi + MQTT observer) se connecte avec
succès au broker MQTT (`mqtt.no-ip.org:1883`), mais la connexion tombe très
précisément **toutes les ~5000ms**, en boucle continue, depuis le tout début
de sa mise en service.

Logs typiques côté Heltec (moniteur série) :
```
MqttBridge: connecting to mqtt.no-ip.org:1883 (state before=-3)...
MqttBridge: connect() returned 1, state after=0, WiFi RSSI=-70
MqttBridge: connection DROPPED at t=10250 ms, state=-3
MqttBridge: connecting to mqtt.no-ip.org:1883 (state before=-3)...
MqttBridge: connect() returned 1, state after=0, WiFi RSSI=-71
MqttBridge: connection DROPPED at t=15258 ms, state=-3
```
Écarts entre les DROPPED : 10250 → 15258 → 20303 → 25256 → 30251
(deltas ~5000-5050ms, remarquablement réguliers).

Côté broker (`docker logs corescope`), chaque cycle produit **deux** lignes
de connexion (deux ports source différents, même timestamp) :
```
1789482702: New connection from 169.155.241.197:XXXXX on port 1883.
1789482702: Client heltec_v4_observer already connected, closing old connection.
1789482702: New client connected from 169.155.241.197:XXXXX as heltec_v4_observer (p2, c1, k15).
```
`k15` = keepalive MQTT de 15s (valeur par défaut PubSubClient, jamais changée
explicitement).

## Découverte majeure (capture tcpdump)
Une capture réseau bas niveau sur le VPS (`tcpdump -i any -nn -tttt 'port 1883
and host <IP box>'`) révèle un fait déterminant : **aucun `RST` ni `FIN` n'est
envoyé par le Heltec au moment où la connexion "meurt"**. Le handshake TCP se
déroule normalement (SYN → SYN-ACK → CONNECT MQTT 32 octets → CONNACK 4
octets → ACK du CONNACK), puis le socket reste silencieux — sans fermeture
propre ni coupure réseau visible — jusqu'à ce qu'un **tout nouveau SYN**
apparaisse (nouveau port source), abandonnant l'ancienne connexion sans
jamais la refermer proprement.

**Conclusion** : le problème n'est ni réseau, ni broker, ni box/NAT — il est
**purement logiciel, côté ESP32**. `_mqtt.connected()` doit basculer à
`false` très rapidement après le succès du `connect()` (probablement en
quelques millisecondes, pas 5 secondes comme on le pensait) ; c'est notre
propre throttle de 5000ms dans `ensureConnected()` qui masque ce fait en
retardant le prochain print/essai, donnant l'illusion trompeuse d'une
connexion qui "tient" ~5s. Aucun `PINGREQ` n'est d'ailleurs jamais observé
dans la capture — cohérent avec `_mqtt.loop()` qui cesse d'être exécuté utile
dès que `connected()` ment sur l'état réel.

## RÉSULTAT DÉCISIF : test isolé (sans MeshCore)
Un firmware minimal (`examples/mqtt_isolated_test/main.cpp`, env
`heltec_v4_mqtt_isolated_test`) reproduisant uniquement WiFi + PubSubClient
+ la même logique `ensureConnected()`/throttle 5s que `MqttBridge`, **sans
aucun code MeshCore** (pas de radio LoRa, pas de Room Server, pas d'écran),
a été flashé et testé sur le même Heltec V4, même réseau, même broker.

**Résultat : connexion parfaitement stable pendant plusieurs minutes, aucune
ligne `DROPPED`.** Le cycle de reconnexion de 5s **ne se reproduit pas** en
isolation.

**Conclusion définitive** : le bug n'est ni dans PubSubClient, ni dans
WiFiClient, ni dans le core ESP32-Arduino pris isolément — c'est une **vraie
interaction avec le reste du firmware MeshCore**. Suspects principaux,
par ordre de probabilité :
1. **La radio LoRa (SX126x via SPI)** — écoute continue (CAD/RX), même avec
   `repeat off`, pourrait interférer avec le driver WiFi/TCP de façon
   subtile (contention SPI/interruptions, timing partagé sur l'ESP32-S3).
2. **La charge de la boucle principale `MyMesh::loop()`** — l'ensemble
   Room Server + Dispatcher + CommonCLI + UITask pourrait retarder l'appel
   à `_mqtt.loop()` suffisamment pour perturber PubSubClient, même si le
   symptôme observé (aucun RST/FIN, connexion muette) ne ressemble pas à un
   simple problème de fréquence d'appel.

## Prochaine étape recommandée
Réintroduire les composants un par un dans le sketch isolé (d'abord juste
l'init radio SX126x sans trafic, puis le Dispatcher, puis Room Server
complet) pour identifier précisément lequel déclenche le bug — plutôt que
de deviner. Le sketch isolé actuel est le point de départ idéal pour cette
approche incrémentale.

**Tentative en cours (session actuelle) — non concluante, à reprendre :**
Un second sketch isolé a été créé (`examples/mqtt_isolated_radio_test/main.cpp`,
env `heltec_v4_mqtt_isolated_radio_test`) ajoutant `board.begin()` +
`radio_init()` (radio LoRa active, sans aucun traitement de paquets) au
premier test isolé. Compilation et flash réussis. Le moniteur série a
confirmé qu'un vrai redémarrage avait bien eu lieu (`free heap` différent
d'un essai à l'autre), mais impossible de récupérer les toutes premières
lignes de boot (bannière, `Radio init OK/FAILED`, `Connecting WiFi...`) —
soit perdues avant ouverture du moniteur, soit hors d'écran.

**Blocage technique résolu** : un redémarrage complet de Windows a bien réglé
le problème de pilote USB-série qui empêchait le moniteur série de
fonctionner en fin de session précédente.

## Extension envisagée (phase 2, après résolution du bug ci-dessus)
Ajouter un **second bridge MQTT** en parallèle du premier, dédié à LetsMesh
(`mqtt-eu-v1.letsmesh.net:443`), avec :
- Connexion WSS (WebSocket + TLS) au lieu du TCP brut actuel
- Génération de token JWT signé avec la clé Ed25519 de l'appareil (la
  librairie `ed25519` est déjà liée dans le build — vue dans les logs de
  compilation — mais la logique de construction du JWT reste à écrire)
- Format de topic spécifique : `meshcore/{IATA}/{clé_publique}/status` et
  `.../packets`
- Nécessite un compte sur `forum.letsmesh.net`, avec l'email lié comme
  "Owner Email"

**Préparation déjà faite (prête pour la prochaine session)** :
- ✅ Compte créé sur `forum.letsmesh.net`
- ✅ Owner Email : `on6dp@on6dp.be`
- ✅ Code IATA choisi : `LGG` (Liège)
- ✅ Broker cible : `mqtt-eu-v1.letsmesh.net:443` (EU, pertinent pour la Belgique)

**Important** : ne pas tenter cette extension avant d'avoir résolu le bug
de reconnexion de base ci-dessus — ajouter une deuxième connexion MQTT (plus
complexe : TLS+JWT) sur un firmware déjà instable sur ce point risquerait de
compliquer le diagnostic plutôt que de le simplifier.

**Résultat confirmé (après redémarrage Windows, pilote USB réparé)** : le
firmware `heltec_v4_mqtt_isolated_radio_test` a tourné plusieurs minutes,
observé à deux reprises, avec `Radio init OK` et **aucune ligne `DROPPED`**
— connexion parfaitement stable. Résultat reproductible.

**Test suivant — WebServer désactivé sur le vrai firmware Room Server** :
en utilisant le flag `DEBUG_SKIP_WEBSERVER` (nouveau, ajouté ce jour) sur le
firmware complet `heltec_v4_room_server_wifi_mqtt` (radio + Dispatcher +
Room Server + MQTT, mais sans serveur web), **le cycle de 5s persiste à
l'identique** (115326 → 120318 ms, delta ~4992ms). Le serveur web est donc
**écarté** — ce n'est pas lui.

**Test suivant — Écran OLED (UITask) désactivé** : même principe avec un
nouveau flag `DEBUG_SKIP_UITASK`, serveur web réactivé, écran désactivé.
**Le cycle persiste à l'identique** (70480 → 75486 ms, delta ~5006ms).
L'écran OLED est donc **également écarté**.

**Test suivant — Minuteurs d'annonces désactivés** (sans recompilation,
juste via CLI) : `set advert.interval 0` + `set flood.advert.interval 0`
sur le build avec écran désactivé. **Aucun effet** — cycle observé sur 24
itérations consécutives (60s à 180s), toujours ~5000ms pile, `free
heap=248316` parfaitement stable tout du long (confirmation supplémentaire
qu'il n'y a aucune fuite mémoire). Les annonces périodiques sont donc
**écartées** comme cause.

**Test décisif — Dispatcher (`mesh::Mesh::loop()`) désactivé** : correction
importante du plan initial — `bridge.loop()` (notre MQTT) est appelé
**depuis l'intérieur** de `MyMesh::loop()`, donc on ne peut pas simplement
sauter `the_mesh.loop()` dans `main.cpp` (ça couperait aussi notre propre
MQTT, rendant le test inutile). Correctif appliqué : nouveau flag
`DEBUG_SKIP_DISPATCHER` ajouté **dans `MyMesh.cpp`**, entourant
spécifiquement l'appel à `mesh::Mesh::loop()` (la classe de base/Dispatcher),
tout en laissant `bridge.loop()` totalement intact et actif juste après.

**Résultat, très surprenant** : **le bug persiste à l'identique**
(10328 → 15325 → 20317 ms, deltas ~4997-4992ms) même avec le Dispatcher
complètement désactivé. **Le Dispatcher/traitement des paquets radio n'est
PAS la cause** — hypothèse pourtant la plus probable, écartée.

**Test suivant — Capteurs environnementaux (`sensors.loop()`) désactivés** :
même résultat négatif, cycle identique (10276 → 15249 → 20254 ms).
**Écarté**.

**Test suivant — RTC (`rtc_clock.tick()`) désactivé** : même résultat
négatif, cycle identique sur 5 itérations (10440 → 30442 ms, toujours
~5000ms pile). **Écarté**.

**Analyse du code complet de `MyMesh::loop()`** : avec le Dispatcher
désactivé, il a été confirmé que le reste de la fonction (bloc ACL, timers
radio temporaires, sauvegarde des contacts) ne s'exécute quasiment jamais
dans nos conditions de test (aucun client connecté, aucune annonce active)
— donc même désactivé, `MyMesh::loop()` était déjà un quasi no-op à part
`bridge.loop()`, quasiment identique au sketch isolé stable. Et pourtant le
bug était toujours présent à ce moment-là.

**Test suivant — Lecture des commandes Serial désactivée** : même résultat
négatif, cycle identique sur 6 itérations consécutives (10272 → 35272 ms,
toujours ~5000-5006ms). **Écarté**.

**Conclusion mise à jour — résultat remarquable** : absolument **tout ce
qui s'exécute périodiquement** dans le firmware complet a été testé et
écarté un par un : Dispatcher, capteurs, RTC, écran, serveur web, annonces,
lecture Serial. Le problème ne vient donc **pas** d'une interaction entre
deux morceaux de code actifs en boucle — il doit s'agir d'une **différence
structurelle**, présente dès le démarrage, entre le sketch isolé (stable)
et le vrai firmware (buggy).

**Suspect principal restant, jamais testé isolément** : dans le vrai
firmware, `MqttBridge bridge;` est un **objet membre de la classe
`MyMesh`**, construit avec des dépendances (`&_prefs, _mgr, &rtc`), et son
`begin()` charge ses réglages depuis la **NVS** (`Preferences`/flash) via
`loadSettings()`. Dans tous nos sketches isolés jusqu'ici, on a toujours
utilisé un `PubSubClient`/`WiFiClient` **autonome, recopié à la main** —
jamais la vraie classe `MqttBridge` elle-même, et jamais en tant que membre
d'un objet plus large comme `MyMesh` (qui contient de gros tableaux
internes — ACL, posts — pouvant affecter l'allocation mémoire/pile
disponible pour le reste du programme).

**Test décisif — NVS/Preferences + String (comme la vraie classe `MqttBridge`)** :
plutôt que de recréer la classe complète (dépendances `NodePrefs`/
`PacketManager`/`RTCClock` trop risquées à instancier isolément), le sketch
isolé de base a été enrichi pour charger `server`/`port`/`topic` depuis la
NVS (`Preferences`, même namespace `"mqttcfg"`) dans des `String`
(allocation heap), exactement comme `MqttBridge::loadSettings()` le fait
réellement — au lieu des littéraux `const char*` utilisés jusqu'ici.

**Résultat** : connexion stable pendant plusieurs minutes, aucune ligne
`DROPPED` locale. Confirmé côté broker (`docker logs corescope`) : les
seules reconnexions de `heltec_v4_isolated_test` sont **espacées de 27s
puis 188s** — des aléas normaux, rien à voir avec le cycle implacable de
~5s observé partout ailleurs. **NVS/Preferences + String sont donc
écartés** comme cause.

**Conclusion finale de cette session** : après avoir éliminé
individuellement — avec des tests reproductibles et des preuves
solides — buffer, veille WiFi, timeout socket, fuite de socket, trafic
mesh, fuite mémoire, serveur web, écran OLED, minuteurs d'annonces,
Dispatcher, capteurs, RTC, lecture Serial, **et maintenant NVS/Preferences
+ String**, il ne reste plus qu'**une seule explication plausible** : le
bug est lié au fait que `MqttBridge` est un **objet membre de la classe
`MyMesh`** elle-même — pas à un comportement du bridge en tant que tel.
Pistes possibles à cette structure : taille de l'objet `MyMesh` (contient
de gros tableaux internes — ACL, posts) affectant l'allocation
mémoire/pile disponible pour le reste du programme, ou un effet lié à
l'**ordre d'initialisation** du constructeur (le bridge est construit
avec des pointeurns vers `_prefs`/`_mgr`/`rtc` qui font partie du même
objet `MyMesh`, potentiellement pas encore pleinement initialisés à ce
stade précis de la construction).

**Prochaine étape logique, la plus décisive qui reste** : instancier la
vraie classe `MqttBridge` (pas une copie) comme **membre d'une classe
"conteneur" minimale et artificielle** — pas `MyMesh` complète, mais une
classe simple avec juste assez de champs pour satisfaire le constructeur
(`NodePrefs`, `PacketManager`, `RTCClock` minimaux) — afin d'isoler
précisément si c'est "être membre d'une classe" en général qui pose
problème, ou quelque chose de spécifique à `MyMesh`.

## Ce qui a été écarté (testé et confirmé sans effet)
1. **Taille du buffer PubSubClient** — `_mqtt.setBufferSize(1024)` ajouté
   (au lieu du défaut 256 octets). Aucun changement.
2. **Veille WiFi (modem-sleep)** — `WiFi.setSleep(false)` ajouté dans
   `connectWifi()`. Aucun changement.
3. **Timeout du socket WiFiClient** — `_wifiClient.setTimeout(30)` (au lieu
   du défaut 5s, qui coïncidait suspicieusement avec le cycle observé).
   Aucun changement.
4. **Fuite de socket non fermé** — `_wifiClient.stop()` ajouté avant chaque
   tentative de reconnexion. Aucun changement.
5. **Trafic mesh / repeat** — testé avec `set repeat off` : le cycle de
   reconnexion persiste à l'identique (~5000-5060ms), preuve que le bug est
   totalement indépendant du traitement des paquets LoRa/repeat.
6. **Fuite mémoire (heap)** — testé avec `ESP.getFreeHeap()` loggé à chaque
   cycle sur 4+ tentatives consécutives : `free heap=248124` **identique à
   l'octet près** à chaque fois, aucune baisse. Pas de fuite mémoire.

## Ce qui a été confirmé (isolation du problème)
- ✅ **Le réseau n'est pas en cause** : test comparatif avec MQTT Explorer
  (PC), connecté au même réseau WiFi (`on6dp`) que le Heltec, vers le même
  broker — reste stable sans aucune coupure pendant plusieurs minutes.
- ✅ **Le broker Mosquitto fonctionne normalement** : `mosquitto.conf` propre
  (`listener 1883`, `allow_anonymous true`, `persistence true`), aucune
  anomalie détectée après avoir activé les logs `notice`/`information`.
- ✅ **Le problème est bien reproductible et spécifique au firmware du
  Heltec V4** (ESP32-S3 + PubSubClient + WiFiClient).

## Pistes non encore testées, à explorer en priorité la prochaine fois
1. **Test isolé, en dehors de MeshCore** : écrire un tout petit sketch
   Arduino séparé (WiFi + PubSubClient uniquement, sans radio LoRa, sans
   Room Server, sans écran) sur le même Heltec V4, se connectant au même
   broker. Si le bug persiste en isolation totale → confirme un problème
   PubSubClient/WiFiClient/core ESP32-Arduino, indépendant de MeshCore. Si
   le bug disparaît → confirme une interaction avec le reste du firmware
   (SPI radio, interruptions, boucle principale trop chargée).
2. **Instrumenter `_mqtt.connected()` à haute fréquence** — logger son
   retour à chaque itération de `MyMesh::loop()` (pas seulement au moment du
   throttle) pendant les 2 premières secondes suivant un `connect()` réussi,
   pour voir EXACTEMENT à quel instant (probablement en dessous de la
   seconde) l'état bascule à faux.
3. **Vérifier une éventuelle interaction SPI radio ↔ WiFi** — le module LoRa
   (SPI) et le WiFi partagent des ressources sur l'ESP32-S3 ; une routine
   d'écoute radio (CAD, RX continue) pourrait perturber le driver WiFi/TCP
   de façon subtile, même avec `repeat off` (la radio continue d'écouter).
4. **`MQTT_SOCKET_TIMEOUT` interne à PubSubClient** (distinct du timeout
   WiFiClient déjà testé) — valeur par défaut 15s dans la librairie.
5. **Alimentation/tension instable** (setup solaire) — un brownout bref
   pourrait perturber le WiFi sans déclencher `WiFi.onEvent()`. À vérifier
   avec `get pwrmgt.bootreason`.

## État du code au moment de la pause
Fichier concerné : `src/helpers/bridges/MqttBridge.cpp`
- `begin()` : buffer 1024, timeout WiFiClient 30s
- `ensureConnected()` : `_wifiClient.stop()` avant chaque connect, logs
  détaillés (state avant/après, RSSI, free heap)
- `loop()` : détection immédiate des transitions connecté→déconnecté avec
  timestamp précis (`millis()`)
- `publish()` : log de la taille de chaque message JSON envoyé

Toute cette instrumentation de debug (les `Serial.printf`) peut être
conservée telle quelle pour la reprise — elle sera utile pour la suite du
diagnostic.

## Référence Heltec V4 (pour rappel)
- Environnement PlatformIO : `heltec_v4_room_server_wifi_mqtt`
- Port série actuel : COM6 (a changé plusieurs fois dans la journée après
  chaque reset — toujours revérifier dans le Gestionnaire de périphériques)
- MQTT : `mqtt.no-ip.org:1883`, topic `meshcore/heltec_v4/rx`,
  client ID `heltec_v4_observer`
- Page web : `http://192.168.50.194/` (IP dynamique, a aussi changé
  plusieurs fois)
