# Matériel — Seeed reTerminal E1002

ESP32-S3 avec 8 Mo de PSRAM (OPI) et 32 Mo de flash, écran ePaper **E Ink Spectra 6**
7,3" de **800×480** pixels en 6 couleurs : noir, blanc, rouge, vert, bleu, jaune.

Côté PlatformIO, la carte se déclare comme `seeed_xiao_esp32s3` — le reTerminal est bâti
autour du même module.

## Brochage

| Fonction | GPIO | Notes |
|---|---|---|
| ePaper SCK | 7 | SPI |
| ePaper MOSI | 9 | |
| ePaper CS | 10 | |
| ePaper DC | 11 | |
| ePaper RESET | 12 | |
| ePaper BUSY | 13 | |
| Bouton vert | 3 | `INPUT_PULLUP`, actif à l'état bas |
| Bouton blanc droit | 4 | idem — **aussi la broche de réveil deep-sleep (ext0, niveau bas)** |
| Bouton blanc gauche | 5 | idem |
| Buzzer | 45 | PWM (`ledc`) |
| LED utilisateur | 6 | logique inversée |
| SHT4x SDA | 19 | I²C |
| SHT4x SCL | 20 | |
| Mesure batterie (ADC) | 1 | diviseur ×2 |
| Activation mesure batterie | 21 | mettre à l'état haut, attendre ~10 ms, puis lire |

## Batterie

Accu 2000 mAh. Lecture :

```cpp
digitalWrite(21, HIGH);
delay(10);                                   // sans ce délai la mesure est fausse
uint32_t mv = analogReadMilliVolts(1) * 2;   // diviseur par 2 sur la carte
```

Correspondance état de charge : **4,15 V → 100 %**, **3,27 V → 0 %**.

## Contrainte structurante : le temps de rafraîchissement

Un rafraîchissement complet prend **25 à 30 secondes** d'après Seeed — **37 s mesurées** sur
l'exemplaire de test, écran complet. Le Spectra 6 n'offre pas de
rafraîchissement partiel exploitable en couleur. Tout le firmware est construit autour de
ce fait : on ne redessine que lorsque le contenu change réellement, jamais pour animer quoi
que ce soit. Les appuis sur les boutons sont donc confirmés par le **buzzer**, seul retour
immédiat disponible.

## Typographie : n'utiliser que les FreeFonts

Les polices numérotées de `Seeed_GFX` ne sont pas des polices générales. La 6 est une police
d'horloge qui ne contient que `1234567890:-.apm`, la 7 seulement des chiffres. **Les
caractères absents sont supprimés silencieusement** : « ePaper Spotify » s'y affiche « ap p ».

Le firmware n'emploie donc que les FreeFonts d'Adafruit GFX :

```cpp
epaper.setFreeFont(&FreeSansBold24pt7b);
epaper.drawString("Titre du morceau", x, y);  // sans argument de police
```

`setFreeFont()` bascule déjà la police courante ; passer en plus une constante `GFXFF` — vue
dans les exemples de la bibliothèque — ne compile pas, elle appartient à un en-tête d'exemple
et non à `Seeed_GFX`.

## Couleur : les aplats seulement, jamais les traits fins

Les six encres du Spectra 6 ne se comportent pas comme celles d'un écran classique. Une
**surface large** — un texte de 24 pt, une pochette tramée — sort dans une couleur franche.
Un **trait de quelques pixels**, lui, est composé par le panneau à partir de ses pigments et
ressort délavé : les premiers pictogrammes météo, cernés de noir sur 4 px, apparaissaient
**bruns** sur la photo de contrôle.

D'où la règle retenue : la couleur pour les aplats, le noir et blanc pour tout ce qui se
dessine au trait. Les pictogrammes météo sont donc monochromes, avec un contour d'au moins
3 px et jamais moins de 13 % du rayon de l'icône. Le volume du nuage est suggéré par deux
barres pleines, faute d'un gris que le panneau n'a pas.

Le défaut ne se voit sur aucun aperçu : il faut photographier le panneau.

