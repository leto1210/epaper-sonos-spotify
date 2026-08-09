# Intégration Home Assistant

> Implémentée à la livraison L11. Ce document décrit le contrat visé ; les captures d'écran
> arrivent avec le code.

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
