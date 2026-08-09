# L'API UPnP locale de Sonos

Chaque enceinte Sonos expose un serveur HTTP sur le **port 1400**. Tout ce que fait ce
projet passe par là : aucune API cloud, aucun compte développeur, aucune authentification.

Toutes les requêtes ci-dessous sont reproductibles au `curl` — c'est ainsi que sont
capturées les fixtures des tests unitaires. Remplacez `SONOS_IP` par l'IP d'une enceinte.

## Découverte

SSDP M-SEARCH en multicast sur `239.255.255.250:1900` :

```
M-SEARCH * HTTP/1.1
HOST: 239.255.255.250:1900
MAN: "ssdp:discover"
MX: 1
ST: urn:schemas-upnp-org:device:ZonePlayer:1
```

Chaque enceinte répond avec un en-tête `LOCATION` contenant son IP.

> **Le multicast ne franchit pas un routeur.** Si le boîtier est sur un SSID « objets
> connectés » placé sur un VLAN distinct de celui des enceintes — configuration courante, et
> celle du montage de référence — la découverte SSDP ne renverra jamais rien. Renseignez
> alors `SONOS_SEED_IP` : **une seule** enceinte suffit, la topologie complète (toutes les
> zones, leurs IP, leurs coordinateurs) se déduit ensuite par SOAP depuis celle-ci.
>
> Dans cette configuration, le seul flux à autoriser est **TCP 1400** du boîtier vers le
> sous-réseau des enceintes — il porte à la fois l'API et les pochettes.

## Topologie et coordinateur de groupe

**Le point crucial de toute l'intégration.** Quand des enceintes sont groupées, seule celle
qui est **coordinatrice** connaît le morceau en cours ; interroger une esclave renvoie
`NOT_IMPLEMENTED` pour les métadonnées.

```bash
curl -s -X POST \
  -H 'Content-Type: text/xml; charset="utf-8"' \
  -H 'SOAPAction: "urn:schemas-upnp-org:service:ZoneGroupTopology:1#GetZoneGroupState"' \
  -d '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:GetZoneGroupState xmlns:u="urn:schemas-upnp-org:service:ZoneGroupTopology:1"></u:GetZoneGroupState></s:Body></s:Envelope>' \
  http://SONOS_IP:1400/ZoneGroupTopology/Control
```

La réponse contient, pour chaque `<ZoneGroup Coordinator="UUID">`, la liste des
`<ZoneGroupMember UUID=... ZoneName=... Location="http://IP:1400/xml/device_description.xml">`.
On y lit à la fois le nom de la pièce et l'IP à interroger.

## Morceau en cours

```bash
curl -s -X POST \
  -H 'Content-Type: text/xml; charset="utf-8"' \
  -H 'SOAPAction: "urn:schemas-upnp-org:service:AVTransport:1#GetPositionInfo"' \
  -d '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:GetPositionInfo xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"><InstanceID>0</InstanceID></u:GetPositionInfo></s:Body></s:Envelope>' \
  http://SONOS_IP:1400/MediaRenderer/AVTransport/Control
```

Champs utiles : `TrackDuration`, `RelTime` (position), `TrackURI` et surtout
`TrackMetaData` — du **DIDL-Lite échappé** qu'il faut dé-échapper avant d'y lire
`dc:title` (titre), `dc:creator` (artiste), `upnp:album` et `upnp:albumArtURI`.

État de lecture, avec `GetTransportInfo` sur le même endpoint : `PLAYING`,
`PAUSED_PLAYBACK`, `STOPPED`, `TRANSITIONING`.

## Spotify Connect : le cas qui surprend

Quand la lecture est lancée depuis l'application Spotify (Spotify Connect), l'URI du morceau
prend la forme `x-sonos-vli:RINCON_...:2,spotify:<session>` — *vli* pour « virtual line-in ».
Sonos traite alors Spotify comme une entrée ligne.

Conséquence vérifiée sur du matériel réel :

| État de l'enceinte | `TrackMetaData` |
|---|---|
| `PLAYING` | DIDL-Lite complet : titre, artiste, album, pochette |
| `PAUSED_PLAYBACK` / `STOPPED` | `NOT_IMPLEMENTED` |

`GetMediaInfo` n'aide pas : il ne décrit que la source (`<dc:title>Spotify</dc:title>`,
classe `audioItem.linein`). L'abonnement aux événements GENA ne fait pas mieux à l'arrêt.

Ce n'est donc pas un blocage, mais cela impose une règle : **« l'enceinte joue » et « on
connaît le morceau » sont deux choses distinctes**. Le firmware conserve la dernière fiche
connue plutôt que d'effacer l'écran dès la mise en pause.

## Pochette

`upnp:albumArtURI` n'est pas toujours relatif : avec Spotify il s'agit d'une URL **absolue
en HTTPS** vers le CDN (`https://i.scdn.co/image/...`), inexploitable directement depuis
l'ESP32 — il faudrait TLS et un accès Internet.

La solution est l'endpoint proxy de l'enceinte, qui sert la même image en **HTTP simple sur
le LAN**, à partir de l'URI du morceau encodée pour-cent :

```
http://SONOS_IP:1400/getaa?s=1&u=<TrackURI-encodée>
```

Mesuré sur une piste Spotify : JPEG 640×640, environ 200 Ko. C'est ce qui permet au boîtier
de fonctionner sans aucun accès Internet.

## Contrôle de la lecture

Mêmes endpoint et service que ci-dessus, avec `InstanceID=0` :

| Action | SOAPAction | Paramètres |
|---|---|---|
| `Next` | `...AVTransport:1#Next` | `InstanceID` |
| `Previous` | `...AVTransport:1#Previous` | `InstanceID` |
| `Play` | `...AVTransport:1#Play` | `InstanceID`, `Speed=1` |
| `Pause` | `...AVTransport:1#Pause` | `InstanceID` |

À envoyer **au coordinateur** du groupe, comme les requêtes de lecture d'état.

## Confidentialité des captures

Les réponses contiennent un Household ID, les UUID des enceintes, des IP privées et des
identifiants de compte Spotify. Voir [CONTRIBUTING.md](../CONTRIBUTING.md) avant de commiter
la moindre fixture.
