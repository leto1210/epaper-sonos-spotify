# Intégration Home Assistant

> Les entités publiées par le boîtier arrivent à la livraison L11. La météo, décrite en fin
> de page, est déjà spécifiée et testée.

Le firmware ne dépend pas de Home Assistant pour fonctionner : laissez `MQTT_HOST` vide dans
`src/config.h` pour désactiver complètement cette partie.

## Principe

Le boîtier publie sa propre configuration via **MQTT Discovery** sur un broker Mosquitto.
Aucun YAML à écrire côté Home Assistant : l'appareil apparaît tout seul.

- Topics de configuration : `homeassistant/<composant>/reterminal_sonos/<objet>/config`
  (retain)
- Disponibilité : LWT retain sur `reterminal_sonos/status` (`online` / `offline`), ce qui
  fait passer les entités en `unavailable` si le boîtier se coupe
- Bloc `device` commun à toutes les entités, pour qu'elles soient regroupées sous un seul
  appareil « ePaper Sonos »

## Entités exposées

| Entité | Composant | Détail |
|---|---|---|
| Batterie | `sensor` | `device_class: battery`, en %, tension brute en attribut |
| En charge | `binary_sensor` | `device_class: battery_charging` |
| Température | `sensor` | SHT4x, °C |
| Humidité | `sensor` | SHT4x, % |
| Signal Wi-Fi | `sensor` | RSSI, diagnostic |
| Uptime | `sensor` | diagnostic |
| Dernier rafraîchissement | `sensor` | `device_class: timestamp`, diagnostic |
| Nombre de rafraîchissements | `sensor` | diagnostic — sert de garde-fou : si ce compteur grimpe alors que la musique ne change pas, il y a une régression |
| Morceau en cours | `sensor` | valeur = titre, attributs : artiste, album, zone, URL de pochette |
| Rafraîchir l'écran | `button` | déclenche un redraw immédiat |
| Zone suivie | `select` | `auto` ou une pièce précise |

## Fréquence de publication

À chaque changement de morceau, et au minimum toutes les 5 minutes. C'est volontairement
**découplé de l'écran** : l'ePaper ne se redessine que lorsque le morceau change, mais les
mesures de batterie et de température remontent en continu.

## Météo : le sens inverse

Quand rien ne joue, l'écran affiche la météo. Elle vient de Home Assistant, mais **le boîtier
n'appelle jamais Home Assistant** : il s'abonne à un sujet MQTT. Pas de jeton d'API à stocker
dans le firmware, pas de sondage, et l'écran est à jour dès le démarrage grâce au drapeau
`retain`.

Installez [`homeassistant/weather_to_mqtt.yaml`](../homeassistant/weather_to_mqtt.yaml) comme
automatisation, en adaptant l'entité météo. Elle publie toutes les 30 minutes, et au
démarrage de Home Assistant, sur `reterminal_sonos/weather` :

```json
{
  "ts": 1786284627,
  "now": {"cond": "sunny", "temp": 32.6, "hum": 23, "wind": 10, "uv": 5.6},
  "hourly": [
    {"h": "16", "c": "partlycloudy", "t": 33.3, "p": 0},
    {"h": "18", "c": "sunny", "t": 33.6, "p": 0}
  ]
}
```

Six créneaux, une échéance sur deux : douze heures de visibilité pour environ 400 octets.

Deux détails qui ont l'air anodins et ne le sont pas :

- `ts` n'est pas décoratif. Le sujet étant **retenu**, un broker resservirait indéfiniment la
  dernière météo publiée si l'automatisation s'arrêtait. Le firmware considère un rapport de
  plus de deux heures comme périmé et l'affiche comme tel, plutôt que de présenter des
  valeurs anciennes avec l'aplomb des valeurs fraîches.
- `cond` reprend le vocabulaire de Home Assistant (`sunny`, `clear-night`, `partlycloudy`,
  `lightning-rainy`…), traduit côté firmware en pictogrammes. Une valeur inconnue s'affiche
  neutre plutôt que de faire échouer la lecture du payload.

Le contrat entre les deux moitiés est figé dans `test/fixtures/weather.json`, qui est la
sortie réelle du template évaluée par Home Assistant, et sur lequel tourne le parseur
embarqué. Si vous modifiez le template, mettez la fixture à jour — voir
[docs/development.md](development.md).
