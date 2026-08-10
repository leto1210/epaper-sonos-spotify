# Avancement

Une livraison = un commit autonome et flashable. On ne passe pas à la suivante tant que le
test de la précédente ne passe pas.

## Reprise — séance du 10 août 2026, hors matériel

Le boîtier ne sera pas branché : tout ce qui suit se fait au clavier, avec
`pio test -e native` et `pio run` comme seuls juges. Le matériel servira ensuite à valider.

État au 9 août 2026 au soir : **L0 à L13 terminées et validées sur cible**, 81 tests au vert,
huit commits locaux **non poussés** (`git log origin/main..main`).

**État actuel (10 août, après L14) :** L14 complétée hors matériel — 91 tests, machine à états
pure testée offline. L15 en cours : documentation mise à jour (architecture, README), en
attente des photos et captures HA.

Il reste :

1. **L15 — finition.** Photo du panneau au mur (`docs/images/device.jpg`, attendue pour
   compléter le README), capture des neuf entités Home Assistant. Le schéma d'architecture
   a été mis à jour pour inclure SLEEP et les détails du 10-minute seuil + 60 s tranches.
   
2. **Validation matériel de L14 et L15.** Deep sleep — power draw, GPIO4 interrupt, 60 s
   timer. Photos et captures HA ne peuvent être produites que sur le boîtier réel ou une
   instance Home Assistant en fonctionnement.

Deux points à ne pas oublier :

- `src/config.h`, `mqtt-user.env`, `*.env` et `test/fixtures/raw/` sont git-ignorés et le
  dépôt est **public** — vérifier `git status --short` avant chaque commit.
- Reste ouverte la question de réécrire l'historique pour purger `mqtt-user.env` des anciens
  commits. Le mot de passe ayant été changé, c'est du confort, plus de la sécurité. Aucune
  autorisation donnée pour un `git filter-repo` + `push --force`.

---

**Reprise du développement sans le matériel** : voir [docs/development.md](../docs/development.md)
— il indique, livraison par livraison, ce qui se teste hors ligne (`pio test -e native`, sur
les captures réelles de `test/fixtures/`) et ce qui exige le boîtier branché.

- [x] **L0** — Squelette PlatformIO + doc + CI
      → `pio test -e native` passe, `pio run` compile
- [x] **L1** — Hello World ePaper
      → flashé sur cible : 800x480 initialisé, 1 seul rafraîchissement, 37,3 s
- [x] **L2** — Wi-Fi + NTP
      → IP obtenue en 1,3 s, heure NTP correcte, test TCP de joignabilité intégré
- [x] **L3** — Fixtures Sonos + parseur DIDL-Lite
      → 8 tests unitaires au vert sur 5 captures réelles anonymisées
- [x] **L4** — Topologie + choix du coordinateur
      → 8 zones et leurs coordinateurs en 350-500 ms, 10 tests sur la politique de choix
- [x] **L5** — `GetPositionInfo` en direct
      → morceau réel lu sur cible, position et URL de pochette correctes
- [x] **L6** — Layout texte sur ePaper (maquette C, repli A)
- [x] **L7** — Anti-redraw + compteur de refresh
      → validé sur cible : 1 rafraîchissement puis 4 sondages sans redraw
- [x] **L8** — Pochette : HTTP + JPEGDEC + tramage
      → 640x640 téléchargée, décodée et tramée en 1,0 s ; 8 tests sur le tramage
- [x] **L9** — Capteurs SHT4x + batterie
      → 30,7 °C / 36 %HR / 4150 mV lus sur cible, 5 tests sur la conversion
- [x] **L10** — Boutons + buzzer (Next / Previous / Play-Pause)
      → validé sur cible : les quatre gestes reconnus, un seul rafraîchissement chacun,
        et l'appui long n'entraîne pas de lecture/pause au relâchement ; 6 tests
- [x] **L11** — MQTT + Home Assistant Discovery
      → validé sur cible : 9/9 entités dans HA sous un seul appareil, mesures et morceau
        à jour ; 8 tests
- [x] **L12** — Bouton refresh et select de zone depuis HA
      → validé sur cible : bouton → redessin, sélecteur → zone imposée et état confirmé
        dans HA ; 5 tests
- [x] **L13** — Météo : abonnement MQTT + parsing + automatisation HA
      → validé sur cible : écran météo affiché quand plus aucune zone ne sait ce
        qu'elle joue, puis stable ; 11 tests
- [x] **L14** — Veille et deep sleep
      → `pio test -e native` au vert (10 nouveaux tests + 81 existants), intégration
        dans main.cpp avec esp_deep_sleep() ; 91/91 tests ; validation matériel en suspens
