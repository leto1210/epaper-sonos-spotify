// Configuration écran pour Seeed_GFX. La bibliothèque inclut ce fichier
// automatiquement (User_Setup_Select.h : `#if __has_include("driver.h")`).
//
// 521 = reTerminal E1002 — ePaper 7,3" 800x480, 6 couleurs (pilote ED2208).
// Il définit à lui seul le brochage SPI (SCK 7, MOSI 9, CS 10, DC 11, RST 12,
// BUSY 13) ; voir docs/hardware.md.
//
// Pour un autre modèle de la série E10xx, régénérez cette valeur avec le
// Seeed GFX Configuration Tool : https://seeed-studio.github.io/Seeed_GFX/
#pragma once

#define BOARD_SCREEN_COMBO 521
