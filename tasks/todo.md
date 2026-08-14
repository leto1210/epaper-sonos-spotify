# Avancement

Une livraison = un commit autonome et flashable. On ne passe pas à la suivante tant que le
test de la précédente ne passe pas.

## État au 14 août 2026

**L0 à L23 terminées**, 142 tests au vert. Le boîtier tourne au mur, batterie remise en place
— la campagne de mesure est close, et elle exigeait de la retirer (voir L19).

**La refonte énergie est terminée.** Le cycle de sommeil divise la consommation par 6,2 quand
rien ne joue : 0,050 W contre 0,31 W éveillé. Relevé complet dans
[`tasks/l19-validation.md`](l19-validation.md).

**La dernière question du brief est tranchée**, et elle l'est par la mesure, pas par
l'extrapolation. Dormir aussi *pendant la lecture* est implémenté sous l'option
`SONOS_SLEEP_WHILE_PLAYING` : 0,1162 W contre 0,3149 W, soit 64 h d'autonomie au lieu de 23.
L'option est néanmoins **livrée désactivée** — les commandes venues de Home Assistant sont
perdues quand elles arrivent pendant un sommeil, ce qui a été vérifié sur le matériel. Pour un
boîtier alimenté en permanence, la fiabilité du pilotage vaut mieux qu'une autonomie qu'on
n'utilise pas.

Il ne manque plus qu'une **photo du panneau au mur** (`docs/images/device.jpg`, encore un
`TODO` dans le README), qui ne dépend pas du code.

**La mise à jour par le réseau a été essayée puis retirée** (L21). Le câble reste le seul
chemin de flashage. Le constat complet est dans
[docs/architecture.md](../docs/architecture.md) : ce n'est pas la place en flash qui
manquait, mais le sens de la mise à jour — *poussée*, elle exige que le boîtier rappelle le
poste de développement, ce que ce réseau ne permet pas.

Un **audit de sécurité** a été publié sur `origin/main` (`7a0e130`) par une autre séance.
Il relève trois choses justes — transport en clair, validation insuffisante des commandes
MQTT, dépendances non épinglées — mais **manque la fuite réelle** : `mqtt-user.env` est
toujours lisible dans l'historique public. Voir la revue en fin de fichier.

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
        dans main.cpp avec esp_deep_sleep() ; validée sur matériel depuis, en L19 et L20
- [~] **L15** — Finition doc : photos, captures HA, architecture mise à jour
      → photo de l'écran météo dans le README ; restent la photo du panneau au mur et
        une capture des entités HA

## Refonte énergie — dormir entre les sondages (11 août 2026)

Brief complet dans [veille-continue.md](veille-continue.md). La branche
`energie/sommeil-entre-sondages` a été fusionnée dans `main` puis supprimée.

Objectif : faire dormir le boîtier entre deux sondages, **même pendant la lecture**. Le
sommeil quand rien ne joue est mesuré et acquis ; la part « pendant la lecture » ne l'est pas.

- [x] **L16** — État RTC étendu (a3e9c56)
      → sérialisation de `g_forced_zone`, `g_last_zone`, `g_last_ip`, PauseTimer, SleepManager
      → 4 tests nouveaux sur ser/deser, intégration dans main.cpp
      → `pio test -e native` + `pio run` au vert
      
- [x] **L17** — Réveil par 3 boutons (ext1) (b48e9da)
      → GPIO3, GPIO4, GPIO5 en `ext1`, tous actifs bas
      → `esp_sleep_get_ext1_wakeup_status()` pour identifier le bouton
      → **décision à documenter** : exécuter l'action du bouton réveilleur ou garder « ne pas sauter » ?
      → tests de la logique de réveil
        **Décision prise : exécuter l'action du bouton.** Documentée dans `docs/architecture.md`.
- [x] **L18** — Gestion MQTT et disponibilité HA
      → keepalive adapté à intervalle réaliste (mesure cyclique)
      → publication `online` en `retain`
      → comportement documenté dans `docs/home-assistant.md`
      → entités restent `available` d'un cycle à l'autre
      → ajout de `mqtt::beforeDeepSleep()` : fermeture propre MQTT, politique
        `online` retained en sommeil court / `offline` retained en sommeil long
      
- [x] **L19** — Mesure réelle et validation (14 août 2026)
      → cycle de sommeil : **0,050 W** de moyenne, contre **0,31 W** éveillé — facteur 6,2
      → plancher de deep sleep 0,028 W, pointes d'éveil 0,287 W, un réveil toutes les 67 s
      → autonomie : 24 h en lecture continue, 148 h quand rien ne joue
      → **la mesure n'a de sens que batterie retirée** : branchée, le chargeur limite le
        courant d'entrée et un rafraîchissement de 37 s devient invisible
      → relevé complet et verdict dans [l19-validation.md](l19-validation.md)

