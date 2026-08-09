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

## Sources

- [Wiki Seeed — Getting started reTerminal E1002](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1002/)
- [Wiki Seeed — Arduino, ePaper](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino/)
- [Wiki Seeed — périphériques embarqués et GPIO](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_advanced/)