- [~] **L15** — Finition doc : photos, captures HA, architecture mise à jour
      → photo de l'écran météo dans le README ; restent la photo du panneau au mur et
        une capture des entités HA

## Revue

### Pictogrammes météo

Les pastilles de couleur disaient la famille de temps sans la nommer. Elles sont remplacées
par des pictogrammes dessinés au trait — soleil, croissant, nuage, pluie, éclair, flocons,
bourrasque — un grand dans le coin haut droit, un petit par créneau.

Dessinés en primitives géométriques plutôt qu'en bitmaps : les FreeFonts n'ont pas de
symboles météo, et une image tramée devrait exister en deux tailles et occuper de la flash.
Des cercles et des lignes se redimensionnent d'eux-mêmes, `r` étant le rayon utile.

Deux détails de dessin qui ne sont pas gratuits :
- le nuage est un **aplat sans contour** (trois disques et un rectangle de la même couleur) —
  un contour aurait laissé apparaître les traits intérieurs des disques ;
- les flocons sont **noirs**, pas blancs : du blanc sur fond blanc ne se verrait pas.

Le pictogramme se choisit sur l'énumération `Condition`, jamais sur le libellé traduit —
d'où l'ajout de `condition` à `weather::View`, couvert par deux tests. Une météo périmée
n'affiche aucun pictogramme : dessiner un soleil au-dessus de « Météo périmée » reviendrait
à affirmer ce qu'on vient de démentir.

**Validé sur le panneau, après une correction que seul le matériel pouvait révéler.** La
première version employait les six encres — soleil jaune, nuage bleu, éclair rouge. Sur la
photo de contrôle, les contours de 4 px ressortaient **bruns** et non noirs : le Spectra 6
compose un trait fin à partir de ses pigments, et seule une surface large garde une couleur
franche. Tout est repassé en noir et blanc, avec des traits épaissis de 9 % à 13 % du rayon
(minimum 3 px). Documenté dans `docs/hardware.md` : la couleur pour les aplats, le
monochrome pour le trait.

Les douze conditions ont été vérifiées **avant** le flash par un petit rastériseur qui
rejoue la même géométrie en Python (dans le bac à sable, non versionné) : douze
rafraîchissements de 37 s pour juger de proportions auraient été absurdes. Il a fait
apparaître trois défauts — un croissant flanqué d'une pleine lune blanche, des gouttes
longues comme des barreaux d'échelle, neige et grêle identiques.

Trois réglages typographiques ont suivi, chacun signalé sur photo et invisible autrement :

- la **température des créneaux** passe en graisse normale — en gras elle écrasait l'heure,
  alors que les deux valeurs se lisent ensemble ;
- **9 px de respiration** entre l'heure et le pictogramme, qui se lisait comme un accent du
  texte tant il y était collé ; le rayon passe de 20 à 18 px et tout le bloc remonte de 8 px,
  faute de quoi la ligne de précipitations serait passée sous le trait du bandeau ;
- le **grand pictogramme se centre sur le libellé** au lieu d'occuper une position fixe : sa
  position se calcule à partir de la largeur du texte, mesurée avec la police qui servira à
  le tracer. L'alignement tient donc du court « Orage » au long « Peu nuageux ». Le libellé
  reste aligné à droite — centré sous l'icône, il déborderait de la marge.

La photo du README montre l'écran définitif.

### Correction de L14 : la compilation cible échouait

L14 est passée en natif mais **pas** pour l'ESP32-S3, et la CI est restée rouge. Deux causes,
aucune visible depuis l'environnement `native` :

1. **`namespace sleep` entrait en collision avec `sleep()` de POSIX**, déclarée par
   `unistd.h` que la chaîne Espressif tire par `lwipopts.h`. Le compilateur refusait de voir
   le même nom désigner un espace de noms et une fonction. Renommé en `power`.
2. **`esp_deep_sleep()` prend une durée en microsecondes** ; elle était appelée sans
   argument. L'appel arme lui-même le réveil par timer, ce qui rend
   `esp_sleep_enable_timer_wakeup` redondant — supprimé.

La leçon vaut d'être notée : `pio test -e native` ne remplace pas `pio run`. Une livraison
écrite hors matériel doit passer les deux avant d'être poussée, et la CI le vérifie
justement dans cet ordre.

Le README pointait aussi vers `docs/images/device.jpg`, qui n'existe pas encore : image
cassée sur la page d'accueil d'un dépôt public. Remis en commentaire jusqu'à la photo.

### L14 (hors ligne, sans matériel)
Machine à états pure (aucune dépendance Arduino) : quand s'endormir, pour combien de temps,
ce qui réveille. Suivant le modèle de `core/pause_timer`, tous les états et transitions
se testent sur le Mac sans le boîtier.

