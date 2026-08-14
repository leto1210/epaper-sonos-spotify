# L19 — Validation énergie sur matériel

**État** : **mesurée le 14 août 2026.** Résultats et verdict en fin de page.

## Objectif

Mesurer et comparer la consommation réelle avant/après refonte énergie. C'est la validation
cœur du brief : sans cette mesure, la refonte n'a aucune raison d'être, et ne doit pas être
fusionnée dans `main`.

L16 à L18 sont sur `main` ; la branche `energie/sommeil-entre-sondages` a été fusionnée
puis supprimée.

## Prérequis matériel

- **ESP32-S3 flashé** avec L19 (compile en natif ; besoin Python ≥3.10 pour `pio run`)
- **Multimètre ou pince ampèremétrique** à la prise 5V
- **Home Assistant** en fonctionnement avec Mosquitto MQTT
- **Sonos** avec au minimum une zone jouant Spotify
- **Batterie retirée** — voir la checklist, c'est la condition sans laquelle la mesure ment
- **Durée** : 15 min + 10 min par mesure = ~45 min total

## Plan de mesure

### Mesure 1 : Avant (L14, état actuel)

**But** : établir la baseline ~1 W mesuré dans `docs/hardware.md`.

1. Flasher le boîtier en **L14** (`git checkout afa20cf`)
2. Brancher le multimètre/wattmètre à la prise 5V
3. Lancer une **lecture Sonos continue** sur une zone (p. ex., liste de lecture)
4. **Laisser tourner 10 min minimum** — le boîtier doit rester éveillé (musicque ne s'arrête pas)
5. Noter la **consommation moyenne** en watts (U × I, ou affichage direct)
6. Documenter :
   - Tension mesurée (V)
   - Intensité moyenne (A)
   - Puissance moyenne (W)
   - Variation observée (min/max)
   - Conditions : zone affichée, débit réseau, proximité AP

**Résultat attendu** : ~1 W (200 mA à 5V), ± 0,2 W.

### Mesure 2 : Après (L18, refonte énergie)

**But** : vérifier que dormir entre sondages réduit la consommation.

1. Flasher le boîtier en **L18** (`git checkout 1e64db4`)
2. Même configuration matériel et réseau
3. Lancer une **lecture Sonos continue** sur la même zone
4. **Observer 10 min minimum** — le boîtier doit entrer en deep sleep entre deux sondages
5. Vérifier sur la console série (si visible) ou dans Home Assistant :
   - Cycle observable : `[veille] deep sleep pour 60000 ms` / réveil
   - Compteur de rafraîchissements **stagne** (ne monte que si le morceau change)
   - Entités Home Assistant restent `available` (pas de clignotement)
6. Noter la **consommation moyenne** (même protocole que Mesure 1)
7. Documenter aussi les **coûts de réveil** observés :
   - Durée Wi-Fi ← association
   - Durée Sonos ← sondage GetPositionInfo
   - Durée MQTT ← reconnexion + rediscovery
   - Total par cycle (en ms)

**Résultat attendu** : < 0,3 W en moyenne (économie > 70 %).

Hypothèse de travail pour le budget :
- Éveillé 3–5 s par min (réveil, Wi-Fi, Sonos, MQTT, redraw si change)
- Dormant 55–57 s par min (deep sleep ~10 µA)
- Consommation moyenne = 0,05 × 1 W + 0,95 × 0,0001 W ≈ **0,05 W** (cas optimiste)

En réalité, wifi/Sonos/MQTT coûtent de l'énergie même au repos (radio associée),
donc chiffre plus proche de **0,15–0,2 W** en conditions réelles.

## Checklist de validation sur cible

### Avant flashage

- [ ] Batterie chargée (mesurer voltage initial si possible)
- [ ] Serial USB branché ET **détaché juste avant mesure** (DTR/RTS réinitialise la cible)
- [ ] **Batterie retirée du boîtier.** Non négociable : batterie branchée, le chargeur limite
      le courant d'entrée et absorbe toute variation. Mesuré, batterie à 100 % : 2,211 W au
      repos contre 2,220 W pendant un rafraîchissement complet de 37 s — l'écart est dans le
      bruit, et le courant tient à 0,4202 A d'un échantillon à l'autre. Batterie retirée, le
      même boîtier consomme **0,31 W**. Un « avant/après » pris batterie branchée donnerait
      deux fois 2,2 W et la conclusion fausse que la refonte n'apporte rien.
- [ ] Multimètre prêt, calibré en DC 5V/2A — ou un POWER-Z KM003C, lisible en série depuis le
      Mac (voir `tools/`), ce qui permet d'enregistrer une courbe plutôt que de lire un
      afficheur. Fermer l'application ChargerLab d'abord : elle tient le port en exclusivité.
- [ ] Sonos en réseau, une zone accessible
- [ ] **Lecture en cours au Sonos Séjour**, et non sur une zone en pause. C'est le cas
      nominal — celui que la mesure doit refléter — et c'est aussi le pire pour la
      consommation : tant que la musique tourne, le boîtier ne s'endort pas au titre de
      l'inactivité. Mesurer sur une maison silencieuse donnerait un chiffre flatteur et faux.
      Attention à la maison réelle : le Beam annonce `PLAYING` dès qu'il diffuse le son de la
      télévision, et une enceinte laissée en pause reste candidate. Vérifier sur la console
      que la zone retenue est bien `Sonos Séjour` avant de lancer le chronomètre.
- [ ] HA + MQTT en fonctionnement
- [ ] Topic `reterminal_sonos/status` dans HA, capteur rafraîchissements visible

### Mesure 1 : Baseline (L14)

- [ ] `git checkout afa20cf` (dernier commit main avant L16)
- [ ] Compiler : `pio run` (Python 3.10+)
- [ ] Flasher : `pio run -t upload --upload-port /dev/cu.usbserial-XXX`
- [ ] Boot observer sur console : `[boot] ePaper Spotify`, Wi-Fi OK, HA OK
- [ ] Déconnecter USB série
- [ ] Brancher multimètre
- [ ] Lancer lecture Sonos **continue**
- [ ] Attendre 2–3 min de stabilisation
- [ ] Mesurer 10 min, noter moyenne + min/max

**Résultat** : `U_baseline_V`, `I_baseline_A`, `W_baseline = U × I`

### Mesure 2 : Refonte (L18)

- [ ] `git checkout 1e64db4` (HEAD = L18)
- [ ] Compiler : `pio run`
- [ ] Flasher
- [ ] Boot observer, vérifier Home Assistant reconnection
- [ ] Optionnel : connecter USB série pour observer `[veille] deep sleep`
- [ ] **Vérifier que le boîtier s'endort** :
  - Console : logs cycles toutes les ~60 s
  - HA : morceau stable, compteur de refresh ne monte pas
- [ ] Déconnecter USB (critique)
- [ ] Brancher multimètre
- [ ] Lancer même lecture
- [ ] Stabilisation 3–5 min (plus long, reconnexion MQTT)
- [ ] Mesurer 10 min
- [ ] Observer oscillations rapides ? (signe d'instabilité MQTT ou réveil intempestif)

**Résultat** : `U_refonte_V`, `I_refonte_A`, `W_refonte = U × I`

### Coûts de réveil (optionnel mais utile)

Si console série accessible sans réinitialiser (impossible sur CH340, donc skip) :

- [ ] Mesurer timestamp "`reveil`" → "`deep sleep`" sur 3–5 cycles
- [ ] Enregistrer logs pour traçabilité

## Critères d'acceptation

### Imperatives

- [ ] **Consommation moyenne mesurée en L18 < L14** (au minimum 20 % d'économie)
- [ ] **Compteur de rafraîchissements n'augmente pas** pendant lecture si morceau ne change pas
- [ ] **Entités Home Assistant restent disponibles** d'un réveil à l'autre (pas d'oscillation)
- [ ] **Les 3 boutons réveillent** et exécutent l'action (vérifier dans HA ou sur écran)

### Désiré (pour confiance)

- [ ] Consommation > 50 % d'économie (target < 0,5 W)
- [ ] Cycle observable : réveil visible sur HA toutes les 60 s
- [ ] Temps de réveil < 5 s (visible sur monitorage MQTT timestamp)
- [ ] Batterie dure > 15 h en lecture continue (vs ~7 h actuellement)

## Décision et suite

### Si critères imperatives ✓

```
Gain mesurable documenté.
→ Fusionner L16 + L17 + L18 dans `main` via PR.
→ Clore branche `energie/sommeil-entre-sondages`.
→ Fin du projet refonte énergie.
```

### Si critères imperatives ✗

```
Pas de gain ou régression.
→ NE PAS fusionner.
→ Conserver branche avec mesures et conclusion.
→ Documenter les leçons apprises.
→ Considérer alternative : intervalle de sondage plus long au lieu de sleep court.
```

## Mesures et résultats — 14 août 2026

Relevé au POWER-Z KM003C, **batterie retirée** (voir les prérequis : batterie branchée, la
mesure ne veut rien dire), boîtier alimenté par le Mac, entrée ligne au Séjour donc rien à
afficher — le cas où la veille s'applique.

### Consommation mesurée

| Régime | Puissance | Part du temps |
|---|---|---|
| Plancher de deep sleep | **0,0284 W** | 91 % |
| Pointes d'éveil (Wi-Fi, sondage) | 0,287 W | 8 % |
| **Moyenne du cycle, rien ne joue** | **0,0501 W** | — |
| **Lecture continue** (Sonos Bureau, 8,5 min) | **0,3149 W** | — |
| Rafraîchissement de l'écran (37 s) | 0,365 W | — |

En lecture continue, **aucun** des 439 échantillons ne descend sous 0,06 W : le boîtier ne
s'endort jamais tant que la musique joue. C'est la prémisse du brief, désormais vérifiée par
la mesure et non plus par la lecture du code.

Le rafraîchissement n'ajoute que 0,05 W au-dessus du sondage seul (0,313 W) — confirmation
que le panneau n'est pas le poste coûteux.

Huit réveils en neuf minutes, soit un toutes les 67 s : la tranche de 60 s plus environ sept
secondes d'éveil. Le cycle fonctionne exactement comme prévu.

### Autonomie sur la batterie de 2000 mAh (~7,4 Wh)

| Situation | Autonomie |
|---|---|
| Lecture continue (jamais de sommeil) | **23 h** |
| Rien ne joue, cycle de sommeil | **148 h**, soit six jours |

### Verdict

**Le cycle de sommeil divise la consommation par 6,2** quand rien ne joue. À conserver.

Mais il faut être exact sur ce que L16-L18 ont apporté. Le sommeil après dix minutes
d'inactivité existait déjà en L14. Ce que ces livraisons ont ajouté, c'est **de le rendre
utilisable** : sans l'état conservé en mémoire RTC, chaque réveil redessinait l'écran pendant
37 s ; sans `resumeAfterWake`, le boîtier restait éveillé dix minutes pour une minute de
sommeil. Le gain mesuré ici est donc bien le leur, mais par correction de défauts, pas par
une mécanique nouvelle.

**L'objectif affiché du brief — dormir aussi entre deux sondages pendant la lecture — n'est
pas implémenté.** Tant que la musique joue, le boîtier reste à 0,31 W.

### Ce que gagnerait la suite, et ce qu'elle coûterait

En extrapolant depuis les sept secondes d'éveil mesurées :

| Intervalle de sondage pendant la lecture | Moyenne estimée | Autonomie | Retard sur un changement de morceau |
|---|---|---|---|
| aucun sommeil (aujourd'hui) | 0,31 W | 24 h | immédiat, puis 37 s de rendu |
| 20 s | ~0,095 W | ~78 h | jusqu'à 20 s, puis 37 s |
| 60 s | ~0,055 W | ~135 h | jusqu'à 60 s, puis 37 s |

Le sondage de 20 s garde une réactivité honnête pour un écran qui met déjà 37 s à se
redessiner, et gagne un facteur trois. Au-delà, on gagne peu et on attend beaucoup.

### La limite qu'aucun firmware ne franchira

Le plancher mesuré est de **0,0284 W, soit 5,4 mA sous 5,24 V** — très au-dessus des quelques
microampères d'un ESP32-S3 en deep sleep. Ce n'est pas le processeur : c'est la carte, avec
son pont série CH340, ses régulateurs et son circuit de charge. Aucune optimisation du
firmware ne descendra sous ce plancher tant que le boîtier est alimenté par l'USB.

C'est aussi ce qui borne le bénéfice de toute suite : dormir plus longtemps rapproche de
0,028 W, jamais de zéro.
