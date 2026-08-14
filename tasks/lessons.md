# Leçons

Erreurs commises sur ce projet et règles que j'en tire. Une entrée = une correction reçue.

## Vérifier ce qu'un commit emporte, avant de committer

**Ce qui s'est passé.** Thomas a déposé `mqtt-user.env`, contenant les identifiants Mosquitto,
à la racine du projet. Mon `git add -A` de la livraison L6/L7 l'a ramassé, et le commit est
parti sur un dépôt **public**. Les identifiants sont restés exposés une trentaine de minutes,
jusqu'à ce que je m'en aperçoive en cherchant tout autre chose.

**Pourquoi c'est arrivé.** J'utilisais `git add -A` par réflexe, sans jamais regarder la liste
des fichiers ajoutés. La commande fait exactement ce qu'on lui demande — c'est la vérification
qui manquait, pas l'outil.

**La règle.** Avant tout commit, lire la liste des fichiers mis en index, et se poser la
question pour chacun : *est-ce que ce fichier a sa place dans un dépôt public ?*

```bash
git add -A && git status --short     # puis LIRE la sortie avant de committer
```

Vigilance particulière sur : `*.env`, `*.key`, `*.pem`, `config.*`, `secrets.*`, tout fichier
apparu à la racine sans que je l'y aie mis, et tout fichier apporté par l'utilisateur.

**Ce que ça coûte quand on se trompe.** Retirer le fichier de `HEAD` ne suffit pas : il reste
dans l'historique et dans les caches de la forge. Seul le **changement du secret** répare
vraiment. Autrement dit, l'erreur n'est pas rattrapable par un commit — elle impose une action
manuelle à quelqu'un d'autre.

## Un test codé en dur peut ne tester que ma supposition

**Ce qui s'est passé.** Pour la frontière entre les deux dispositions d'écran, j'avais figé des
phrases d'exemple dans le test. Je me suis trompé **deux fois de suite** sur la longueur de
bascule : le test échouait, et à chaque fois c'était mon attente qui était fausse, pas le code.

**La règle.** Quand je ne connais pas la valeur attendue, le test doit la **calculer** et
vérifier qu'elle tombe dans une plage défendable, au lieu de figer un exemple deviné.

## Des tests écrits en même temps que le code partagent ses angles morts

**Ce qui s'est passé, deux fois.**

1. Ma politique de choix de zone traitait « en pause » et « en lecture » à égalité. Mes tests
   validaient ce comportement — ils reproduisaient la confusion du code. C'est le matériel qui
   a révélé le problème : une pièce en pause masquait celle où la musique tournait.
2. Une enceinte peut annoncer `PLAYING` sans rien savoir du morceau — barre de son sur l'entrée
   TV, ou état résiduel après un basculement Spotify Connect. Aucun test ne l'envisageait,
   parce que le code ne l'envisageait pas non plus.

**La règle.** Les tests écrits en même temps que le code ne prouvent que sa cohérence interne.
Pour les hypothèses sur le monde extérieur — « une enceinte qui joue sait ce qu'elle joue » —
seule la confrontation au réel tranche. En tirer un test **après** l'observation, en nommant le
cas réel dans le commentaire.

## Un correctif révèle un motif : chercher ses frères

**Ce qui s'est passé.** Thomas a signalé « l'écran reste figé sur la pause ». Le compte à
rebours de grâce conservait un `millis()` **absolu** en mémoire RTC ; le deep sleep repasse par
`setup()`, où `millis()` repart de zéro, et la date relue dans cette époque neuve donnait un
écart négatif. Le délai ne pouvait plus échoir.

**Pourquoi c'est arrivé.** J'avais déjà rencontré ce motif, et je ne l'ai pas reconnu.
`SleepManager` persiste lui aussi un `millis()` en RTC — il n'échappe au défaut que parce que
`resumeAfterWake()`, écrit pour une raison sans rapport, écrase la valeur restaurée avant
qu'elle ne serve. J'avais réparé un symptôme sans nommer la classe de défauts dont il
relevait, si bien que le second exemplaire est resté en place, à quelques lignes du premier.

**La règle.** Quand un correctif met au jour un motif — ici « une date `millis()` traverse le
deep sleep » —, le nommer, puis **chercher ses autres occurrences** avant de refermer. Un
`grep` sur ce qui entre en mémoire RTC coûtait quelques secondes.

Corollaire pour ce genre de compteur : ce qui traverse un sommeil doit être une **durée**, et
le réveil doit **créditer le temps dormi**. Omettre la seconde moitié ne casse rien de visible
tout de suite — le délai s'écoule simplement des dizaines de fois trop lentement, ce qui se
diagnostique bien plus mal qu'une panne franche.

## Ma propre documentation est une source, pas un décor

**Ce qui s'est passé.** J'ai conçu un bouton Home Assistant pour ouvrir une fenêtre de mise à
jour. Le premier appui réel n'est jamais parvenu au boîtier : il dormait. Or j'avais écrit
moi-même, une heure plus tôt, que *« les commandes venues de Home Assistant sont perdues si
elles arrivent pendant un sommeil »* — je l'avais même **vérifiée expérimentalement** avant de
l'écrire.

**Pourquoi c'est arrivé.** J'ai traité cette phrase comme une conclusion classée, propre à la
fonctionnalité qui l'avait produite, au lieu d'une contrainte du système. Elle disqualifiait
pourtant d'emblée toute commande ponctuelle venue de Home Assistant.

**La règle.** Avant de concevoir quelque chose qui repose sur un mécanisme existant, relire ce
que la documentation du dépôt dit **de ce mécanisme** — en particulier les limites que j'y ai
consignées. Une contrainte vérifiée sur le matériel ne cesse pas de s'appliquer parce qu'on
change de fonctionnalité.

Corollaire, appris dans la foulée : quand un boîtier s'abonne au sujet sur lequel il publie,
il se parle à lui-même. L'effacement d'une commande retenue est arrivé en retour comme une
nouvelle commande, et le buzzer a bipé en continu. Toute publication sur un sujet auquel on
est abonné doit être pensée comme une entrée à valider, pas comme une écriture.

## L'utilisateur voit ce que je ne peux pas voir

Trois défauts d'affichage — bandeau désaligné, tiers d'écran vide, faux zéros des capteurs —
n'ont été trouvés que sur les photos envoyées par Thomas. Le journal série les ignorait
totalement.

**La règle.** Pour tout ce qui est visuel, demander une photo plutôt que de conclure du succès
d'un rendu au fait qu'il n'a pas planté. Et ne jamais afficher une valeur par défaut là où une
mesure est attendue : `0.0 C` se lit comme une mesure, pas comme une absence de mesure.
