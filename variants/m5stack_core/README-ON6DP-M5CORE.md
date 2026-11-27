# M5Stack Core Basic + Module13.2 LoRa-1262 + Module13.2 LAN — Repeater MeshCore avec passerelle MQTT Ethernet

Variante ON6DP pour le **M5Stack Core Basic / Gray / Fire** (ESP32, modèle à 3 boutons).

- **Repeater MeshCore** sur le Module13.2 LoRa-1262 (SX1262).
- **Passerelle MQTT « observer »** par le câble réseau (W5500), même format de messages que le firmware Heltec V4 (compatible CoreScope).
- **Écran couleur 320x240** avec 4 pages et **3 boutons**.
- **Heure par NTP** dès que le réseau est disponible.

> ⚠️ Firmware en cours de mise au point. Validé sur un Core Basic : compilation, écran, radio initialisée, Ethernet (DHCP), heure NTP et connexion MQTT. Encore à vérifier : boutons, trafic radio réel et tenue dans la durée.

## 1. Réglage des modules

Tout partage le bus SPI du Core (SCK=G18, MISO=G19, MOSI=G23). Chaque module doit avoir **ses propres lignes**. Le firmware est réglé pour que le module LAN reste **en configuration d'usine** : seul le module LoRa est à régler.

**Module LoRa** — mettre sur ON les DIP switches **4, 6 et 10**, tous les autres sur OFF :

| Signal | Switch | GPIO du Core |
|---|---|---|
| NSS | 4 | G15 |
| BUSY | 6 | G36 |
| IRQ | 10 | G34 |
| Adresse de l'expandeur M5IOE1 | bloc A/B : A=0, B=0 | 0x74 (I2C sur G21 / G22) |

**Module LAN** — cavaliers en position d'usine (premier choix de la colonne « Core » imprimée au dos) :

| Signal | GPIO du Core |
|---|---|
| CS | G5 |
| RST | G0 |
| INT | G35 |

Avec tous les DIP switches du module LoRa sur OFF (état à la livraison), la radio n'est reliée à rien et le démarrage reste bloqué sur « Please wait ».

Pour une autre combinaison, adapte les lignes `-D P_LORA_*` et `-D P_ETH_*` dans `variants/m5stack_core/platformio.ini`. Choix proposés par les modules, colonne « Core » :

| Module | Signal | Choix |
|---|---|---|
| LoRa | NSS | switch 1 = G0, 2 = G5, 3 = G12, 4 = G15 |
| LoRa | BUSY | switch 5 = G2, 6 = G36, 7 = G13, 8 = G25 |
| LoRa | IRQ | switch 9 = G35, 10 = G34, 11 = G26 |
| LAN | CS | G5 ou G15 |
| LAN | RST | G0 ou G13 |
| LAN | INT | G35 ou G34 |

Un seul switch sur ON par signal. Les deux modules ne doivent jamais utiliser la même broche. À éviter pour la radio : **G25** (haut-parleur) et **G12** (broche lue au démarrage de l'ESP32).

**Alimentation** : l'USB suffit pour les essais ; pour l'installation définitive, utilise le jack 9–24 V du module LAN. **Visse l'antenne avant de mettre sous tension.**

## 2. Compilation et flash

```bash
pio run -e M5Stack_Core_repeater_lan_mqtt -t upload --upload-port COMx
```

Avant de compiler, modifie dans `variants/m5stack_core/platformio.ini` : `ADVERT_NAME`, `ADMIN_PASSWORD`, `ADVERT_LAT` / `ADVERT_LON`, `MQTT_TOPIC`, `MQTT_CLIENT_ID`.

L'environnement `M5Stack_Core_repeater` donne le même repeater sans le module LAN.

## 3. Boutons

| Bouton | Appui court | Appui long |
|---|---|---|
| **A** (gauche) | Page suivante | Page précédente |
| **B** (milieu) | Advert local (zero-hop) | Advert flood |
| **C** (droite) | Luminosité : forte → faible → éteint | Redémarrage |

L'écran s'éteint après 5 minutes sans appui (`-D M5UI_AUTO_OFF_SECS=0` pour le garder allumé). Le premier appui le rallume.

## 4. Pages

1. **État** : fréquence, modulation, puissance, uptime, heure UTC, adresse IP, MQTT, version.
2. **Radio** : paquets reçus / émis, erreurs, flood / direct, RSSI / SNR, bruit, temps d'émission.
3. **Réseau** : lien Ethernet, IP, NTP, broker, port, état MQTT, messages publiés, topic.
4. **Voisins** : derniers repeaters entendus (identifiant, ancienneté, SNR).

## 5. Commandes CLI ajoutées au repeater

```
get mqtt.server      set mqtt.server <hôte>
get mqtt.port        set mqtt.port <port>
get mqtt.topic       set mqtt.topic <topic>
get mqtt.status
```

Les valeurs sont enregistrées (NVS) et appliquées sans redémarrage.

## 6. Réseau

- **DHCP** par défaut. IP fixe : décommente `ETH_STATIC_IP`, `ETH_GATEWAY`, `ETH_DNS`, `ETH_SUBNET`.
- Pendant une demande DHCP sans réponse, ou si le broker est injoignable, la boucle principale est bloquée quelques secondes : des paquets radio peuvent être manqués pendant ce temps.
- Pas de page web de configuration dans cette version (elle repose sur le WiFi dans le firmware Heltec).

## 7. Dépannage (console série 115200)

| Message | Cause probable |
|---|---|
| `M5IOE1 expander not answering at 0x74` | bloc A/B du module LoRa pas sur A=0, B=0 |
| `ERROR: radio init failed: -2`, écran bloqué sur « Please wait » | DIP switches du LoRa mal réglés, ou conflit de broches avec le module LAN |
| Radio OK mais rien reçu ni émis | switch IRQ ne correspond pas à `P_LORA_DIO_1` |
| `ETH: no W5500 found` | cavalier CS du module LAN ≠ `P_ETH_CS` |
| Couleurs de l'écran inversées | ajouter `-D M5_LCD_INVERT=1` (ou `=0`) |
