# L19 — Validation énergie sur matériel

**État** : Préparée hors ligne, en attente d'accès au boîtier branché.

## Objectif

Mesurer et comparer la consommation réelle avant/après refonte énergie. C'est la validation
cœur du brief : sans cette mesure, la refonte n'a aucune raison d'être, et ne doit pas être
fusionnée dans `main`.

Branche : `energie/sommeil-entre-sondages`. Code de L16 à L18 : `pio test -e native` ✓.

## Prérequis matériel

- **ESP32-S3 flashé** avec L19 (compile en natif ; besoin Python ≥3.10 pour `pio run`)
- **Multimètre ou pince ampèremétrique** à la prise 5V
- **Home Assistant** en fonctionnement avec Mosquitto MQTT
- **Sonos** avec au minimum une zone jouant Spotify
- **Batterie chargée** du boîtier (2000 mAh, 7,4 Wh)
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

## Mesures et résultats

À remplir **sur le terrain** avec les valeurs réelles :

### Baseline (L14)

| Grandeur | Valeur | Unité | Commentaires |
|---|---|---|---|
| Tension moyenne | — | V | |
| Intensité moyenne | — | A | |
| Puissance moyenne | — | W | |
| Min / Max | — | W | |
| Durée mesure | — | min | |
| Zone affichée | — | — | (Salon, Cuisine, …) |
| Morceau joué | — | — | (titre, artiste) |
| Statut HA | — | — | (available / unavailable) |

### Refonte (L18)

| Grandeur | Valeur | Unité | Commentaires |
|---|---|---|---|
| Tension moyenne | — | V | |
| Intensité moyenne | — | A | |
| Puissance moyenne | — | W | |
| Min / Max | — | W | |
| Durée mesure | — | min | |
| Zone affichée | — | — | (même que L14) |
| Morceau joué | — | — | (même que L14) |
| Statut HA | — | — | (doit rester available) |
| Compteur refresh | — | — | (doit stagner) |
| Cycles visibles | — | (Y/N) | (s'endort et se réveille ?) |

### Analyse

| Metrique | Formule | Valeur | Verdict |
|---|---|---|---|
| Économie (%) | `(1 - W_refonte/W_baseline) × 100` | — % | (>20% OK ?) |
| Ratio | `W_baseline / W_refonte` | — × | (>1.5 OK ?) |
| Durée batterie avant | `7.4 Wh / W_baseline` | — h | (ref. ~7 h) |
| Durée batterie après | `7.4 Wh / W_refonte` | — h | (target > 15 h) |

## Conclusion

À remplir après mesure :

---

**Date** : \_\_\_\_\_\_\_\_\_\_\_\_\_

**Observateur** : \_\_\_\_\_\_\_\_\_\_\_\_\_

**Verdict** : ☐ FUSIONNER ☐ ABANDONNER

**Justification** :

(Décrire brièvement : gain réel vs objectif, pièges rencontrés, recommandations.)

---

**Pièces jointes** : logs console, captures HA, photos multimètre (optionnel).
