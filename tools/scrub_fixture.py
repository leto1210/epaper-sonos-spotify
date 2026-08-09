#!/usr/bin/env python3
"""Anonymise une réponse SOAP Sonos avant de la verser dans test/fixtures/.

Une capture brute contient l'identifiant de foyer Sonos, les UUID des enceintes,
les adresses IP et MAC du réseau, les noms des pièces (souvent des prénoms) et
l'identifiant de session du compte Spotify. Rien de tout cela n'a sa place dans
un dépôt public.

Le remplacement est *déterministe et cohérent* : un même UUID reçoit toujours le
même pseudonyme, y compris d'un fichier à l'autre traités dans le même appel.
C'est indispensable, sinon les tests de résolution du coordinateur — qui
consistent justement à faire correspondre des UUID entre eux — perdraient leur
sens.

    ./tools/scrub_fixture.py test/fixtures/raw/*.xml -o test/fixtures/
"""

import argparse
import pathlib
import re
import sys

# Noms de pièces neutres, attribués dans l'ordre de première rencontre.
ROOM_NAMES = [
    "Salon", "Cuisine", "Bureau", "Chambre", "Salle de bain",
    "Entree", "Terrasse", "Garage", "Couloir", "Atelier",
]


class Scrubber:
    def __init__(self):
        self._uuids = {}
        self._ips = {}
        self._rooms = {}

    def _uuid(self, match):
        raw = match.group(0)
        if raw not in self._uuids:
            self._uuids[raw] = f"RINCON_{len(self._uuids) + 1:012d}01400"
        return self._uuids[raw]

    def _ip(self, match):
        raw = match.group(0)
        # 192.0.2.0/24 est le bloc TEST-NET-1 réservé à la documentation (RFC 5737).
        if raw not in self._ips:
            self._ips[raw] = f"192.0.2.{len(self._ips) + 10}"
        return self._ips[raw]

    def _room(self, match):
        quote, raw = match.group(1), match.group(2)
        if raw not in self._rooms:
            i = len(self._rooms)
            self._rooms[raw] = ROOM_NAMES[i] if i < len(ROOM_NAMES) else f"Piece {i + 1}"
        return f"ZoneName={quote}{self._rooms[raw]}{quote}"

    def scrub(self, text: str) -> str:
        # UUID d'enceintes — la forme apparaît aussi bien dans les attributs que
        # dans les URI x-rincon: et x-sonos-vli:.
        text = re.sub(r"RINCON_[0-9A-F]{17}", self._uuid, text)
        # Identifiant de foyer.
        text = re.sub(r"(?i)(Sonos_)[0-9A-Za-z]{20,}", r"\1HOUSEHOLDxxxxxxxxxxxxxxxxx", text)
        # Noms de pièces. La topologie est renvoyée en XML échappé : le guillemet
        # est tantôt `"`, tantôt `&quot;`.
        text = re.sub(r'ZoneName=("|&quot;)(.*?)\1', self._room, text)
        # Adresses IP privées.
        text = re.sub(r"\b(?:10|192\.168|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}\b",
                      self._ip, text)
        # Adresses MAC.
        text = re.sub(r"\b(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\b", "00:11:22:33:44:55", text)
        # Identifiant de session Spotify Connect (spotify:<32 hexa>) et numéro de
        # compte du service (sn=), tous deux liés au compte de l'utilisateur.
        text = re.sub(r"spotify:[0-9a-f]{32}", "spotify:" + "0" * 32, text)
        text = re.sub(r"(sn=)\d+", r"\g<1>1", text)
        # Jetons de session éventuels.
        text = re.sub(r"(?i)(token=)[0-9A-Za-z._-]+", r"\1REDACTED", text)
        return text


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", type=pathlib.Path)
    ap.add_argument("-o", "--out-dir", type=pathlib.Path, required=True)
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    scrubber = Scrubber()  # partagé : la cohérence inter-fichiers en dépend

    for path in args.files:
        out = args.out_dir / path.name
        out.write_text(scrubber.scrub(path.read_text()))
        print(f"{path} -> {out}")

    print(f"\n{len(scrubber._uuids)} UUID, {len(scrubber._ips)} IP, "
          f"{len(scrubber._rooms)} pièce(s) anonymisés.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
