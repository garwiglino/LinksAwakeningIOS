# Link’s Awakening DX — iOS

Port iOS natif en cours, pour la version **Game Boy Color** (pas le remake Switch).

**Premier dépôt de travail : le projet Xcode et la chaîne de préparation sont encore en cours de création. Ce commit ne constitue pas encore une application compilable.**

La base est le recompileur C de [sp00nznet/gb-recompiled](https://github.com/sp00nznet/gb-recompiled), commit `ceb856bb66dcf0d839054148b9f96f49d4fe23b8`.
Le port multiplateforme [linksawakening-portable](https://github.com/sp00nznet/linksawakening-portable) a servi de référence pour son interface de plateforme.

Les sources présentes ajoutent un backend iOS UIKit/Metal/AVAudioEngine, une file audio sans verrou, des sauvegardes atomiques par empreinte ROM et une sérialisation des états CPU/PPU/APU sans restaurer de pointeurs.

La logique connue du jeu est recompilée en C puis en code machine. Le runtime conserve une simulation du matériel Game Boy et un interpréteur de secours pour le code non traduit. Aucun JIT ni navigateur n’est requis.

L’affichage conserve le ratio 160:144. L’interface sera adaptée aux écrans larges iPhone/iPad ; un agrandissement du monde visible au-delà de l’écran Game Boy n’est pas encore implémenté.

Aucune ROM ni source dérivée de ROM n’est incluse dans les fichiers ajoutés par ce port.
