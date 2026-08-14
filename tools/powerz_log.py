#!/usr/bin/env python3
"""Enregistre la consommation du boîtier depuis un POWER-Z KM003C (ChargerLab).

Le compteur s'expose comme un port série. On lui demande son bloc ADC et il
renvoie tension et courant en micro-unités. Sortie en CSV sur la sortie
standard, pour être tracée ou moyennée ensuite.

    python3 tools/powerz_log.py /dev/cu.usbmodemXXXX 600 > mesure.csv

Deux pièges, tous deux rencontrés :

- **L'application ChargerLab tient le port en exclusivité.** Il faut la fermer,
  sinon macOS renvoie « Resource busy ».
- **Un seul lecteur à la fois.** Deux processus sur le même port entrelacent
  leurs trames, et le décodage produit alors des valeurs aberrantes — des
  milliers de watts négatifs, faciles à repérer mais faciles à croire si l'on
  ne regarde que la moyenne.

Et surtout, côté mesure elle-même : **retirer la batterie du reTerminal**. Voir
docs/hardware.md — batterie branchée, le chargeur limite le courant d'entrée et
masque entièrement la consommation de l'ESP32.
"""
import struct
import sys
import time

import serial

CMD_GET_DATA = 0x0C
ATT_ADC = 0x001


def request(packet_id: int) -> bytes:
    """En-tête de contrôle : type, identifiant, attribut décalé d'un bit."""
    return struct.pack("<BBH", CMD_GET_DATA, packet_id & 0xFF, ATT_ADC << 1)


def main() -> int:
    port_name = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem0762112"
    duration_s = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0

    deadline = time.time() + duration_s
    started = time.time()
    print("t_s,volts,amps,watts", flush=True)

    with serial.Serial(port_name, 115200, timeout=1) as port:
        packet_id = 0
        while time.time() < deadline:
            port.reset_input_buffer()
            port.write(request(packet_id))
            port.flush()
            packet_id += 1

            time.sleep(0.03)
            data = port.read(64)
            if len(data) < 16:
                continue

            # 4 octets d'en-tête, 4 d'en-tête étendu, puis la charge utile ADC :
            # tension et courant, en microvolts et microampères.
            volts_uv, amps_ua = struct.unpack_from("<ii", data, 8)
            volts, amps = volts_uv / 1e6, amps_ua / 1e6
            print(f"{time.time() - started:.2f},{volts:.4f},{amps:.5f},"
                  f"{volts * amps:.4f}", flush=True)
            time.sleep(0.45)

    return 0


if __name__ == "__main__":
    sys.exit(main())
