# Link’s Awakening DX — iOS

Application UIKit pour **Link’s Awakening DX (Game Boy Color, 2D)**, iPhone et iPad à partir d’iOS 16. Le jeu est recompilé en C à partir de la ROM locale, puis compilé par Xcode pour ARM64. Rendu Metal, audio AVAudioEngine et manettes GameController. Aucun SDL, navigateur, JIT, CocoaPods ou téléchargement de dépendances à la compilation.

## Ouvrir et compiler

1. Cloner ce dépôt, puis ouvrir **`LinksAwakeningIOS.xcodeproj`**.
2. Sélectionner le schéma **LinksAwakeningDX** et un simulateur ou votre iPhone/iPad.
3. Pour un appareil physique : choisir votre équipe dans **Signing & Capabilities**, et adapter l’identifiant de bundle si nécessaire.
4. **⌘R**. La phase « Prepare and verify DX ROM » détecte automatiquement l’unique `.gbc` à la racine. La première compilation construit le recompileur local puis génère les sources C ; elle est plus longue que les suivantes.

Utiliser Xcode 16 ou ultérieur avec ses outils en ligne de commande et Python 3 (`/usr/bin/python3`). Le projet et le schéma partagé sont déjà versionnés : aucun générateur de projet à installer.

Pour une ROM stockée ailleurs :

```sh
python3 Scripts/prepare_rom.py '/chemin/vers/jeu.gbc'
open LinksAwakeningIOS.xcodeproj
```

Les dumps DX non modifiés FR, EN, DE et JP sont reconnus par empreinte. La ROM française Rev 1 présente dans le dépôt correspond à `68242187b65166b5f8225b20e2021659` (MD5). Les sources générées et fichiers de travail restent dans `DerivedSources/` et `build/`, ignorés par Git. Le manifeste de préparation vérifie l’intégrité de chaque source avant compilation.

Option de signature locale, sans modifier le projet : créer `LocalConfig.xcconfig` :

```xcconfig
DEVELOPMENT_TEAM = VOTRE_EQUIPE
PRODUCT_BUNDLE_IDENTIFIER = votre.identifiant.awakeningdx
```

## Fonctions iOS

- Portrait et paysage, zones de sécurité pour encoche et indicateur d’accueil, iPhone et iPad.
- Pixels nets avec Metal, format original respecté, option de grossissement entier.
- Tactile multitouch : diagonales, A/B, Start/Select, vibrations désactivables.
- Manettes compatibles GameController : croix/stick, A/B avec inversion optionnelle, Start, Select ; **L + R** ouvre la pause. Masquage du tactile en présence d’une manette en option.
- Audio stéréo 44 100 Hz ; pause lors d’une interruption, d’un débranchement audio ou d’un passage en arrière-plan.
- Sauvegarde cartouche périodique, reprise automatique en arrière-plan, sauvegarde/chargement manuels et export via la feuille de partage iOS.
- Cadence du jeu indépendante des écrans 60/120 Hz ; aucune accélération sur ProMotion.

L’application démarre en pause. Toucher **Reprendre**, puis utiliser **START** dans le jeu.

## Nature du port et écran large

Le chemin principal utilise le C produit par la recompilation statique. Le runtime conserve la simulation du matériel Game Boy Color (mémoire, interruptions, PPU, APU) et un interpréteur de secours pour les adresses non couvertes et le code en RAM. Ce n’est pas une réécriture intégrale de la logique du jeu en Swift.

Le paysage utilise la largeur de l’écran pour l’interface et les commandes. **La vue du monde reste en 160 × 144 : un véritable champ de vision 16:9 avec davantage de décor n’est pas implémenté.** Aucun étirement ou recadrage du jeu n’est appliqué.

## Sauvegardes

Fichiers privés dans `Application Support/LinksAwakeningDX/Saves/`, nommés par SHA-256 de ROM : `.sav` pour la cartouche et `.state` pour la reprise. L’écriture remplace atomiquement le fichier précédent. Une reprise est liée à la ROM et à la version du runtime ; une corruption ou une troncature est rejetée avant modification de la partie en mémoire.

Il y a **un emplacement de reprise** : la sauvegarde manuelle et la reprise en arrière-plan partagent cet emplacement. Exporter une copie pour la conserver. Les fichiers `.sav` sont indépendants du format interne de reprise. Ne pas supprimer l’app sans avoir exporté les sauvegardes à conserver.

## Vérification

Le test natif local a démarré la ROM française jusqu’à l’écran titre en couleur et vérifie le flux de frames/audio, la reprise déterministe, le rejet des snapshots corrompus/tronqués et le redémarrage depuis disque. Cela ne constitue pas une validation d’une partie complète.

```sh
python3 Scripts/test_core.py
```

Le workflow **iOS build and core checks** vérifie le noyau avec AddressSanitizer/UBSan sur une ROM synthétique libre, puis compile Debug pour simulateur et Release pour appareil sur macOS. Il lance aussi l’app dans un simulateur. La compilation macOS utilise la ROM à la racine quand elle est disponible ; sinon une fixture synthétique identifiée explicitement. Aucun binaire de jeu n’est publié par le workflow.

La validation Xcode et la validation sur appareil physique doivent être distinguées : consulter le résultat du workflow ; tactile, Bluetooth, interruptions et autonomie restent à confirmer sur un iPhone/iPad réel.

Pour vérifier la chaîne sans ROM de jeu :

```sh
mkdir -p build
python3 Tests/make_fixture.py build/fixture.gbc
python3 Scripts/prepare_rom.py build/fixture.gbc --fixture
DX_ALLOW_FIXTURE=YES xcodebuild -project LinksAwakeningIOS.xcodeproj -scheme LinksAwakeningDX -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO DX_ALLOW_FIXTURE=YES build
```

Après ce test, relancer la préparation avec la ROM DX avant de jouer. Les sources de fixture sont refusées par défaut dans Xcode.

## Sources et licences

- [gb-recompiled](https://github.com/sp00nznet/gb-recompiled), moteur C et recompileur C++, figés au commit `ceb856bb66dcf0d839054148b9f96f49d4fe23b8`, MIT ; voir `Vendor/UPSTREAM.md`.
- [linksawakening-portable](https://github.com/sp00nznet/linksawakening-portable), référence de portage DX étudiée. Le backend iOS de ce dépôt est propre à UIKit/Metal/AVAudioEngine.
- [zladx/LADX-Disassembly](https://github.com/zladx/LADX-Disassembly), référence des empreintes des révisions DX.

Le code d’intégration est sous MIT ; les ROM, données et sources de jeu générées conservent les droits de leurs titulaires.
