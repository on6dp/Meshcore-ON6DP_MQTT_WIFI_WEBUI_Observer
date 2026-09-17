# Référence des commandes CLI — Firmware ON6DP pour Heltec V4

Toutes ces commandes fonctionnent de façon identique via :
- la console série (USB, 115200 bauds)
- l'onglet **CLI** de la page web du Heltec

Syntaxe générale : `get <nom>` pour lire une valeur, `set <nom> <valeur>` pour la modifier.

---

## Identité du nœud

| Commande | Exemple | Description |
|---|---|---|
| `name` | `get name` / `set name "Mon Node"` | Nom affiché du nœud |
| `lat` | `get lat` / `set lat 50.541508` | Latitude (décimal) |
| `lon` | `get lon` / `set lon 5.523446` | Longitude (décimal) |
| `owner.info` | `get owner.info` / `set owner.info Texte\|ligne2` | Infos propriétaire (`\|` = retour à la ligne) |
| `public.key` | `get public.key` | Clé publique du nœud (lecture seule) |
| `role` | `get role` | Rôle du firmware (lecture seule) |
| `guest.password` | `get guest.password` / `set guest.password ...` | Mot de passe invité |

## Radio LoRa

| Commande | Exemple | Description |
|---|---|---|
| `radio` | `get radio` / `set radio ...` | Fréquence, bande passante, SF, CR combinés |
| `freq` | `get freq` / `set freq 868.0` | Fréquence seule (MHz) |
| `tx` | `get tx` / `set tx 22` | Puissance d'émission (dBm) |
| `cad` | `get cad` / `set cad on` \| `off` | Listen-before-talk |
| `radio.rxgain` | `get radio.rxgain` / `set radio.rxgain ...` | Gain RX boosté |
| `radio.fem.rxgain` | `get radio.fem.rxgain` / `set ...` | Gain front-end RX |
| `radio.fem.txgain` | `get radio.fem.txgain` / `set ...` | Gain front-end TX |
| `extra.sf` | `get extra.sf` / `set extra.sf ...` | Spreading factor additionnel |
| `int.thresh` | `get int.thresh` / `set int.thresh ...` | Seuil d'interférence |
| `agc.reset.interval` | `get agc.reset.interval` / `set ...` | Intervalle reset AGC |

## Comportement mesh / forwarding

| Commande | Exemple | Description |
|---|---|---|
| `repeat` | `get repeat` / `set repeat on` \| `off` | Relayer le trafic des autres |
| `flood.max` | `get flood.max` / `set flood.max ...` | Hops max en flood |
| `flood.max.advert` | `get flood.max.advert` / `set ...` | Hops max pour les annonces |
| `flood.max.unscoped` | `get flood.max.unscoped` / `set ...` | Hops max sans scope région |
| `loop.detect` | `get loop.detect` / `set loop.detect ...` | Détection de boucles (off/minimal/moderate/strict) |
| `path.hash.mode` | `get path.hash.mode` / `set ...` | Mode de hash de chemin |
| `advert.interval` | `get advert.interval` / `set advert.interval 0` | Intervalle d'annonce locale |
| `flood.advert.interval` | `get flood.advert.interval` / `set ... 0` | Intervalle d'annonce floodée |
| `dutycycle` | `get dutycycle` / `set dutycycle ...` | Pourcentage de duty cycle |
| `af` | `get af` / `set af ...` | Facteur airtime |
| `rxdelay` | `get rxdelay` / `set rxdelay ...` | Délai RX |
| `txdelay` | `get txdelay` / `set txdelay ...` | Délai TX |
| `direct.txdelay` | `get direct.txdelay` / `set ...` | Délai TX direct |
| `multi.acks` | `get multi.acks` / `set multi.acks ...` | Nombre d'ACKs additionnels |
| `allow.read.only` | `get allow.read.only` / `set ...` | Autoriser accès lecture seule distant |

## MQTT (observateur) — modifiable à chaud, sans reflasher

| Commande | Exemple | Description |
|---|---|---|
| `mqtt.server` | `get mqtt.server` / `set mqtt.server mqtt.no-ip.org` | Adresse du broker |
| `mqtt.port` | `get mqtt.port` / `set mqtt.port 1883` | Port du broker |
| `mqtt.topic` | `get mqtt.topic` / `set mqtt.topic meshcore/xyz/rx` | Topic de publication |

## WiFi — modifiable à chaud, sans reflasher *(ajout ON6DP)*

| Commande | Exemple | Description |
|---|---|---|
| `wifi.ssid` | `get wifi.ssid` / `set wifi.ssid MonReseau` | Nom du réseau WiFi |
| `wifi.pwd` | `set wifi.pwd MotDePasse` | Mot de passe WiFi (`get` renvoie toujours "(hidden)" — jamais affiché en clair) |

*(Astuce web : l'onglet WiFi propose aussi un bouton "Scan networks" pour choisir le réseau dans une liste plutôt que de taper son nom.)*

## Divers

| Commande | Exemple | Description |
|---|---|---|
| `adc.multiplier` | `get adc.multiplier` / `set ...` | Calibration batterie |
| `prv.key` | `set prv.key ...` | Restaurer une identité (écriture seule, série uniquement) |
| `bootloader.ver` | `get bootloader.ver` | Version bootloader (lecture seule) |
| `pwrmgt.support` / `.source` / `.bootreason` / `.bootmv` | `get pwrmgt.support` | Gestion d'énergie (lecture seule sur ce matériel) |
| `time` | `time 1789483421` | Régler l'horloge (secondes Unix — utilise `date +%s` sur un serveur pour l'obtenir) |

## Commandes spéciales

| Commande | Exemple | Description |
|---|---|---|
| `setperm` | `setperm <clé-publique-hex> <permissions>` | Droits ACL d'un client |
| `get acl` | `get acl` | Liste des clients ACL (console série uniquement) |
| `room.post` | `room.post Mon message` | Poster un message sur le BBS |

---

*Firmware ON6DP — MeshCore Room Server pour Heltec V4, 868 MHz*
*73, Paul ON6DP*
