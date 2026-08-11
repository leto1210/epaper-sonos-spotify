# Refonte énergie : dormir aussi pendant la lecture

Brief de travail pour une session assistée (Copilot / Claude Code). À lire en entier avant
d'écrire une ligne : la moitié des pièges décrits ici ont déjà coûté une session sur cette
base de code.

> **État au 11 août 2026.** Les livraisons **L16 à L18** — état RTC étendu, réveil `ext1`
> sur les trois boutons, disponibilité MQTT — sont faites et fusionnées dans `main`. Elles
> ont été écrites en parallèle de ce brief, sur `main` directement ; la branche
> `energie/sommeil-entre-sondages` n'a donc jamais porté que ce document et a été supprimée.
>
> **Il reste L19 : la mesure**, décrite dans [`l19-validation.md`](l19-validation.md). C'est
> elle qui tranche. Voir les critères d'acceptation en fin de page — sans mesure, la refonte
> n'est pas validée.

## Le problème

Le boîtier consomme **~1 W** à la prise, soit ~200 mA sous 5 V — l'ordre de grandeur d'un
ESP32-S3 dont la radio Wi-Fi est associée. Le deep sleep, lui, se compte en microampères.

Aujourd'hui le firmware ne s'endort que si **rien n'a joué pendant dix minutes**
(`core/sleep_manager`). Tant que la musique tourne, la radio reste allumée en permanence.
Sur la batterie de 2000 mAh (~7,4 Wh), cela donne un ordre de grandeur de sept heures.

## L'objectif

Dormir **entre deux sondages**, y compris pendant la lecture. L'image d'un ePaper est
bistable : elle survit au sommeil sans consommer. Le cycle visé :

```
réveil -> Wi-Fi -> sondage Sonos -> (redessin seulement si le morceau a changé) -> sommeil
```

Cible : quelques secondes éveillé par tranche de sondage, au lieu d'un éveil permanent.

## Ce qui est déjà en place, et qu'il ne faut pas refaire

- `core/sleep_manager` — seuil d'inactivité et tranches de sommeil, avec ses tests.
- `RTC_DATA_ATTR` dans `src/main.cpp` — l'empreinte anti-redraw (condensé FNV-1a) et le
  compteur de rafraîchissements survivent déjà au sommeil.
- `Seeed_GFX` met le panneau en veille (`EPD_SLEEP()`) à la fin de chaque mise à jour.
- `WiFi.setSleep(true)` est actif depuis L2.

## Les pièges, tous déjà rencontrés sur ce matériel

1. **Le deep sleep repasse par `setup()`.** Toute variable non marquée `RTC_DATA_ATTR` est
   perdue. C'est ce qui avait provoqué un rafraîchissement de 37 s à chaque réveil, puis un
   boîtier qui restait éveillé dix minutes pour une minute de sommeil.
2. **Ouvrir le port série réinitialise l'ESP32** par DTR/RTS. Un boîtier observé à la console
   vient de redémarrer : impossible d'y observer un cycle de sommeil sans en tenir compte.
   Voir `docs/hardware.md`.
3. **`namespace sleep` ne compile pas** pour la cible : `unistd.h` déclare `sleep()`. D'où
   `namespace power`. Le défaut n'apparaît qu'avec `pio run`, jamais en natif.
4. **`pio test -e native` ne remplace pas `pio run`.** Les deux, avant tout commit.
5. **Un rafraîchissement bloque 37 s**, pendant lesquelles rien n'entretient la session MQTT.
   Le keepalive est à 90 s pour cette raison.

## Le travail

### 1. État à sauver en mémoire RTC

Recenser ce qui doit survivre au sommeil, au-delà de l'existant :

- la zone imposée depuis Home Assistant (`g_forced_zone`) — aujourd'hui volontairement
  oubliée au redémarrage, ce qui n'a plus de sens si le boîtier redémarre chaque minute ;
- la dernière zone et son coordinateur (`g_last_zone`, `g_last_ip`), sans quoi un appui de
  bouton juste après un réveil n'a aucune cible ;
- l'état de `idle::PauseTimer`, sinon le délai de grâce de cinq minutes ne s'écoule jamais ;
- l'état de `power::SleepManager`.

