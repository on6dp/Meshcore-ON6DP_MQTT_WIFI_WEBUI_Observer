Firmware ON6DP — MeshCore Room Server personnalisé



Ce dépôt est un fork personnalisé de MeshCore, maintenu par ON6DP (radioamateur belge, région Liège). Le projet MeshCore original reste inchangé — ce fichier documente uniquement ce qui a été ajouté par rapport à l'original.



🇧🇪 Ce que ce firmware ajoute au Room Server MeshCore de base



Flashé sur un Heltec V4 (ESP32-S3, 868 MHz)

Ce firmware transforme un Room Server MeshCore standard en observateur MQTT connecté et administrable à distance, avec :



* WiFi entièrement modifiable à chaud (NVS) — plus besoin de reflasher pour changer de réseau, y compris un scan des réseaux disponibles directement depuis la page web
* Bridge MQTT intégré — publie tout le trafic mesh observé vers un broker MQTT (serveur/port/topic modifiables à chaud), avec :
* Encodage hexadécimal correct des paquets (compatible CoreScope)
* Message de statut périodique (origin, firmware, client\_version), pour apparaître nommé dans l'onglet Observers de CoreScope
* Publication automatique et périodique d'un message promotionnel sur le réseau mesh (BBS Room Server), pour inviter d'autres opérateurs à rejoindre le même broker
* Page web de configuration à 5 onglets (Radio / MQTT / WiFi / Stats / CLI), inspirée de gessaman's observer :
* Console CLI asynchrone (API JSON, historique persistant, pas de rechargement de page)
* Autocomplétion sur \~40 commandes CLI
* Rafraîchissement automatique de toutes les valeurs affichées
* Écran OLED enrichi — affiche en continu l'adresse IP et la version du firmware, en plus des infos radio standard



📚 Documentation

examples/simple\_room\_server/TUTO\_FLASH\_HELTEC\_ON6DP.md — tutoriel complet pour flasher ce firmware, pensé pour des débutants complets (Python, Git, PlatformIO Core, sans VS Code requis)

examples/simple\_room\_server/COMMANDES\_CLI\_ON6DP.md — référence complète des commandes CLI disponibles



🛠️ Environnement de compilation



L'environnement principal est heltec\_v4\_room\_server\_wifi\_mqtt, défini dans variants/heltec\_v4/platformio.ini.



bash



git clone https://github.com/on6dp/Meshcore-ON6DP\_MQTT\_WIFI\_WEBUI\_Observer.git

cd Meshcore-ON6DP\_MQTT\_WIFI\_WEBUI\_Observer

pio run -e heltec\_v4\_room\_server\_wifi\_mqtt -t upload --upload-port COMx



⚠️ Sur certains PC/câbles, le flash peut échouer avec No serial data received. Ajouter upload\_flags = --no-stub et upload\_speed = 115200 à l'environnement résout ce problème (déjà en place dans ce dépôt).



🙏 Crédits



Ce firmware s'appuie entièrement sur le travail du projet MeshCore et de sa communauté. Les ajouts documentés ici sont des personnalisations locales, partagées dans l'esprit open-source du projet d'origine.



73, Paul — ON6DP

