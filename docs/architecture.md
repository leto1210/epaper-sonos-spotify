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
## Entrée ligne : une platine, un ampli

Une zone branchée sur son entrée ligne (`x-rincon-stream:`) ne livre **aucune** métadonnée :
ni titre, ni artiste, ni pochette. Sonos ne sait rien de ce qui passe par la prise. L'écran
affiche donc un disque vinyle, le nom de la pièce, et le dit franchement — « Sonos ne dit pas
quoi ». Cela vaut mieux que de basculer sur la météo, qui laisserait croire la maison
silencieuse alors qu'elle ne l'est pas.

Ce cas passe **avant** la météo, mais **après** toute zone qui sait ce qu'elle joue : une
entrée ligne oubliée ne doit pas masquer un morceau Spotify diffusé ailleurs.

L'état de transport n'y est pas fiable — mesuré : le Séjour s'annonce `PAUSED_PLAYBACK`
pendant que la platine tourne, Sonos ne « jouant » pas une entrée ligne comme il joue un
morceau. L'écran s'affiche donc quel que soit cet état, mais le boîtier ne compte l'entrée
ligne comme une lecture — donc comme une raison de ne pas s'endormir — que si elle annonce
vraiment `PLAYING`. Sans cette nuance, une entrée ligne oubliée tiendrait le boîtier éveillé
indéfiniment.

## Quand l'écran passe à la météo

Trois cas, tous vérifiés sur le matériel :

1. aucune zone ne joue ni n'est en pause ;
2. plus aucune zone ne sait ce qu'elle joue — télévision ou silence ; l'entrée ligne, elle,
   a désormais son propre écran ;
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
- **Le compte à rebours de pause était faussé, et l'écran restait figé.** Symptôme observé :
  une zone mise en pause gardait sa fiche à l'écran indéfiniment, sans jamais rendre la place
  à la météo. La sauvegarde en mémoire RTC conservait un `millis()` **absolu** ; relu après un
  réveil, dans une horloge repartie de zéro, il donnait un écart *négatif*. Les cinq minutes
  ne pouvaient plus échoir.

Le cycle mesuré est alors : réveil, Wi-Fi, sondage, aucun redessin, rendormissement — soit
quelques secondes éveillé par minute.

### La règle : une date n'a de sens que dans son époque

Les trois pertes ci-dessus sont trois manifestations d'une seule : `millis()` compte le temps
depuis le démarrage, et le deep sleep crée un nouveau démarrage. Une valeur d'`millis()`
écrite avant un sommeil et relue après ne mesure plus rien.

