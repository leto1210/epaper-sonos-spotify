# Reprendre le développement sans le matériel

Ce projet est conçu pour qu'on puisse travailler dessus **sans reTerminal branché, sans
enceintes Sonos et sans Home Assistant** — depuis VS Code, un agent, ou une machine
quelconque. Ce document dit ce qui est faisable hors ligne, comment le vérifier, et ce qui
exige réellement le matériel.

## En une commande

```bash
pio test -e native
```

C'est la boucle de travail principale. Elle compile `src/core/` et lance toutes les suites de
tests sur des **captures réelles** figées dans `test/fixtures/` : réponses SOAP d'enceintes
Sonos, sortie du template Jinja de Home Assistant. Aucun réseau, aucun périphérique.

```bash
pio run                  # vérifie que le firmware compile pour l'ESP32-S3
pio run -t upload        # nécessite le matériel
```

## La règle d'or : `src/core/` ou pas

| Répertoire | Dépend d'Arduino | Testable hors ligne |
|---|---|---|
| `src/core/` | non | **oui** — c'est là que va toute la logique |
| `src/` (racine) | oui (WiFi, SPI, I²C, MQTT) | non, compilation seulement |

Tout ce qui peut être décidé à partir de données — analyser une réponse, choisir une zone,
tramer une image, convertir une tension, composer un payload — appartient à `src/core/` et
doit arriver **accompagné de son test**. Les fichiers de la racine se contentent de faire des
entrées-sorties et d'appeler `core/`.

Cette séparation est la raison pour laquelle le parseur Sonos a pu être écrit, débogué et
validé alors que le boîtier n'était même pas alimenté.

## Ajouter une fixture sans le matériel

Les fixtures existantes suffisent pour la majorité du travail. Pour en ajouter une, il faut
un accès ponctuel au matériel réel, puis :

```bash
# 1. capturer (voir docs/sonos-api.md pour les requêtes complètes)
curl -s ... > test/fixtures/raw/mon_cas.xml

# 2. anonymiser — obligatoire, le dépôt est public
python3 tools/scrub_fixture.py test/fixtures/raw/*.xml -o test/fixtures/
```

`test/fixtures/raw/` est git-ignoré. Ne commitez jamais une capture brute : elle contient
l'identifiant de foyer Sonos, les UUID des enceintes, des IP privées et l'identifiant de
session du compte Spotify.

## État par livraison

Le suivi vit dans [`tasks/todo.md`](../tasks/todo.md). Pour chaque livraison, voici ce qui se
fait hors ligne et ce qui exige le matériel.

| Livraison | Hors ligne | Exige le matériel |
|---|---|---|
| L0 squelette, CI | tout | — |
| L1 écran | compilation | rendu visuel, mesure du temps de rafraîchissement |
| L2 Wi-Fi, NTP | compilation | connexion, joignabilité réseau |
| L3 parseur Sonos | **tout**, sur fixtures | — |
| L4 topologie, coordinateur | **tout**, sur `topology.xml` | confirmation des IP réelles |
| L5 sondage en direct | logique de décision | requêtes vers les enceintes |
| L6 mise en page | calcul du placement, découpe du texte | rendu |
| L7 anti-redraw | **tout** — c'est une machine à états pure | comptage réel |
| L8 pochettes | **tout** : décodage et tramage, avec un JPEG de test | rendu couleur |
| L9 capteurs | conversion tension → pourcentage | lecture ADC et I²C |
| L10 boutons | anti-rebond, coalescence | appuis réels |
| L11-L12 MQTT, Home Assistant | construction des payloads, analyse des commandes reçues | broker |
| L13 météo | **tout** : parseur, mise en forme, template Jinja | broker |
| L14 veille, deep sleep | logique de transition | consommation, réveil |
| L16-L18 état RTC, réveil `ext1`, MQTT | sérialisation, décodage du réveil | cycle réel |
| L19 mesure d'énergie | — | **tout** : il faut un wattmètre et la batterie retirée |
| Entrée ligne | reconnaissance de la source | rendu du disque |
| Repli ASCII | **tout** : accents, ligatures, ponctuation | — |

Le délai de grâce sur pause (`core/pause_timer`) illustre la règle : il ne voit qu'une horloge
et un état, donc ses six tests couvrent la reprise, le changement de pièce et le débordement
de `millis()` sans qu'aucune enceinte soit branchée. Seule la bascule d'écran demandait le
matériel.

`core/text_fold` en est le cas extrême : le défaut se voyait sur une photo du panneau — un
accent manquant — mais se corrige et se vérifie **entièrement** hors matériel, y compris le
cas d'une séquence UTF-8 tronquée.

À l'inverse, `core/weather_rtc` rappelle la limite de l'exercice : sa sérialisation se teste
sur le Mac, mais le défaut qu'elle corrige — un bulletin perdu à chaque réveil — n'était
visible qu'en photographiant l'écran, qui annonçait « Météo indisponible » alors que la donnée
existait.

Quand une livraison est « **tout** » hors ligne, elle doit l'être vraiment : si son test a
besoin du matériel, c'est que la logique est au mauvais endroit.

## Vérifier un template Home Assistant sans HA

Le template de `homeassistant/weather_to_mqtt.yaml` a été validé en l'exécutant dans le
moteur de Home Assistant, puis sa sortie a été figée dans `test/fixtures/weather.json`. Le
parseur embarqué est testé contre ce fichier. Si vous modifiez le template, rejouez-le dans
**Outils de développement > Modèle** et mettez la fixture à jour — c'est le contrat entre les
deux moitiés du système.

## Simuler le boîtier

Pour observer ce que HA recevra, sans flasher quoi que ce soit :

```bash
mosquitto_sub -h <broker> -t 'reterminal_sonos/#' -v
```

Et pour lui pousser une météo de test :

```bash
mosquitto_pub -h <broker> -t reterminal_sonos/weather -r -f test/fixtures/weather.json
```

## Ce qu'on ne peut pas simuler

Trois choses n'ont pas d'équivalent hors matériel, et il faut en tenir compte plutôt que
prétendre les couvrir :

- **Le rendu de l'ePaper.** Le tramage se vérifie en écrivant l'image tramée dans un PNG et
  en la regardant, mais le rendu final sur Spectra 6 diffère — les couleurs sont peu
  saturées et les aplats se comportent autrement qu'à l'écran.
- **Le temps de rafraîchissement** — 37 s mesurées, à comparer aux 25-30 s annoncées.
- **La consommation.** Toute affirmation sur l'autonomie demande une mesure — et une mesure
  prise dans les bonnes conditions : batterie branchée, le chargeur limite le courant
  d'entrée et masque entièrement l'activité du processeur. Relevé et méthode dans
  [`tasks/l19-validation.md`](../tasks/l19-validation.md), outil dans `tools/powerz_log.py`.
