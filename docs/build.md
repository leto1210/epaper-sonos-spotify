# Compilation, flash et tests

## Prérequis

[PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) :

```bash
pip install --upgrade platformio
```

Aucun paquet Arduino IDE n'est nécessaire : PlatformIO télécharge la toolchain ESP32-S3 au
premier build.

> **Plateforme** : le projet utilise le fork [pioarduino](https://github.com/pioarduino/platform-espressif32)
> de `platform-espressif32`, épinglé dans `platformio.ini`. Le paquet `espressif32` du
> registre PlatformIO est resté sur le core Arduino 2.x et ne construit pas pour
> l'ESP32-S3 avec PSRAM OPI.

## Configuration

```bash
cp config.example.h src/config.h
```

Puis renseignez Wi-Fi, MQTT et éventuellement `SONOS_SEED_IP`. `src/config.h` est
git-ignoré.

> **Ne déposez aucun identifiant ailleurs que dans `src/config.h`.** Les fichiers `*.env` sont
> ignorés eux aussi, mais un fichier de secrets posé à la racine d'un dépôt public reste à un
> `git add -A` de se retrouver publié — c'est arrivé sur ce projet. Si cela vous arrive :
> changez le mot de passe concerné, le retrait du fichier ne suffit pas.

## Commandes

```bash
pio test -e native                 # tests unitaires, sur votre machine, sans matériel
pio run                            # compilation ESP32-S3
pio run -t upload -t monitor       # flash + moniteur série (115200 bauds)
```

Les tests natifs couvrent toute la logique pure du projet (`src/core/`) : parsing des
réponses Sonos, sélection de zone, tramage des pochettes, conversion de la tension batterie.
Ils tournent en quelques secondes et sont exécutés par la CI à chaque push.

## `driver.h`

`Seeed_GFX` a besoin d'un fichier de configuration décrivant l'écran. Il est généré par le
[Seeed GFX Configuration Tool](https://seeed-studio.github.io/Seeed_GFX/) ; pour le
reTerminal E1002, le code combinaison carte/écran est **521**.

> Le `driver.h` du dépôt est généré pour l'E1002 à partir de la livraison L1. Si vous ciblez
> un autre modèle de la série E10xx, régénérez-le avec le code correspondant.

## Dépannage

| Symptôme | Cause probable |
|---|---|
| `fatal error: SPI.h: No such file` | `lib_ldf_mode = deep+` et les entrées `SPI`/`Wire` de `lib_deps` ont été retirées de `platformio.ini` |
| `TypeError: ... not 'NoneType'` au démarrage du build | La plateforme du registre PlatformIO a été utilisée au lieu du fork pioarduino |
| Rien sur le port série après le flash | `ARDUINO_USB_CDC_ON_BOOT=1` est requis ; laissez 2 s au CDC pour s'énumérer avant les premiers `Serial.printf` |
| Le boîtier n'est pas détecté par `pio run -t upload` | Maintenir le bouton BOOT à la connexion USB-C pour forcer le mode téléversement |
