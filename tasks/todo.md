# Avancement

Une livraison = un commit autonome et flashable. On ne passe pas à la suivante tant que le
test de la précédente ne passe pas.

- [x] **L0** — Squelette PlatformIO + doc + CI
      → `pio test -e native` passe, `pio run` compile
- [x] **L1** — Hello World ePaper
      → flashé sur cible : 800x480 initialisé, 1 seul rafraîchissement, 37,3 s
- [x] **L2** — Wi-Fi + NTP
      → IP obtenue en 1,3 s, heure NTP correcte, test TCP de joignabilité intégré
- [x] **L3** — Fixtures Sonos + parseur DIDL-Lite
      → 8 tests unitaires au vert sur 5 captures réelles anonymisées
- [ ] **L4** — SSDP + choix du coordinateur
      → zones listées en série, coordinateur identifié
- [ ] **L5** — `GetPositionInfo` en direct
      → titre/artiste corrects en série en moins de 20 s
- [ ] **L6** — Layout texte sur ePaper
- [ ] **L7** — Anti-redraw + compteur de refresh
      → 10 min sur le même morceau ⇒ compteur = 1
- [ ] **L8** — Pochette : HTTP + JPEGDEC + tramage
- [ ] **L9** — Capteurs SHT4x + batterie
- [ ] **L10** — Boutons + buzzer (Next / Previous / Play-Pause)
      → 4 appuis rapides ⇒ un seul refresh
- [ ] **L11** — MQTT + Home Assistant Discovery
- [ ] **L12** — Bouton refresh et select de zone depuis HA
- [ ] **L13** — Météo : abonnement MQTT + parsing + automatisation HA
      → couper la musique ⇒ conditions actuelles, prévisions horaires, intérieur/extérieur
- [ ] **L14** — Écran de veille + deep sleep
- [ ] **L15** — Finition doc : photos, captures HA, schéma

## Revue

### L0
`platformio.ini` a demandé deux corrections avant de compiler, toutes deux documentées dans
`docs/build.md` :
1. la plateforme `espressif32` du registre PlatformIO échoue (`FRAMEWORK_DIR` nul) — il faut
   le fork **pioarduino**, épinglé par URL ;
2. `SPI.h`/`Wire.h` n'étaient pas résolus pour les bibliothèques tierces — ajout de
   `lib_ldf_mode = deep+` et des entrées `SPI`/`Wire` dans `lib_deps`.

### L3
Deux découvertes sur du matériel réel, toutes deux documentées dans `docs/sonos-api.md` :

1. **Spotify Connect** (`x-sonos-vli:`) ne fournit les métadonnées que pendant la lecture
   effective ; en pause elles valent `NOT_IMPLEMENTED`. Ni `GetMediaInfo` ni les événements
   GENA n'y changent quoi que ce soit. D'où la distinction, dans `TrackInfo`, entre
   « l'enceinte joue » et « on connaît le morceau » : l'écran conservera la dernière fiche
   connue au lieu de se vider à la pause.
2. La **pochette** est une URL HTTPS absolue vers le CDN Spotify, pas un chemin relatif.
   Inexploitable depuis l'ESP32 ; on passe par `/getaa` sur l'enceinte, qui sert la même
   image en HTTP simple sur le LAN (640×640, ~200 Ko).

Le piège du coordinateur s'est présenté dès la première capture : l'enceinte interrogée était
esclave et renvoyait `NOT_IMPLEMENTED`. Cette capture est conservée comme fixture.

Un bug réel attrapé par les tests : la recherche de balise trouvait `upnp:albumArtURI` quand
on demandait `upnp:album`.

### L1 (validée sur cible)
Le reTerminal E1002 expose son port série par un **pont CH340 sur UART0**, pas par l'USB
natif de l'ESP32-S3 : avec `ARDUINO_USB_CDC_ON_BOOT=1`, `Serial` partait vers le CDC natif et
la console restait muette alors que le ROM, lui, parlait bien. Corrigé en passant le drapeau
à 0.

Rafraîchissement mesuré : **37,3 s** pour un écran complet, au-dessus des 25-30 s annoncés
par Seeed. La politique anti-redraw n'en est que plus justifiée.

### L2
Le LDF ne propage pas les bibliothèques intégrées au core vers les bibliothèques tierces :
`SPI.h` manquait à Adafruit BusIO, `Network.h` à `WiFi` (scindée depuis le core Arduino 3.x).
Ni `lib_ldf_mode = deep+` ni l'ajout dans `lib_deps` n'y suffisent — les chemins sont
désormais donnés explicitement via `$PROJECT_PACKAGES_DIR`.

**Contrainte réseau découverte à l'exécution** : le boîtier obtient une adresse sur le VLAN
« objets connectés » (192.168.110.x) alors que les enceintes sont sur un autre sous-réseau.
Le test TCP ajouté au démarrage l'a montré immédiatement plutôt qu'à travers un délai
d'attente inexpliqué au premier sondage. Home Assistant, lui, est du bon côté : l'intégration
MQTT et la météo ne demandent aucune ouverture. Il reste à autoriser **TCP 1400** vers les
enceintes ; la découverte SSDP, en multicast, ne franchira de toute façon jamais le routeur,
d'où le passage de `SONOS_SEED_IP` du statut de repli à celui de mode nominal.
