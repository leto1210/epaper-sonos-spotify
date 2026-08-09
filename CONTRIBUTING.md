# Contribuer

## Avant toute PR

```bash
pio test -e native   # doit passer
pio run              # doit compiler
```

La CI exécute exactement ces deux commandes.

## Organisation du code

- `src/core/` — **logique pure, sans dépendance Arduino**. Parsing des réponses Sonos,
  sélection de zone, tramage, conversions. C'est le seul code couvert par les tests natifs,
  et c'est là que doit aller toute nouvelle logique testable.
- `src/` (racine) — couches matérielles : Wi-Fi, SPI, ADC, I²C, MQTT. Fines par construction,
  elles se contentent d'appeler `core/`.
- `test/test_native/` — tests Unity.
- `test/fixtures/` — réponses Sonos réelles, **anonymisées**.

## Fixtures

Les réponses SOAP d'un Sonos contiennent un Household ID, des UUID d'enceintes, des adresses
IP privées et des identifiants de compte Spotify. Ne commitez jamais une capture brute :
passez-la par `tools/scrub_fixture.py`, qui remplace ces champs par des valeurs factices, et
ne versionnez que le résultat. Le répertoire `test/fixtures/raw/` est git-ignoré à cet effet.

De même, n'utilisez pas de pochette d'album commerciale comme fixture binaire : générez une
image de test localement.
