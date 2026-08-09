# Avancement

Une livraison = un commit autonome et flashable. On ne passe pas à la suivante tant que le
test de la précédente ne passe pas.

- [x] **L0** — Squelette PlatformIO + doc + CI
      → `pio test -e native` passe, `pio run` compile
- [ ] **L1** — Hello World ePaper
      → texte affiché, un seul refresh
- [ ] **L2** — Wi-Fi + NTP
      → IP et heure correctes en série, reconnexion après coupure
- [ ] **L3** — Fixtures Sonos + parseur DIDL-Lite
      → unitaire : métadonnées extraites de 3 fixtures (Spotify, radio, file vide)
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
- [ ] **L13** — Écran de veille + deep sleep
- [ ] **L14** — Finition doc : photos, captures HA, schéma

## Revue

### L0
`platformio.ini` a demandé deux corrections avant de compiler, toutes deux documentées dans
`docs/build.md` :
1. la plateforme `espressif32` du registre PlatformIO échoue (`FRAMEWORK_DIR` nul) — il faut
   le fork **pioarduino**, épinglé par URL ;
2. `SPI.h`/`Wire.h` n'étaient pas résolus pour les bibliothèques tierces — ajout de
   `lib_ldf_mode = deep+` et des entrées `SPI`/`Wire` dans `lib_deps`.