Les `std::string` ne peuvent pas aller en mémoire RTC. Prévoir des tampons de taille fixe
(`char[N]`) ou des condensés, et une fonction de sérialisation testable dans `src/core/`.

### 2. Réveil par les trois boutons

Aujourd'hui seul GPIO4 réveille (`ext0`). Il faut `ext1` sur GPIO3, GPIO4 et GPIO5, tous
actifs à l'état bas — ce sont des broches RTC sur l'ESP32-S3, donc éligibles.

Au réveil, `esp_sleep_get_ext1_wakeup_status()` indique quelle broche a déclenché. La règle
actuelle « un réveil ne saute jamais de morceau » a été écrite quand GPIO4 était la seule
broche de réveil ; avec `ext1` on sait quel bouton a été pressé, et l'action peut être
exécutée délibérément. **Ce choix est à trancher explicitement et à documenter dans
`docs/architecture.md`.**

Attention : le buzzer doit biper à l'appui. Après un réveil, l'appui est déjà terminé —
vérifier que l'accusé de réception reste perceptible.

### 3. Disponibilité MQTT

Un boîtier qui se rendort toutes les vingt secondes se déconnecte du broker à chaque cycle.
Sans précaution, les entités de Home Assistant vont **osciller entre disponible et
indisponible**, ce qui rend l'appareil inutilisable dans une automatisation.

Pistes à évaluer, à ne pas cumuler sans mesure :

- publier `online` en `retain` et ne poser un testament (`will`) que sur les arrêts longs ;
- allonger le keepalive au-delà de la durée d'une tranche de sommeil ;
- accepter la déconnexion et documenter le comportement attendu côté Home Assistant.

### 4. Budget réel d'un réveil

À mesurer avant de conclure que la refonte gagne quelque chose :

| Poste | Ordre de grandeur connu |
|---|---|
| Association Wi-Fi | 1,7 s (mesuré, L2) |
| Topologie Sonos + `GetPositionInfo` | 250-500 ms (mesuré, L4) |
| Connexion MQTT + découverte | à mesurer |

Si le coût d'un réveil approche la durée de la tranche de sommeil, la refonte ne gagne rien :
**il faudra alors allonger l'intervalle de sondage plutôt que multiplier les réveils.**
Cette conclusion est un résultat acceptable, à condition d'être étayée par des mesures.

## Critères d'acceptation

Hors matériel :

- `pio test -e native` au vert, tests neufs compris — la sérialisation de l'état RTC et
  toute décision nouvelle vivent dans `src/core/` et se testent sans boîtier ;
- `pio run` compile.

Sur cible :

- pendant une lecture continue, le boîtier dort entre deux sondages et le morceau affiché
  reste juste ;
- un changement de morceau est visible en moins d'une tranche de sondage plus 37 s ;
- **le compteur de rafraîchissements n'augmente pas** quand le morceau ne change pas — c'est
  le garde-fou anti-régression du projet, visible dans Home Assistant
  (`sensor.epaper_sonos_rafraichissements`) ;
- les trois boutons réveillent le boîtier et leur action aboutit ;
- les entités Home Assistant ne passent pas en `unavailable` d'un cycle à l'autre ;
- consommation moyenne mesurée à la prise, comparée au ~1 W actuel. **Sans cette mesure, la
  refonte n'est pas validée** : c'est son unique raison d'être.

## Méthode

- Une livraison = un commit autonome et flashable, avec son test. Voir `tasks/todo.md`.
- Le code métier va dans `src/core/`, sans dépendance Arduino. Les couches matérielles
  restent de fines enveloppes. Un test qui exige le boîtier signale une logique mal placée.
- Commentaires et messages de commit **en français**, expliquant le *pourquoi* et non le
  *quoi*.
- **Le dépôt est public.** `src/config.h`, `mqtt-user.env`, `*.env` et `test/fixtures/raw/`
  sont git-ignorés. Vérifier `git status --short` avant chaque commit.
- Si la mesure finale montre que la refonte n'apporte rien, **le dire et ne pas fusionner**.
  Une branche abandonnée avec ses mesures vaut mieux qu'une complexité gratuite.
