# M5Stack Core Basic + Module13.2 LoRa-1262 + Module13.2 LAN — Repeater MeshCore avec passerelle MQTT Ethernet

Variante ON6DP pour le **M5Stack Core Basic / Gray / Fire** (ESP32, modèle à 3 boutons).

- **Repeater MeshCore** sur le Module13.2 LoRa-1262 (SX1262).
- **Passerelle MQTT « observer »** par le câble réseau (W5500), même format de messages que le firmware Heltec V4 (compatible CoreScope).
- **WiFi de secours** : sans câble (ou sans module LAN dans la pile), la passerelle passe automatiquement par le WiFi.
- **Écran couleur 320x240** avec 4 pages et **3 boutons**.
- **Heure par NTP** dès que le réseau est disponible.

> ✅ Validé sur un Core Basic (27/11/2026) : écran et boutons, réception et émission radio, Ethernet (DHCP), heure NTP, publication MQTT visible dans CoreScope. Reste à observer : la tenue dans la durée.

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
| **B** sur la page Réseau | rappel à l'écran | **Mode configuration** |
| **C** (droite) | Luminosité : forte → faible → éteint | Redémarrage |

L'écran s'éteint après 5 minutes sans appui (`-D M5UI_AUTO_OFF_SECS=0` pour le garder allumé). Le premier appui le rallume.

## 4. Pages

1. **État** : fréquence, modulation, puissance, uptime, heure UTC, adresse IP, MQTT, version.
2. **Radio** : paquets reçus / émis, erreurs, flood / direct, RSSI / SNR, bruit, temps d'émission.
3. **Réseau** : lien Ethernet, WiFi, IP, NTP, broker, état MQTT, messages publiés, topic.
4. **Voisins** : derniers repeaters entendus (identifiant, ancienneté, SNR).

## 5. Commandes CLI ajoutées au repeater

```
get mqtt.server      set mqtt.server <hôte>
get mqtt.port        set mqtt.port <port>
get mqtt.topic       set mqtt.topic <topic>
get mqtt.status

get wifi.ssid        set wifi.ssid <nom du réseau>      (réseau 1, ex. maison)
                     set wifi.pwd <mot de passe>
get wifi2.ssid       set wifi2.ssid <nom du réseau>     (réseau 2, ex. téléphone)
                     set wifi2.pwd <mot de passe>
get wifi.status
```

Les valeurs sont enregistrées (NVS) et appliquées sans redémarrage.

## 6. Réseau

**Câble (prioritaire)**

- **DHCP** par défaut. IP fixe : décommente `ETH_STATIC_IP`, `ETH_GATEWAY`, `ETH_DNS`, `ETH_SUBNET`.
- Pendant une demande DHCP sans réponse, ou si le broker est injoignable, la boucle principale est bloquée quelques secondes : des paquets radio peuvent être manqués pendant ce temps.

**WiFi (secours)**

- Deux réseaux sont mémorisés, par exemple celui de la maison et le point d'accès du téléphone. À configurer une fois, par la console série ou la console d'administration à distance :

```
set wifi.ssid ReseauMaison
set wifi.pwd MotDePasseMaison
set wifi2.ssid PointAccesTelephone
set wifi2.pwd MotDePasseTelephone
```

- Le Core essaie les deux réseaux à tour de rôle (12 secondes chacun) jusqu'à ce que l'un réponde. Le point d'accès du téléphone doit être en **2,4 GHz**.

- Le WiFi démarre quand aucun câble n'est branché depuis 4 secondes, ou quand le module LAN n'est pas dans la pile. Il s'éteint dès que le câble revient.
- `set wifi.ssid` ou `set wifi2.ssid` sans valeur efface le réseau correspondant.
- Les identifiants sont enregistrés dans le Core (NVS), pas dans le dépôt.

**Sur batterie** : la radio à 22 dBm, le WiFi et l'écran consomment beaucoup. Pour un usage mobile, baisse la puissance (`set tx 14` par exemple) et laisse l'écran s'éteindre.

**Mode configuration (WiFi et MQTT depuis un téléphone ou un PC)**

1. Va sur la page **Réseau** de l'écran : le bouton du milieu s'appelle alors **CONFIG**.
2. Appui long sur ce bouton. Le Core ouvre son propre réseau WiFi `ON6DP-M5` et affiche un mot de passe à 8 chiffres, différent à chaque fois.
3. Connecte ton téléphone ou ton PC à ce réseau, puis ouvre `http://192.168.4.1`.
4. Saisis les deux réseaux WiFi et les réglages MQTT, puis **Enregistrer**. Le mode se ferme tout seul.

Le mode se ferme aussi par un appui sur le bouton du milieu (QUITTER), ou après 10 minutes sans activité. La passerelle par câble continue de fonctionner pendant ce temps.

## 7. Dépannage (console série 115200)

| Message | Cause probable |
|---|---|
| `M5IOE1 expander not answering at 0x74` | bloc A/B du module LoRa pas sur A=0, B=0 |
| `ERROR: radio init failed: -2`, écran bloqué sur « Please wait » | DIP switches du LoRa mal réglés, ou conflit de broches avec le module LAN |
| Radio OK mais rien reçu ni émis | switch IRQ ne correspond pas à `P_LORA_DIO_1` |
| `ETH: no W5500 found` | cavalier CS du module LAN ≠ `P_ETH_CS` |
| Couleurs de l'écran inversées | ajouter `-D M5_LCD_INVERT=1` (ou `=0`) |
