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

## L'utilisateur voit ce que je ne peux pas voir

Trois défauts d'affichage — bandeau désaligné, tiers d'écran vide, faux zéros des capteurs —
n'ont été trouvés que sur les photos envoyées par Thomas. Le journal série les ignorait
totalement.

**La règle.** Pour tout ce qui est visuel, demander une photo plutôt que de conclure du succès
d'un rendu au fait qu'il n'a pas planté. Et ne jamais afficher une valeur par défaut là où une
mesure est attendue : `0.0 C` se lit comme une mesure, pas comme une absence de mesure.
