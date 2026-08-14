# Audit de securite - epaper-sonos-spotify

Date: 2026-08-14
Auteur: GitHub Copilot (GPT-5.3-Codex)
Type: Revue statique du code et de la configuration
Portee: firmware C++/Arduino, scripts, CI, hygiene des secrets

## 1) Resume executif

Le projet est globalement propre sur les points de robustesse memoire et hygiene de depot, mais il repose sur un modele de confiance LAN: controle Sonos et MQTT en clair, sans chiffrement transport ni authentification applicative forte.

Risque principal:
- Un acteur present sur le reseau local (ou sur un VLAN mal segmente) peut observer ou injecter des commandes MQTT/Sonos.

Priorites:
1. Durcir MQTT (TLS, ACL strictes, droits minimum).
2. Valider strictement les commandes MQTT cote firmware.
3. Rendre la chaine de build plus reproductible (pinning exact des dependances/outils).

## 2) Methode

- Lecture ciblee des fichiers de configuration, reseau, MQTT, Sonos, album art, scripts et CI.
- Recherche de motifs a risque: secrets, APIs reseau en clair, fonctions C dangereuses, parsing d'entrees non fiables.
- Verification des garde-fous anti-fuite dans le depot.

Limites:
- Pas de test d'intrusion en environnement reel.
- Pas de scan CVE automatise en ligne.
- Conclusion dependante de la configuration broker/HA reseau effective.

## 3) Constatations detaillees (classees par severite)

### [Elevee] Controle et telemetrie en clair sur le LAN

Constat:
- MQTT utilise un socket non TLS (WiFiClient + PubSubClient) et credentials en clair sur le transport.
- Sonos SOAP et recuperation de pochettes en HTTP non chiffre.

References:
- src/mqtt.cpp:28
- src/mqtt.cpp:82
- config.example.h:37
- src/sonos_client.cpp:44
- src/sonos_client.cpp:45
- src/core/sonos_parser.cpp:235
- src/albumart.cpp:94

Impact:
- Ecoute passive possible (metadonnees, topics, etat).
- Injection active possible (commandes de controle, payloads malveillants) depuis un poste sur le meme segment reseau.

Recommandations:
- Activer MQTT TLS (port 8883) et certificats broker.
- Segmenter VLAN IoT et limiter les flux sortants/entrants au strict necessaire.
- Appliquer ACL broker: un seul publisher autorise sur topics de commande, droits differencies lecture/ecriture.

---

### [Moyenne] Validation insuffisante des commandes MQTT

Constat:
- Les commandes HA sont acceptees des qu'elles arrivent sur les topics attendus.
- Pour la zone, seule la non-vacuite du payload est verifiee.

References:
- src/mqtt.cpp:94
- src/mqtt.cpp:96
- src/core/ha_discovery.cpp:251
- src/core/ha_discovery.cpp:260
- src/core/ha_discovery.cpp:263
- src/main.cpp:204
- src/main.cpp:211
- src/main.cpp:219

Impact:
- Un client non autorise sur le broker peut provoquer des rafraichissements ePaper inutiles (usure/energie) et perturber la logique de selection de zone.

Recommandations:
- Verifier que la zone commandee appartient a la liste publiee des zones detectees.
- Ajouter une validation de schema/pattern sur payloads de commande.
- Optionnel: signature applicative simple (HMAC) si le broker ne peut pas garantir l'identite de l'emetteur.

---

### [Moyenne] Risques supply-chain et reproductibilite build

Constat:
- Plateforme ESP32 chargee via URL zip externe.
- Une librairie deposee via URL Git directe non figee sur commit/tag immuable.
- Plusieurs dependances en semver flottant compatible.
- CI installe PlatformIO en upgrade sans pinning strict.

References:
- platformio.ini:13
- platformio.ini:49
- platformio.ini:50
- platformio.ini:52
- platformio.ini:53
- .github/workflows/ci.yml:18
- .github/workflows/ci.yml:39

Impact:
- Variabilite des builds dans le temps.
- Exposition accrue a une compromission en amont (release, registry, transitive deps).

Recommandations:
- Epingler versions exactes des libs critiques et de PlatformIO.
- Figer Seeed_GFX sur commit SHA.
- Documenter/verifier checksums des artefacts telecharges.

---

### [Faible a moyenne] Gestion des secrets compile-time

Constat:
- Secrets Wi-Fi/MQTT definis en macros dans la config locale compilee.
- Le depot protege correctement contre commit accidentel via gitignore.

References:
- config.example.h:7
- config.example.h:39
- .gitignore:10
- .gitignore:23
- .gitignore:24

Impact:
- En cas d'acces physique au binaire/flash, extraction des secrets facilitee selon niveau de protection de la carte.

Recommandations:
- Si menace physique incluse: secure boot, flash encryption, et processus de provisioning secrets hors code source.

## 4) Points positifs

- Hygiene anti-fuite explicite dans le depot (secrets et captures brutes ignores).
  - .gitignore:10
  - .gitignore:14
- Script d'anonymisation Sonos pertinent et deterministe pour fixtures.
  - tools/scrub_fixture.py:56
  - tools/scrub_fixture.py:66
  - tools/scrub_fixture.py:75
- Garde-fous memoire sur le telechargement d'images (taille max et allocation controlee).
  - src/albumart.cpp:19
  - src/albumart.cpp:160
- Pas d'usage evident de fonctions C classiquement dangereuses (strcpy, sprintf non borne) dans src/.

## 5) Plan de remediations priorise

### 0 a 2 semaines

1. Broker MQTT:
- TLS obligatoire.
- ACL minimales sur topics de commande et d'etat.
- Compte device dedie, mot de passe fort, rotation.

2. Firmware:
- Rejeter toute commande zone hors liste des zones connues.
- Journaliser les commandes rejetees (compteur diagnostic).

3. Exploitation:
- VLAN IoT dedie + regles firewall explicites vers Sonos:1400 et broker MQTT.

### 2 a 6 semaines

1. Build/CI:
- Pinning exact versions dependances.
- Pinning exact version PlatformIO en CI.
- Verification d'integrite des artefacts externes.

2. Hardening optionnel:
- Evaluation secure boot / flash encryption selon contraintes de maintenance.

## 6) Checklist de verification post-correction

- Les commandes MQTT non autorisees sont refusees (tests manuels + logs).
- Une zone invalide publiee sur topic cmd ne modifie pas l'etat interne.
- Les pipelines CI produisent des builds reproductibles entre runs rapproches.
- Les secrets ne figurent ni dans git status ni dans les artefacts CI.

## 7) Statut

- Audit: termine (statique)
- Correctifs code: non appliques dans ce document
- Prochaine etape recommandee: creer une tache de durcissement avec patchs incrementaux et tests associes