**Seuils et tranches :**
- Inactivité : 10 minutes sans rien qui joue → entrée en sommeil
- Tranches : 60 secondes par réveil de timer
- Réveil : par timer (60 s) ou immédiatement si appui de bouton (GPIO4)

**Intégration :** `SleepManager` appelé depuis la boucle principale avant tout sondage.
Si `should_sleep` est vrai, la boucle invoque `esp_deep_sleep()` avec timer et GPIO4 comme
sources de réveil. Un réveil par timer redéclenche un sondage complet (jamais un rendu
direct) — sinon l'écran afficherait une fiche obsolète de 60 secondes.

**Tests :** 10 nouveaux tests couvrent les transitions, l'inactivité, les deux modes de
réveil, et le débordement de `millis()` au bout de 49 jours. Un seul point manqué au premier
passage (re-entrée en sommeil après réveil sur timer) — corrigé dans `inactive_since_ms_`
en le réinitialisant à la date de réveil plutôt qu'à 0.

**Validation matériel en suspens :** consommation en deep sleep, GPIO4 interrupt, durée réelle.

### L13 (partielle, sans matériel)
L'écran météo prend la place du morceau quand plus aucune zone ne sait ce qu'elle joue —
silence, télévision, entrée ligne. Auparavant l'écran restait figé sur la dernière fiche
connue.

Toutes les chaînes affichées sont construites dans `core/weather_view`, donc vérifiées par
les tests ; `display` ne fait plus que les poser. Trois décisions qui s'y voient :

- **Un rapport périmé masque ses chiffres** au lieu de les afficher discrètement. Le sujet
  MQTT est retenu : sans cela, un broker resservirait la météo d'avant-hier avec l'aplomb
  d'une mesure fraîche.
- **La ligne intérieure survit à l'absence de météo** : elle vient du SHT4x, pas du réseau.
- **La pluie ne s'affiche que s'il pleut.** Six colonnes « 0 mm » un jour d'été n'apprennent
  rien et chargent l'écran.

Faute de pictogrammes dans les FreeFonts, chaque créneau porte une pastille colorée — c'est
le seul endroit du projet où les six encres servent à porter du sens plutôt qu'à reproduire
une image.

L'empreinte anti-redraw ne retient que ce qui se voit : deux relevés à 32,58 et 32,62 °C ne
valent pas 37 s de rafraîchissement.

### L11 (partielle, sans matériel)
Les payloads se construisent dans `core/ha_discovery`, donc se vérifient au caractère près
sans broker. Deux tests valent surtout par le défaut qu'ils empêchent :

- **`unique_id` distincts** : un doublon ne provoque aucune erreur MQTT, il fait simplement
  disparaître une entité, l'autre l'écrasant. C'est le genre de panne qu'on met une heure à
  comprendre dans l'interface de Home Assistant.
- **bloc `device` identique partout** : sans lui, ce sont neuf appareils orphelins qui
  apparaissent au lieu d'un seul.

Côté transport, le tampon de `PubSubClient` est porté de 256 à 1024 octets : un message de
découverte en fait environ 700, et une publication trop grande **échoue silencieusement**.

Le morceau est publié **avant** le test d'anti-redraw : Home Assistant suit la lecture au
rythme du sondage, sans attendre les 37 s d'un rafraîchissement d'écran. Les mesures partent
toutes les 5 minutes, même écran figé.

### L12
Le sélecteur ne propose pas les noms de la configuration mais **ceux de la topologie réelle** :
la découverte est republiée avec les huit pièces dès le premier sondage. « Séjour » y figure
sous son vrai nom, « Sonos Séjour ».

`rankZones` traitait déjà le cas d'une zone imposée, y compris à l'arrêt — j'avais commencé à
réécrire ce filtre dans `main` avant de m'en rendre compte, et l'ai supprimé.

Deux détails corrigés parce qu'ils se voyaient à l'usage :
- un `button` ne déclare **pas** de `state_topic` : il resterait indisponible en attendant une
  valeur qui n'arrive jamais ;
- l'état est republié **avant** le rendu, sinon le sélecteur garde son ancienne valeur dans
  Home Assistant pendant les 37 s du rafraîchissement. Home Assistant le signalait :
  « state change could not be verified ».

C'est ce test qui a permis de voir enfin **l'écran météo** : forcer la Cuisine, qui ne sait rien,
fait basculer l'affichage. `[sonos] aucune metadonnee nulle part, ecran meteo` puis un seul
rafraîchissement, et plus rien ensuite.

### Recette matérielle de L10, L11 et L13

Les quatre gestes ont été reconnus, chacun suivi d'un seul rafraîchissement :

