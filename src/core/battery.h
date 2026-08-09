#pragma once

// Conversion de la tension de l'accu en état de charge. Logique pure : la
// lecture ADC est ailleurs, ici on ne fait que traduire.
namespace battery {

// Bornes constatées sur l'accu du reTerminal E1002.
constexpr int kFullMv = 4150;
constexpr int kEmptyMv = 3270;

// Pourcentage de 0 à 100, borné. Une tension nulle ou absurde renvoie -1 :
// mieux vaut ne rien afficher qu'un pourcentage inventé.
int percentFromMillivolts(int millivolts);

// Vrai si la tension dépasse ce que l'accu seul peut fournir : le boîtier est
// alors alimenté par l'USB. Il n'y a pas de broche dédiée à cette détection.
bool isCharging(int millivolts);

}  // namespace battery
