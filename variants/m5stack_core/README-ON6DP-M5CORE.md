# M5Stack Core Basic + Module13.2 LoRa-1262 + Module13.2 LAN — Repeater MeshCore avec passerelle MQTT Ethernet

Variante ON6DP pour le **M5Stack Core Basic / Gray / Fire** (ESP32, modèle à 3 boutons).

- **Repeater MeshCore** sur le Module13.2 LoRa-1262 (SX1262).
- **Passerelle MQTT « observer »** par le câble réseau (W5500), même format de messages que le firmware Heltec V4 (compatible CoreScope).
- **Écran couleur 320x240** avec 4 pages et **3 boutons**.
- **Heure par NTP** dès que le réseau est disponible.

> ⚠️ Ce code n'a pas encore été compilé ni testé sur le matériel. La première compilation peut demander de petites corrections.

## 1. Réglage des modules

Tout partage le bus SPI du Core (SCK=G18, MISO=G19, MOSI=G23). Chaque module doit avoir **ses propres lignes** :

| Signal | Broche M5-Bus | GPIO du Basic | Réglage |
|---|---|---|---|
| LoRa NSS | 20 | G5 | DIP SW1 du module LoRa |
| LoRa BUSY | 4 | G36 | DIP SW1 du module LoRa |
| LoRa IRQ | 2 | G35 | DIP SW3 du module LoRa |
| Expandeur M5IOE1 | 17 / 18 | G21 / G22 | DIP SW2 : A=0, B=0 (adresse 0x74) |
| LAN CS | 23 | G15 | cavalier du module LAN |
| LAN RST | 22 | G13 | cavalier du module LAN |
| LAN INT | 26 | G34 | cavalier du module LAN |

Si la sérigraphie de tes modules impose une autre combinaison, adapte les lignes `-D P_LORA_*` et `-D P_ETH_*` dans `variants/m5stack_core/platformio.ini`.

Choix possibles sur le M5-Bus :

- **LoRa NSS** : broche 20 (G5), 21 (G12), 23 (G15), 24 (G0)
- **LoRa BUSY** : broche 4 (G36), 8 (G25), 19 (G2), 22 (G13)
- **LoRa IRQ** : broche 2 (G35), 10 (G26), 26 (G34)
- **LAN CS** : broche 19 (G2), 20 (G5), 23 (G15)
- **LAN RST** : broche 22 (G13), 24 (G0)
- **LAN INT** : broche 2 (G35), 26 (G34)

À éviter sur le Basic : **G25** (haut-parleur), **G12** et **G0** (broches lues au démarrage de l'ESP32). Les deux modules ne doivent jamais utiliser la même broche, en particulier **IRQ LoRa et INT LAN** qui sont toutes deux sur la broche 2 dans les exemples M5Stack.

**Alimentation** : utilise le jack 9–24 V du module LAN. **Visse l'antenne avant de mettre sous tension.**

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
| `M5IOE1 expander not answering at 0x74` | DIP SW2 du module LoRa pas sur A=0, B=0 |
| `ERROR: radio init failed: -2` | NSS / BUSY ne correspondent pas aux DIP switches |
| Radio OK mais rien reçu ni émis | IRQ (SW3) ne correspond pas à `P_LORA_DIO_1` |
| `ETH: no W5500 found` | cavalier CS du module LAN ≠ `P_ETH_CS` |
| Couleurs de l'écran inversées | ajouter `-D M5_LCD_INVERT=1` (ou `=0`) |