```
[boutons] precedent sur Sonos Holiday : ok      -> #4
[boutons] suivant sur Sonos Holiday : ok        -> #5
[boutons] pause sur Sonos Holiday : ok          -> #6
[boutons] redessin force
```

Le relâchement de l'appui long n'a produit aucune commande de transport : la garde tient sur
le matériel comme dans les tests.

**Un défaut trouvé uniquement sur cible.** La session MQTT tombait à chaque rafraîchissement :
le redessin bloque 37 s, pendant lesquelles personne n'entretient la session, et le keepalive
de `PubSubClient` est de 15 s. Le broker publiait donc le testament à chaque redessin — entités
en `unavailable`, puis reconnexion — et la publication des mesures qui suit le redessin tombait
dans ce trou. Home Assistant restait à 0 rafraîchissement, sans horodatage. Keepalive porté à
90 s. Aucun test hors ligne n'aurait pu voir cela : il fallait la durée réelle du panneau.

La météo arrive bien dès l'abonnement, le sujet étant retenu :
`[mqtt] meteo recue : Nuit claire, 30.7 C, 6 creneaux`. L'écran météo lui-même reste à voir :
une zone en pause conserve ses métadonnées, donc l'affichage reste sur le morceau.

### L10 (partielle, sans matériel)
La logique tient dans `core/buttons` et ne voit que des niveaux et une horloge : anti-rebond,
appui long et coalescence se testent donc entièrement sur le Mac. `src/buttons_io` se réduit à
trois `digitalRead` et au bip.

Deux points de conception qui ne se voient pas dans la recette :
1. **Lecture/pause relit l'état de l'enceinte** au lieu de le déduire de l'affichage. Celui-ci
   peut dater de plusieurs minutes, pendant lesquelles la musique a pu être pilotée depuis un
   téléphone — inverser l'affichage aurait envoyé la mauvaise commande une fois sur deux.
2. **L'appui long ne produit pas aussi un play/pause** au relâchement : sans cette garde, tout
   redessin forcé aurait mis la musique en pause.

La boucle principale échantillonne désormais à 10 ms, contre 200 ms : un appui bref serait
passé sous l'anti-rebond de 50 ms.

Un défaut attrapé par les tests, dans le test lui-même : le cas de débordement de `millis()`
échouait parce que l'assistant de test calculait une date de fin qui débordait, pas parce que
le contrôleur s'y perdait. Le contrôleur compare des différences signées.

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

### L4 et L5
La topologie des 8 zones, coordinateurs compris, est résolue en 350 à 500 ms depuis une seule
IP d'amorçage. La découverte SSDP n'a jamais été écrite : elle serait inutilisable ici, et un
code mort qu'on ne peut pas tester ne vaut pas mieux que pas de code.

**Défaut de conception attrapé sur le matériel** : ma politique traitait « en pause » et « en
lecture » à égalité dès le premier tri. Résultat, la Cuisine en pause — prioritaire dans la
configuration — était retenue alors que le Beam jouait vraiment. Le choix se fait désormais en
deux passes : ce qui joue d'abord, les préférences ensuite. Deux tests couvrent exactement ce
cas ; les tests initiaux ne le voyaient pas, puisqu'ils reproduisaient la même confusion que
le code.

Second écart entre configuration et réalité : les zones s'appellent « Sonos Séjour » et non
« Séjour ». Le firmware liste les noms réels au démarrage, et `config.example.h` le signale.

### L8
Trois défauts successifs, aucun visible autrement qu'en exécutant sur la cible :

1. **404 sur `/getaa`.** L'enceinte indexe la pochette par l'URI de la ressource (`<res>` du
   DIDL), pas par le `TrackURI`. En Spotify Connect les deux diffèrent — le second est un
   identifiant de session `x-sonos-vli:`. Mon test manuel avait réussi parce que j'y avais
   collé la bonne URI sans réaliser qu'elle venait d'un autre champ.
2. **Redémarrage en boucle.** L'objet `JPEGDEC`, déclaré sur la pile, embarque ses tables de
   Huffman et ses tampons de blocs : plusieurs dizaines de kilo-octets, contre 8 Ko pour la
   tâche `loop` d'Arduino. Le canari de pile a fait son travail. Alloué sur le tas, plutôt
   qu'en agrandissant la pile — la cause valait mieux que le symptôme.
3. **JPEG illisible.** L'enceinte sert l'image en `Transfer-Encoding: chunked`. Recopier le
   flux tel quel insérait les tailles de blocs au milieu de l'image. `curl` masquait
   complètement le problème en décodant les blocs pour moi.

Résultat mesuré : 74 Ko téléchargés, décodage à demi-résolution, réduction et tramage en
**1,0 s** au total.