- [x] **L20** — Le compte à rebours de pause survit au sommeil (14 août 2026, d0f29ba)
      → **signalé par l'usage, pas par un test** : « l'écran reste figé sur la pause »
      → cause : la mémoire RTC conservait un `millis()` **absolu**. Le deep sleep repasse par
        `setup()` et `millis()` y repart de zéro ; relue dans cette époque neuve, la date
        donnait un écart *négatif* et les cinq minutes n'échoyaient jamais
      → correctif : on persiste une **durée écoulée**, et le réveil crédite le temps dormi
        (`rtc::State::sleep_duration_ms`). Sans ce crédit, un boîtier dormant par tranches
        d'une minute aurait mis des heures à atteindre cinq minutes de grâce
      → deux tests dans `test/test_pause/test_pause.cpp`, dont un qui **échoue sur l'ancien
        code** — c'est lui qui prouve qu'on a corrigé la bonne chose ; l'autre vérifie le sens
        inverse, que le sommeil crédité ne déclenche pas la bascule d'avance
      → validé sur cible : bascule vers la météo à 218 s de pause, rafraîchissement #2 en
        37,5 s, puis huit sondages sans redessiner
      → `SleepManager` portait le même défaut, masqué par `resumeAfterWake()` qui écrase la
        valeur restaurée. La règle générale est écrite dans
        [docs/architecture.md](../docs/architecture.md)

- [x] **L15** — Finition doc : captures Home Assistant (14 août 2026, b8a3d8d)
      → carte d'entités dans le README, page appareil dans `docs/home-assistant.md`
      → `ePaper_HA_MQTT.png` était un fichier **WebP** portant une extension `.png` : GitHub
        sert l'image d'après son extension, l'affichage aurait cassé chez une partie des
        lecteurs. Converti en PNG véritable avant publication
      → **reste ouvert** : la photo du panneau au mur (`docs/images/device.jpg`), qui ne
        dépend pas du code

- [x] **L23** — Le temps de fonctionnement compte le sommeil (14 août 2026, baec507)
      → repéré sur une capture, pas par une panne : « 2,00 s » sur un boîtier allumé depuis
        quarante minutes. `millis()` compte depuis le démarrage, et un réveil *est* un
        démarrage
      → l'entité donnait à lire des redémarrages en boucle, rendant inutilisable le seul
        diagnostic capable de signaler un *vrai* redémarrage intempestif
      → troisième exemplaire du motif corrigé en L20, trouvé en cherchant ses frères
      → cumul en mémoire RTC sur 64 bits, sommeil compris ; des secondes perdraient jusqu'à
        une seconde par cycle, soit une demi-heure de dérive par jour
      → 3 tests dans `test_rtc`, dont la monotonie à travers vingt cycles de sommeil

- [x] **L22** — Pictogramme de pile devant le pourcentage (14 août 2026, 0e34ea9)
      → jauge proportionnelle, éclair quand le circuit charge — information que le firmware
        publiait à Home Assistant sans l'afficher nulle part
      → **l'éclair est posé à côté de la pile, pas dedans.** Quatre itérations sur l'aperçu
        Python : à l'intérieur, il faut loger trois bandes de 3 px dans une vingtaine de
        pixels. Blanc cerné de noir, il se fragmentait dès que la frontière de la jauge le
        traversait ; l'inverse disparaissait sur l'aplat plein
      → géométrie dans `core/battery_icon`, 7 tests : bornes, mesure absente qui ne doit
        rien dessiner, et corps plus étroit que son contour — dont la largeur négative
        serait lue comme un énorme entier non signé par le tracé
      → remplissage de polygone par balayage : la bibliothèque ne remplit que des triangles,
        et découper l'éclair imposerait de traiter sa concavité

- [~] **L21** — Mise à jour par le réseau : essayée, **retirée** (14 août 2026)
      → la table de partitions déclarait déjà `app0`/`app1` de 6,5 Mo : aucun
        repartitionnement n'était nécessaire, l'obstacle n'était pas là
      → trois murs, dont deux tenaient à ma propre conception : le boîtier dort et perdait
        la commande ; publiée en `retain` elle se rejouait en boucle, le boîtier étant
        abonné au sujet où il publiait son propre effacement — buzzer en continu
      → le téléversement démarre, authentifie, et rompt vers 10 % : `espota` exige que la
        carte rappelle le poste, qui est sur un autre sous-réseau. Cause exacte non établie
      → **conclusion** : sur ce réseau, une mise à jour *poussée* va dans le mauvais sens.
        Une mise à jour *tirée* — le boîtier va chercher un binaire sur une URL — a des
        chances d'aboutir, mais demande d'héberger binaire et version. Non fait
      → conservé du chantier : la garde contre une charge utile vide sur **toutes** les
        commandes, qui manquait et que l'audit de sécurité relève par ailleurs
      → constat complet dans [docs/architecture.md](../docs/architecture.md)

## Revue

### Entrée ligne, accents, et météo perdue au réveil (14 août 2026)

Trois défauts trouvés le même jour, tous sur le matériel, aucun visible autrement.