## Consommation : ce que la prise USB montre, et ce qu'elle cache

**Batterie branchée, la mesure à la prise ne veut rien dire.** Le chargeur du reTerminal
limite le courant d'entrée. Quand l'ESP32 consomme plus, le chargeur prend simplement moins
pour la batterie, et le total ne bouge pas. Mesuré au POWER-Z KM003C, batterie à 100 % :

| | puissance moyenne |
|---|---|
| au repos | 2,211 W |
| **pendant un rafraîchissement de 37 s** | 2,220 W |
| après | 2,207 W |

Un rafraîchissement complet du panneau est **invisible**, et le courant reste à 0,4202 A d'un
échantillon à l'autre — la signature d'une régulation, pas d'une consommation.

**Batterie retirée, les chiffres deviennent vrais :**

| | puissance |
|---|---|
| au repos, Wi-Fi associé | **0,31 W** (0,23 à 0,52) |
| **en lecture continue** | **0,315 W** (0,24 à 0,50) |
| pendant un rafraîchissement de 37 s | 0,365 W |
| cycle de sommeil, rien ne joue | **0,050 W** |

Soit **environ 60 mA sous 5,2 V**. Les 2,2 W précédents étaient donc à plus de 85 % de la
charge de la batterie. Et le rafraîchissement de l'ePaper, qu'on soupçonnait d'être coûteux,
n'ajoute qu'un dixième de watt : **c'est la radio Wi-Fi qui domine**, pas le panneau.

Conséquence pour toute mesure future : **retirer la batterie, ou attendre une charge
réellement terminée.** Un « avant/après » pris batterie branchée donnerait deux fois le même
chiffre et la conclusion fausse qu'une optimisation n'apporte rien.

Le firmware ne dort que lorsque **rien n'a joué pendant dix minutes**. Tant que la musique
tourne, le Wi-Fi reste allumé en permanence : c'est le régime nominal d'un afficheur branché,
et le poste de consommation dominant. Sur la batterie de 2000 mAh (~7,4 Wh), 0,31 W donnent
un ordre de grandeur de **vingt-trois heures** — mieux que les sept heures estimées tant que
la mesure était faussée par la charge, mais loin d'une autonomie de plusieurs jours. Quand
rien ne joue, le cycle de sommeil porte l'autonomie à environ **six jours**.

Mesuré sur 8,5 minutes de lecture continue : **aucun** des 439 échantillons ne descend sous
0,06 W. Le boîtier ne s'endort jamais tant que la musique joue — la prémisse du brief
énergie, désormais vérifiée par la mesure et non par la lecture du code.

Deux pistes ont été écartées après essai, et une reste ouverte :

- **Abaisser la fréquence du processeur à 80 MHz** : abandonné. Le rafraîchissement passait
  de 37,3 s à 41,0 s. Or c'est le panneau qui consomme pendant un redessin ; il restait
  alimenté 3,7 s de plus, et l'économie sur le processeur se payait sur le poste le plus
  coûteux.
- **Mettre le panneau en veille** : déjà fait par `Seeed_GFX`, qui appelle `EPD_SLEEP()` à la
  fin de chaque mise à jour.
- **Dormir aussi entre deux sondages pendant la lecture** : c'est le seul vrai levier. L'image
  d'un ePaper est bistable, elle survit au sommeil. Il faudrait accepter un réveil Wi-Fi par
  sondage (~1,7 s) et faire réveiller les trois boutons par `ext1` au lieu du seul GPIO4.
  Non implémenté.

Attention à la méthode de mesure : ouvrir le port série **réinitialise l'ESP32** par DTR/RTS.
Un boîtier observé à la console est donc un boîtier qui vient de redémarrer, et qui réclame
dix nouvelles minutes d'inactivité avant de se rendormir.

## Sources

- [Wiki Seeed — Getting started reTerminal E1002](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1002/)
- [Wiki Seeed — Arduino, ePaper](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino/)
- [Wiki Seeed — périphériques embarqués et GPIO](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_advanced/)
