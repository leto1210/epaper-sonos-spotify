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

Chaque enceinte répond avec un en-tête `LOCATION` contenant son IP. Si le multicast ne passe
pas (VLAN, isolation client Wi-Fi), renseignez `SONOS_SEED_IP` : une seule enceinte suffit,
la topologie complète s'obtient ensuite par SOAP.

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

## Pochette

`upnp:albumArtURI` est une URL **relative**, à préfixer par l'adresse du coordinateur :

```
http://SONOS_IP:1400/getaa?s=1&u=<url-encodée-du-morceau>
```

L'enceinte agit en proxy : elle récupère la pochette chez Spotify et la sert en JPEG sur le
LAN. C'est ce qui permet au boîtier de n'avoir aucun accès Internet.

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