**Le Séjour diffusait par sa prise ligne et l'écran affichait la météo.** Sonos ne livre
aucune métadonnée dans ce mode : ni titre, ni artiste, ni pochette. Un écran dédié montre
désormais un disque vinyle et le nom de la pièce. Le cas passe avant la météo, mais après
toute zone qui sait ce qu'elle joue. Détail mesuré : le Séjour s'annonce `PAUSED_PLAYBACK`
pendant que la platine tourne — Sonos ne « joue » pas une entrée ligne comme un morceau —
donc l'écran s'affiche quel que soit l'état, mais l'entrée ligne ne compte comme lecture, et
donc comme raison de rester éveillé, que si elle annonce vraiment `PLAYING`.

**« Sonos Séjour » s'affichait « Sonos Sjour ».** Les FreeFonts d'Adafruit ne couvrent que
l'ASCII et suppriment le reste **sans avertissement** — le même piège que les polices
numérotées, déjà documenté, mais frappant cette fois du texte venu du réseau. Il ne touchait
pas que les noms de zones : un titre français y perdait ses lettres une à une, et les
apostrophes courbes comme les tirets longs disparaissaient de même. `core/text_fold` replie
accents, ligatures et ponctuation, avec sept tests.

**« Météo indisponible » alors que l'automatisation publiait.** Depuis que le boîtier se
rendort toutes les minutes, il ne reste éveillé que trois secondes ; le bulletin vivait en
RAM et repartait à zéro à chaque réveil. Le sujet MQTT est retenu et l'abonnement le renvoie,
mais la livraison ne tient pas toujours dans cette fenêtre. L16 avait sauvé la zone et les
minuteries, pas le bulletin ni l'horodatage du dernier rafraîchissement.

### Mesure d'énergie : ce que la prise USB cachait

Le premier relevé annonçait 1 W, puis 2,2 W. Les deux étaient faux, pour la même raison : le
chargeur du reTerminal limite le courant d'entrée, et quand l'ESP32 consomme plus il prend
d'autant moins pour la batterie. Un rafraîchissement complet de 37 s en devenait
**invisible** — 2,211 W au repos contre 2,220 W pendant. C'est ce détail, et non un capteur
défaillant, qui rendait le protocole de L19 inopérant.

Batterie retirée, la même mesure donne **0,31 W** éveillé et **0,050 W** en cycle de sommeil.
Deux surprises au passage : le rafraîchissement de l'ePaper, soupçonné d'être le poste
coûteux, n'ajoute qu'un dixième de watt — c'est la radio qui domine ; et le plancher de deep
sleep vaut 5,4 mA, très au-dessus des microampères d'un ESP32-S3, parce que la carte garde
son pont série et ses régulateurs alimentés.

Cela valide rétrospectivement le retrait de la réduction du processeur à 80 MHz : elle
allongeait le rafraîchissement de 3,7 s pour économiser sur un poste négligeable.

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

### L14 sur cible : trois défauts, aucun visible hors matériel

La machine à états était juste et ses tests passaient ; c'est son **intégration** qui ne
tenait pas. Trois défauts, trouvés en relisant le code avant l'essai puis en observant le
boîtier :

1. **Le boîtier ne s'endormait jamais.** `anything_playing` se déduisait de `g_last_zone`,
   qui garde la dernière zone connue pour les boutons et n'est jamais vidé. Dès le premier
   morceau trouvé, le firmware se croyait en lecture pour toujours. Un drapeau distinct est
   désormais réévalué à chaque sondage — et une zone *en pause* ne joue pas.
2. **Un rafraîchissement de 37 s à chaque réveil.** Le deep sleep repasse par `setup()` :
   l'empreinte anti-redraw repartait vide. Elle est conservée en mémoire RTC sous forme de
   condensé FNV-1a, avec le compteur de rafraîchissements. Une collision de condensé
   coûterait un rafraîchissement manqué, jamais un affichage faux.
3. **Dix minutes éveillé pour une minute de sommeil.** `SleepManager` perdait aussi son
   compteur d'inactivité au réveil et réclamait un nouveau délai complet.
   `resumeAfterWake()` l'antidate après un réveil par minuterie ; deux tests le couvrent.

La décision de veille a par ailleurs été déplacée **après** le sondage : placée avant, elle
rendormait le boîtier sans qu'il ait jamais interrogé Sonos.

Cycle observé sur cible :

```
[veille] deep sleep pour 60000 ms
[boot] ePaper Spotify 0.1.0
[veille] reveil par minuterie, 3 rafraichissement(s) deja compte(s)
[ecran] inchange, pas de rafraichissement
[veille] deep sleep pour 60000 ms
```

Home Assistant reste disponible d'un réveil à l'autre et conserve son compteur à 3. Restent
à mesurer la **consommation réelle** et à vérifier le **réveil par GPIO4**.

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

**Validé sur matériel depuis (L19) :** consommation mesurée au wattmètre — 0,050 W en cycle de
sommeil contre 0,31 W éveillé — et réveil par les trois boutons en `ext1`. Un défaut est
apparu ensuite à l'usage, que ces tests ne pouvaient pas voir : voir L20.

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
