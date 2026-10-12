# Provenance

`GBRuntime` et `Recompiler` : sous-ensemble de https://github.com/sp00nznet/gb-recompiled au commit `ceb856bb66dcf0d839054148b9f96f49d4fe23b8` (MIT).

Les fichiers `LICENSE` d’origine sont conservés dans les deux dossiers.
SDL, ImGui et les modules réseau ne sont pas intégrés au binaire iOS.

Modifications locales de runtime : API de copie/restauration opaque APU ; accumulateur audio exact 44 100 Hz ; cadencement PPU/APU à la vitesse matérielle en mode CPU double ; restitution du contrôle lors de l’extinction LCD.

Ces adaptations nécessitent des tests supplémentaires sur le jeu réel et sur iPhone.
