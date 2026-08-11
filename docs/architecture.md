# Architecture

```
[Enceintes Sonos :1400] <--SOAP/HTTP--> [ESP32-S3] --SPI--> [ePaper 800x480]
         ^ découverte SSDP                  |  ^ 3 boutons + buzzer
         ^ ZoneGroupTopology                |  +-- SHT4x (I2C) + ADC batterie
         ^ pochette (/getaa)                |
         v Next / Previous / Play / Pause   +--MQTT--> [Mosquitto] --> [Home Assistant]
```

## Le principe directeur

Un rafraîchissement de l'ePaper Spectra 6 prend **37 secondes**, mesurées sur le boîtier —
au-dessus des 25 à 30 s annoncées par Seeed. Toute l'architecture
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

- **DISCOVER** — `GetZoneGroupState` depuis l'IP d'amorçage : établit la liste des zones et,
  surtout, le **coordinateur** de chaque groupe. Interroger une enceinte esclave renvoie
  `NOT_IMPLEMENTED` pour les métadonnées : c'est le piège classique de l'API Sonos. Mesuré à
  350-500 ms pour huit zones. La découverte SSDP n'est pas implémentée — voir
  [docs/sonos-api.md](sonos-api.md).
- **POLL** — toutes les 20 s, `GetTransportInfo` puis `GetPositionInfo` sur le coordinateur.
- **RENDER** — téléchargement de la pochette, décodage JPEG en PSRAM, tramage vers les
  6 couleurs, composition, un unique refresh.
- **SLEEP** — deep sleep par tranches de 60 s quand rien ne joue depuis 10 minutes.
  Réveil par timer (60 s) ou par GPIO4 (appui de bouton). La transition de SLEEP vers
  POLL redéclenche un sondage complet — le boîtier ne passe jamais en RENDER depuis le
  réveil, sinon il afficherait une fiche obsolète. Cette garde empêche aussi un
  rafraîchissement coûteux à chaque réveil de timer.

## Sélection de la zone affichée

En mode `auto`, le choix se fait en **deux passes** :

1. parmi les zones qui jouent réellement, la première de `SONOS_ZONE_PRIORITY`, ou à défaut
   n'importe laquelle ;
2. seulement ensuite, même chose en acceptant les zones en pause ;
3. à défaut, la dernière zone retenue — l'écran conserve alors sa fiche plutôt que de se
   vider ;
4. sinon, écran météo.

L'ordre des passes n'est pas un détail : traiter « en pause » et « en lecture » à égalité
faisait remonter une pièce favorite mise en pause devant celle où la musique tournait
vraiment. Le cas est couvert par un test.

L'entité `select` exposée dans Home Assistant l'emporte sur toute cette logique. Une zone
forcée devenue introuvable n'affiche rien plutôt que de basculer en silence sur une autre
pièce.

## Boutons

| Bouton | Appui court | Appui long (> 1 s) |
|---|---|---|
| Blanc gauche (GPIO5) | Morceau précédent | — |
| Blanc droit (GPIO4) | Morceau suivant | — |
| Vert (GPIO3) | Play / Pause | Redraw forcé |

**Au réveil du deep sleep** : les trois boutons réveillent avec `esp_sleep_enable_ext1_wakeup()`.
`esp_sleep_get_ext1_wakeup_status()` indique lequel, et son action est exécutée immédiatement.
Justification : si l'utilisateur appuie sur « suivant » pendant une lecture pour changer de
morceau, il s'attend à un changement dès que le boîtier se réveille, pas à un sondage blanc
qui le laisserait sur le même morceau une minute de plus. Alternative rejetée : « un réveil
ne saute jamais de morceau » avait du sens quand seul GPIO4 réveillait, mais avec trois
boutons, exécuter l'action du bouton réveilleur est plus utile.

Attention : en deep sleep, le bouton est encore enfoncé à la reprise de `setup()`. Le buzzer
doit biper quand même, sinon l'utilisateur ne saurait pas que l'appui a été reconnu.
`buttons_io::poll()` détecte l'appui qui se relâche, ce qui survient quelques centaines de
millisecondes après le réveil. Le bip suit, après la requête réseau — un délai que le boîtier
en sommeil ne peut pas compenser.
## Quand l'écran passe à la météo

Trois cas, tous vérifiés sur le matériel :

1. aucune zone ne joue ni n'est en pause ;
2. plus aucune zone ne sait ce qu'elle joue — télévision, entrée ligne, silence ;
3. la zone affichée est **en pause depuis plus de cinq minutes**.

Le troisième cas est venu de l'usage : une fiche de morceau figée depuis une demi-heure
n'informe plus de rien. Cinq minutes couvrent une pause de circonstance sans laisser
l'affichage bloqué toute la nuit. Le compteur repart à zéro à la reprise, et à chaque
changement de pièce — mettre en pause dans le séjour puis reprendre dans la cuisine, c'est
une nouvelle écoute.

Home Assistant, lui, continue de recevoir le morceau : seul l'affichage change.

## Ce que le deep sleep efface, et ce qu'il faut sauver

Sur l'ESP32, un réveil de deep sleep ne reprend pas où l'on s'était arrêté : il repasse par
`setup()`, avec toutes les variables réinitialisées. Deux conséquences, l'une et l'autre
constatées sur le matériel :

- **L'empreinte anti-redraw était perdue.** Chaque réveil repartait avec un écran réputé
  vierge et redessinait : un rafraîchissement de 37 s toutes les minutes, exactement ce que
  la veille est censée éviter. Elle est désormais conservée sous forme de condensé dans la
  mémoire RTC (`RTC_DATA_ATTR`), qui reste alimentée pendant le sommeil, avec le compteur de
  rafraîchissements — sans quoi celui-ci retombait à zéro dans Home Assistant à chaque
  réveil.
- **Le compteur d'inactivité aussi.** `SleepManager` réclamait donc dix nouvelles minutes
  avant de se rendormir : le boîtier serait resté éveillé dix minutes pour une minute de
  sommeil. `resumeAfterWake()` antidate l'inactivité après un réveil par minuterie, si bien
  que le sommeil reprend dès le sondage terminé.

Le cycle mesuré est alors : réveil, Wi-Fi, sondage, aucun redessin, rendormissement — soit
quelques secondes éveillé par minute.

## Entrée en deep sleep

Une fois l'écran météo affiché (c'est-à-dire, rien ne joue), le boîtier compte l'inactivité.
Au-delà de 10 minutes sans activité musicale, il entre en deep sleep par tranches de 60 s.
Un sondage s'effectue à chaque réveil du timer ou du bouton. Avec `ext1`, les trois boutons
réveillent et déclenchent leur action.

La consommation en deep sleep doit être mesurée sur le matériel : voir
[docs/hardware.md](hardware.md).
