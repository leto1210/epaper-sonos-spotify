# Instructions pour l'assistant de code

Firmware Arduino/C++ pour un **Seeed reTerminal E1002** (ESP32-S3, ePaper couleur 7,3"
800×480). Il affiche le morceau Spotify joué sur les enceintes Sonos du réseau local, en
interrogeant directement les enceintes en UPnP/SOAP — aucun service en ligne.

## La contrainte qui explique tout le reste

Un rafraîchissement complet du panneau prend **37 secondes**, mesurées, et il n'existe pas de
rafraîchissement partiel en couleur. Toute l'architecture en découle : **on ne redessine que
lorsque le contenu change réellement**. Un compteur de rafraîchissements est exposé dans Home
Assistant comme garde-fou : s'il grimpe alors que la musique ne change pas, c'est une
régression.

## Structure

| Répertoire | Rôle |
|---|---|
| `src/core/` | Logique pure, **sans dépendance Arduino** : parsing SOAP, choix de zone, mise en page, tramage, payloads MQTT, machines à états |
| `src/` (racine) | Fines couches d'entrée-sortie : Wi-Fi, SPI, ADC, I²C, HTTP |
| `test/` | Tests Unity exécutés en natif sur le Mac, sur des captures réelles anonymisées |
| `docs/` | Documentation du dépôt public |
| `tasks/` | Suivi d'avancement et briefs de travail |

Règle : si un test exige le boîtier branché, c'est que la logique est au mauvais endroit.

## Commandes

```bash
~/.platformio/penv/bin/pio test -e native                      # tests unitaires
~/.platformio/penv/bin/pio run                                 # compilation ESP32-S3
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/cu.usbserial-XXX
```

**Les deux premières avant tout commit.** `pio test -e native` ne remplace pas `pio run` :
une livraison entière est déjà passée en natif tout en refusant de compiler pour la cible.

## Style

- Commentaires, messages de commit et documentation **en français**.
- Un commentaire explique **pourquoi**, jamais **quoi**. Les commentaires qui ont de la valeur
  ici sont ceux qui consignent un piège matériel ou une décision contre-intuitive.
- Les constantes de disposition sont nommées et commentées avec leur raison d'être.
- Pas de `TODO` sans échéance ni propriétaire : le suivi vit dans `tasks/todo.md`.

## Pièges de cette plateforme, tous rencontrés en vrai

- **Polices** : les polices numérotées de `Seeed_GFX` ne sont pas générales — la 6 est une
  police d'horloge qui ne contient que `1234567890:-.apm`, et les caractères absents sont
  supprimés **silencieusement**. N'utiliser que les FreeFonts d'Adafruit.
- **Couleur** : le Spectra 6 ne rend franchement que les **aplats larges**. Un trait de
  quelques pixels ressort délavé, voire brun. Les pictogrammes sont donc monochromes.
- **Console série** : le pont est un CH340 sur UART0, d'où `ARDUINO_USB_CDC_ON_BOOT=0`.
  Ouvrir le port **réinitialise l'ESP32** par DTR/RTS.
- **Deep sleep** : le réveil repasse par `setup()`. Seul ce qui est en `RTC_DATA_ATTR`
  survit. `namespace sleep` ne compile pas pour la cible — `unistd.h` déclare `sleep()`.
- **MQTT** : le tampon de `PubSubClient` fait 256 octets par défaut, un message de découverte
  en fait ~700, et une publication trop grande **échoue sans rien dire**. Porté à 1024, avec
  un keepalive de 90 s pour survivre aux 37 s d'un rafraîchissement.
- **Mémoire** : les tampons d'image vivent en PSRAM (`ps_malloc`). Un `JPEGDEC` sur la pile
  déborde les 8 Ko de la tâche `loop`.
- **Sonos** : seule l'enceinte **coordinatrice** d'un groupe connaît le morceau ; une esclave
  répond `NOT_IMPLEMENTED`. En Spotify Connect, les métadonnées n'existent que pendant la
  lecture effective, et la pochette s'indexe sur l'URI `<res>` du DIDL, pas sur `TrackURI`.

## Sécurité

Le dépôt est **public**. `src/config.h`, `mqtt-user.env`, `*.env` et `test/fixtures/raw/`
sont git-ignorés et ne doivent jamais être commités : ils contiennent identifiants Wi-Fi,
mot de passe MQTT, adresses privées et identifiants de foyer Sonos. **Vérifier
`git status --short` avant chaque commit.** Les captures Sonos passent par
`tools/scrub_fixture.py` avant d'être versionnées.
