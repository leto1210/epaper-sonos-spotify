#pragma once

#include <string>

#include "core/dither.h"

// Récupération et préparation de la pochette. L'image est téléchargée depuis
// l'enceinte elle-même — voir docs/sonos-api.md : `upnp:albumArtURI` pointe
// vers le CDN Spotify en HTTPS, inexploitable ici, alors que l'enceinte sert la
// même image en HTTP simple sur le LAN.
namespace albumart {

// Pochette prête à être dessinée : un indice de palette par pixel, carrée.
// Le tampon vit en PSRAM et appartient au module ; il reste valide jusqu'au
// prochain appel à `load()`.
struct Bitmap {
  const dither::Ink* pixels = nullptr;
  int size = 0;

  bool valid() const { return pixels != nullptr && size > 0; }
};

// Télécharge, décode, réduit et trame en une passe. Renvoie une image invalide
// en cas d'échec — l'appelant dessine alors son emplacement de repli plutôt que
// de laisser un trou.
Bitmap load(const std::string& url, int target_size);

// Libère les tampons PSRAM. Utile avant un sommeil prolongé.
void release();

}  // namespace albumart
