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

## ⚠️ Découverte du jour : bug de reconnexion similaire, mais sur un tout autre canal
En testant CoreDrive RX en direct (PC, Chrome), la connexion WebSocket
(`wss://meshcore.no-ip.org/mqtt-ws`) montre un cycle de coupure très
régulier — environ **5 secondes** de connexion active à chaque fois,
confirmé sur plus de 15 cycles consécutifs dans le journal de l'app :
```
18:57:47 connected → 18:57:52 offline (5s)
18:57:56 connected → 18:58:01 offline (5s)
```

**Rappel important — probablement une coïncidence, pas le même bug** que
celui du Heltec (`MQTT_RECONNECT_BUG.md`) : les deux chemins techniques
sont complètement différents (Heltec = TCP direct port 1883 vers Mosquitto ;
CoreDrive RX = WebSocket port 8084, **via Caddy** en intermédiaire). Un
timing similaire pourrait très bien être deux causes distinctes.

**Vérifié et écarté** : rien dans `mosquitto.conf` n'explique un timeout à
5s (pas de directive de ce genre) ; le `keepalive 60s` négocié par
CoreDrive RX devrait tolérer ~90s d'inactivité avant coupure MQTT normale
— donc pas un simple problème de keepalive MQTT standard.

**Pistes à explorer lors d'une prochaine session** :
- Config de timeout côté **Caddy** pour les connexions WebSocket
  proxifiées (`reverse_proxy` a potentiellement un idle timeout par
  défaut à vérifier/ajuster)
- Logs Mosquitto en mode debug (`log_type debug`) pour voir la vraie
  raison de fermeture côté broker
- Comparer avec une connexion WebSocket testée directement (sans Caddy,
  directement sur le port 8084) pour isoler si Caddy est bien en cause

## 🎉 Intégration technique réussie, affichage encore à découvrir
Toute la chaîne technique fonctionne, de bout en bout, **confirmé par requête
SQL directe** :
- CoreDrive RX (hébergé sur `https://drive.meshcore.no-ip.org`, sous-domaine
  dédié nécessaire — voir note ci-dessous) se connecte en Bluetooth au
  T1000-E (rôle Companion), publie via `wss://meshcore.no-ip.org/mqtt-ws`
- CoreScope stocke bien les données : table `client_rf_samples` (confirmé
  `SELECT COUNT(*)` > 0, avec position GPS, batterie, bruit de fond)
- **Réglage nécessaire dans `config.json`** : il existe **3 tables distinctes**
  à activer séparément (toutes désactivées par défaut) :
  ```json
  "clientRxCoverage": { "enabled": true },
  "clientRxObservations": { "enabled": true },
  "clientRfSamples": { "enabled": true },
  ```
  Les données de CoreDrive RX (`type":"RF_SAMPLE"`) alimentent
  spécifiquement `client_rf_samples`.

**Ce qui reste flou** : aucun onglet du dashboard (`RF Health`, `Map`,
`Analytics`...) n'affiche visiblement ces données pour l'instant — la
fonctionnalité semble stockée mais sans interface graphique dédiée encore
développée côté CoreScope (probablement une fonctionnalité très récente,
"opt-in désactivée par défaut"). À revérifier lors d'une future mise à jour
de CoreScope, ou à interroger directement en SQL en attendant :
```bash
sqlite3 /root/meshcore-data/meshcore.db "SELECT * FROM client_rf_samples ORDER BY rowid DESC LIMIT 10;"
```

## ⚠️ Piège important découvert : sous-chemin vs sous-domaine
CoreDrive RX est compilé avec des chemins **absolus** (`/assets/...`), pas
relatifs — impossible de l'héberger sous un sous-chemin (`/coredrive/`) d'un
domaine existant, ça casse le chargement JS silencieusement (page vide, rien
de cliquable). **Il faut un sous-domaine dédié.**

Solution trouvée : wildcard DNS déjà configuré sur le compte No-IP payant de
l'utilisateur (`*.meshcore.no-ip.org` → IP du VPS), permettant de créer
`drive.meshcore.no-ip.org` sans configuration DNS supplémentaire. Caddy gère
alors ce second domaine séparément dans le même `Caddyfile`, avec son propre
certificat Let's Encrypt automatique.

## Étapes du plan — toutes complétées
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
