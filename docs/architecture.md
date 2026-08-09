# Architecture

```
[Enceintes Sonos :1400] <--SOAP/HTTP--> [ESP32-S3] --SPI--> [ePaper 800x480]
         ^ découverte SSDP                  |  ^ 3 boutons + buzzer
         ^ ZoneGroupTopology                |  +-- SHT4x (I2C) + ADC batterie
         ^ pochette (/getaa)                |
         v Next / Previous / Play / Pause   +--MQTT--> [Mosquitto] --> [Home Assistant]
```

## Le principe directeur

Un rafraîchissement de l'ePaper Spectra 6 prend **25 à 30 secondes**. Toute l'architecture
en découle : **on ne redessine que lorsque le contenu change réellement**. Concrètement, une
empreinte du morceau (`TrackURI` + titre + état de transport) est comparée à celle de
l'image actuellement affichée ; tant qu'elle est identique, l'écran n'est pas touché. La
barre de progression est donc figée à l'instant du rendu — c'est un choix, pas un défaut.

Corollaire : **les données publiées vers Home Assistant sont découplées de l'écran.**
Batterie, température et morceau en cours sont publiés en MQTT au moins toutes les 5 min,
même quand l'ePaper ne bouge pas.

## Machine à états

```
BOOT ──> WIFI ──> DISCOVER ──> POLL ──┬──> RENDER ──┐
                     ^                │             │
                     └────────────────┴─────────────┘
                                      │
                              (rien ne joue > 5 min)
                                      v
                                    SLEEP ──(timer 60 s | bouton)──> POLL
```

- **DISCOVER** — SSDP puis `GetZoneGroupState` : établit la liste des zones et, surtout, le
  **coordinateur** de chaque groupe. Interroger une enceinte esclave renvoie
  `NOT_IMPLEMENTED` pour les métadonnées : c'est le piège classique de l'API Sonos.
- **POLL** — toutes les 20 s, `GetTransportInfo` puis `GetPositionInfo` sur le coordinateur.
- **RENDER** — téléchargement de la pochette, décodage JPEG en PSRAM, tramage vers les
  6 couleurs, composition, un unique refresh.
- **SLEEP** — deep sleep par tranches de 60 s, réveil par timer ou par le bouton GPIO4.

## Sélection de la zone affichée

En mode `auto` : parmi les coordinateurs, on retient ceux dont l'état est `PLAYING` ; si
plusieurs jouent, on applique l'ordre de préférence de `SONOS_ZONE_PRIORITY` ; si aucun ne
joue, on garde la dernière zone active puis on bascule en écran de veille. L'entité `select`
exposée dans Home Assistant permet de forcer manuellement une pièce.

## Boutons

| Bouton | Appui court | Appui long (> 1 s) |
|---|---|---|
| Blanc gauche (GPIO5) | Morceau précédent | — |
| Blanc droit (GPIO4) | Morceau suivant | — |
| Vert (GPIO3) | Play / Pause | Redraw forcé |

Le buzzer bippe **immédiatement** à l'appui : avec 25 s de latence d'affichage, c'est le
seul retour utilisateur possible. Les appuis rapprochés sont coalescés — on ne lance le
rendu qu'après 1,5 s sans nouvel appui, pour n'avoir qu'un seul refresh.

GPIO4 sert aussi de broche de réveil. Le sommeil ne survient que lorsque rien ne joue, où
« morceau suivant » n'aurait pas de sens : un réveil par bouton déclenche donc un
re-sondage et un redraw, jamais un saut de piste.