Ce qui traverse le sommeil doit donc être une **durée**, jamais une date — c'est ce que fait
`PauseTimerState::paused_ms` — et le réveil doit **créditer le temps réellement dormi**
(`PauseTimer::deserialize(state, now_ms, slept_ms)`, alimenté par la durée que `main.cpp`
range dans `rtc::State::sleep_duration_ms` juste avant de s'endormir).

Les deux moitiés comptent, et pour des raisons différentes :

- sans le rebasage, l'écart devient négatif et le délai n'échoit **jamais** ;
- sans le crédit du sommeil, seul l'éveil est compté : un boîtier qui dort par tranches d'une
  minute ne veille que quelques secondes par minute, et cinq minutes de grâce prendraient
  des **heures**.

`SleepManager` portait exactement le même défaut et n'y a échappé que par accident :
`resumeAfterWake()`, écrit pour une tout autre raison, écrase la valeur restaurée avant
qu'elle ne serve. Le motif était donc présent en double, à moitié masqué — et c'est le genre
de chose qu'on ne voit qu'en cherchant les frères d'un défaut plutôt qu'en réparant celui
qu'on a sous les yeux.

Un troisième exemplaire a été trouvé de cette manière, et non par une panne : le **temps de
fonctionnement** publié à Home Assistant valait `millis() / 1000`. L'entité affichait donc
deux secondes sur un boîtier allumé depuis quarante minutes, donnant à lire des redémarrages
en boucle — et rendant inutilisable le seul diagnostic capable de signaler un *vrai*
redémarrage intempestif. Il est désormais cumulé en mémoire RTC, sommeil compris
(`core/uptime.h`), en réutilisant la durée que le correctif précédent y avait déjà rangée.

## Mise à jour par le réseau : essayée, retirée

L'OTA a été implémenté puis **retiré**. Le constat mérite d'être conservé, parce que rien
n'y était théorique.

La table de partitions (`default_16MB.csv`) déclare déjà `app0` et `app1` de 6,5 Mo chacune,
pour un firmware de 1,2 Mo : **aucun repartitionnement n'est nécessaire**. Ce n'est donc pas
là qu'était l'obstacle.

Trois murs se sont présentés, dans cet ordre :

1. **Le boîtier dort.** Un `ArduinoOTA` à l'écoute permanente est inutilisable : le boîtier
   n'est joignable que quelques secondes par minute. On a donc ouvert une fenêtre depuis un
   bouton Home Assistant — sauf qu'**une commande MQTT qui arrive pendant un sommeil est
   perdue**, la session n'étant pas persistante. C'était déjà écrit noir sur blanc dans la
   section consacrée à `SONOS_SLEEP_WHILE_PLAYING`, et le premier appui réel n'est jamais
   parvenu au boîtier.
2. **La commande retenue se rejoue.** Publier la commande en `retain` la fait attendre le
   réveil : c'est la bonne réponse au point précédent. Mais le boîtier est abonné à ce même
   sujet, donc il reçoit le message vide par lequel il efface lui-même la commande — et le
   relit comme une nouvelle demande. Ouvrir, effacer, ouvrir : le buzzer a bipé en continu
   jusqu'à suppression du message retenu depuis Home Assistant. La garde contre une charge
   utile vide existait pour le sélecteur de zone ; elle a été étendue à toutes les commandes,
   et elle reste en place.
3. **Le téléversement n'a jamais abouti.** Le boîtier authentifie, le transfert démarre, puis
   rompt vers 10 %. `espota` exige que la carte **rappelle le poste** sur un port arbitraire ;
   ici le boîtier est sur un autre sous-réseau que le poste de développement. La cause exacte
   n'a pas été établie.

Ce qu'il faut en retenir pour une reprise : sur ce réseau, une mise à jour **poussée** est le
mauvais sens. Le boîtier sait joindre le broker MQTT et les enceintes ; c'est donc une mise à
jour **tirée** — le boîtier va chercher un binaire sur une URL — qui a des chances
d'aboutir. Elle demande en revanche d'héberger le binaire et un numéro de version quelque
part, ce qui n'a pas été fait.

Le câble reste donc le seul chemin de flashage. `kFirmwareVersion` est publié comme
`sw_version` dans Home Assistant : c'est ce qui permettra, le jour venu, de vérifier sans
console qu'une mise à jour a pris.

## Entrée en deep sleep

Une fois l'écran météo affiché (c'est-à-dire, rien ne joue), le boîtier compte l'inactivité.
Au-delà de 10 minutes sans activité musicale, il entre en deep sleep par tranches de 60 s.
Un sondage s'effectue à chaque réveil du timer ou du bouton. Avec `ext1`, les trois boutons
réveillent et déclenchent leur action.

La consommation a été mesurée sur le matériel, au wattmètre USB : **0,050 W** de moyenne sur
un cycle de sommeil, contre **0,31 W** éveillé — facteur 6,2. Le plancher est de 0,028 W, et
il ne vient pas du processeur mais de la carte elle-même (pont série, régulateurs, circuit de
charge) : aucun firmware ne descendra dessous. Relevé complet dans
[tasks/l19-validation.md](../tasks/l19-validation.md), synthèse dans
[docs/hardware.md](hardware.md).

Dormir **aussi pendant la lecture** a été implémenté et mesuré — 0,116 W, soit 64 h
d'autonomie au lieu de 23 — mais l'option `SONOS_SLEEP_WHILE_PLAYING` est livrée désactivée :
les commandes venues de Home Assistant sont perdues si elles arrivent pendant un sommeil, ce
qui a été vérifié et non supposé.
