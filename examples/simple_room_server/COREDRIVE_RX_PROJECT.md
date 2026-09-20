# Chantier à venir : intégration de CoreDrive RX

## Contexte
Découverte du projet **CoreDrive RX** (`github.com/efiten/coredrive-rx`) — une
PWA mobile qui se connecte en Bluetooth à un appareil MeshCore (ton Heltec),
capte les nœuds entendus pendant un déplacement (voiture, vélo...) avec leur
SNR/RSSI, les associe à la position GPS du téléphone, et publie tout vers
MQTT pour qu'un ingesteur CoreScope les stocke — générant ainsi une vraie
carte de couverture radio de terrain, pas juste théorique.

## Obstacle technique principal
CoreDrive RX est une app **navigateur** (PWA) — elle ne peut techniquement
pas ouvrir de connexion MQTT en TCP brut (limitation de sécurité des
navigateurs web). Elle nécessite impérativement du **MQTT over WebSocket**
(`wss://...:8084/ws`), alors que le Mosquitto actuel n'expose que du TCP
simple sur le port 1883.

**Bonne nouvelle vérifiée** : le port **8084** est libre sur le VPS (confirmé
via `ss -tlnp | grep 8084` — aucun résultat).

## Étapes restant à faire
1. ✅ **FAIT** — Listener WebSocket Mosquitto ajouté (`listener 8084` +
   `protocol websockets` dans un `mosquitto.conf` monté depuis
   `/root/meshcore-data/mosquitto.conf`, via `docker-compose.yml`). Testé
   et confirmé fonctionnel (`101 Switching Protocols`).
2. ✅ **FAIT, plus simplement que prévu** — pas besoin de gérer le TLS
   directement dans Mosquitto. Un `Caddyfile` personnalisé
   (`/root/meshcore-data/Caddyfile`, monté de la même façon) fait
   transiter `wss://meshcore.no-ip.org/mqtt-ws` (chemin dédié, HTTPS via
   le certificat Let's Encrypt déjà existant pour le domaine) vers
   `localhost:8084` en interne. Testé et confirmé : `101 Switching
   Protocols` en HTTPS (attention au test avec `curl` : forcer
   `--http1.1`, sinon HTTP/2 casse le test avec une fausse erreur 502).
   **Adresse WebSocket finale à utiliser dans la config CoreDrive RX** :
   `wss://meshcore.no-ip.org/mqtt-ws`
3. **Héberger CoreDrive RX** — télécharger la release depuis GitHub, la
   servir sur un sous-domaine ou un chemin dédié (ex: `rx.meshcore.no-ip.org`
   ou similaire), avec un `config.json` pointant vers le bon broker.
4. **Configurer CoreScope** : activer `clientRxObservations.enabled: true`
   dans `config.json` — sinon les paquets `fullRfLog` de CoreDrive RX sont
   silencieusement ignorés par l'ingesteur (décodés puis jetés, sans erreur
   visible).
5. **Compte MQTT dédié, publish-only** — la doc de CoreDrive RX recommande
   un compte MQTT séparé, à droits limités (publication seulement, pas de
   lecture), car ses identifiants sont embarqués dans l'app cliente
   (visible par n'importe qui inspectant le code du navigateur) — donc à
   traiter comme public, pas secret. Rejoint aussi le chantier sécurité
   MQTT (mise en place d'une authentification, actuellement inexistante).

## Pourquoi le garder pour une session dédiée
Plusieurs points intersectent avec le chantier sécurité MQTT déjà en
attente (authentification, TLS) — plus cohérent de traiter les deux
ensemble plutôt que de bricoler une solution WebSocket isolée maintenant.
