# ePaper Spotify

[![CI](https://github.com/leto1210/epaper-sonos-spotify/actions/workflows/ci.yml/badge.svg)](https://github.com/leto1210/epaper-sonos-spotify/actions/workflows/ci.yml)

Affiche le morceau Spotify joué sur vos enceintes Sonos, sur un ePaper couleur 7,3" —
**100 % en local**. Pas de cloud, pas de compte développeur Spotify, pas d'API Sonos en
ligne : le boîtier interroge directement vos enceintes sur le réseau local.

![ePaper Spotify display on wall](docs/images/device.jpg)

## Fonctionnalités

- Pochette d'album téléchargée depuis l'enceinte et tramée pour l'ePaper 6 couleurs
- Titre, artiste, album, zone Sonos et durée du morceau
- Boutons physiques : morceau précédent / suivant / play-pause
- Intégration Home Assistant via MQTT Discovery (batterie, température, humidité, morceau
  en cours, bouton de rafraîchissement, sélection de zone)
- Écran météo quand rien ne joue : conditions du moment, prévisions horaires, et comparaison
  avec la température intérieure mesurée par le boîtier — il prend aussi la place d'un
  morceau en pause depuis plus de cinq minutes
- Fonctionne sur batterie : l'écran n'est redessiné que lorsque le morceau change

## Matériel

| Élément | Détail |
|---|---|
| [Seeed reTerminal E1002](https://www.seeedstudio.com/reTerminal-E1002-p-6533.html) | ESP32-S3, ePaper couleur 7,3" 800×480, batterie 2000 mAh (~95 €) |
| Enceintes Sonos | S2, sur le même réseau que le boîtier |
| Home Assistant + Mosquitto | Optionnel — uniquement pour l'intégration HA |

Rien à souder : tout est intégré au reTerminal.

## Démarrage rapide

```bash
git clone https://github.com/leto1210/epaper-sonos-spotify.git
cd epaper-sonos-spotify
cp config.example.h src/config.h   # puis éditez vos identifiants Wi-Fi / MQTT
pio run -t upload -t monitor
```

`src/config.h` est git-ignoré : vos identifiants ne partent jamais dans le dépôt.

Détails de compilation, génération de `driver.h` et dépannage : [docs/build.md](docs/build.md).

## Documentation

- [docs/development.md](docs/development.md) — **travailler sur le projet sans le matériel**
- [docs/hardware.md](docs/hardware.md) — brochage complet du reTerminal E1002
- [docs/sonos-api.md](docs/sonos-api.md) — l'API UPnP locale de Sonos, avec des `curl`
  reproductibles
- [docs/home-assistant.md](docs/home-assistant.md) — entités MQTT exposées
- [docs/architecture.md](docs/architecture.md) — machine à états et politique de refresh
- [docs/build.md](docs/build.md) — compilation, flash, tests

## Limites connues

- Un rafraîchissement complet de l'ePaper Spectra 6 prend **37 s** — mesuré, au-dessus des
  25–30 s annoncés — et il n'existe pas de rafraîchissement partiel en couleur. C'est
  pourquoi l'écran affiche la durée du morceau et non une barre de progression : celle-ci
  serait figée à l'instant du rendu.
- Spotify Connect ne fournit les métadonnées **que pendant la lecture effective** : une
  enceinte mise en pause ne dit plus ce qu'elle joue. Le boîtier conserve donc la dernière
  fiche connue, jusqu'à la bascule sur la météo.
- Sonos S2 uniquement (l'API UPnP locale de S1 diffère et n'est pas testée).
- La découverte SSDP repose sur du multicast : elle ne fonctionne que si le boîtier et les
  enceintes partagent le même sous-réseau. Si le boîtier est sur un VLAN « objets
  connectés », renseignez `SONOS_SEED_IP` dans `src/config.h` et autorisez le flux **TCP
  1400** vers les enceintes. Voir [docs/sonos-api.md](docs/sonos-api.md).

## Crédits

[Seeed Studio](https://wiki.seeedstudio.com/reterminal_e10xx_main_page/) pour le matériel et
`Seeed_GFX`, [JPEGDEC](https://github.com/bitbank2/JPEGDEC) de bitbank2 pour le décodage des
pochettes.

Licence [MIT](LICENSE).
