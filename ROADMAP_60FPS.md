# Feuille de Route Technique : Vrai 60 FPS & Framerate Élevé (SwitchWaker)

Le document complet détaillant l'architecture, la cartographie des fichiers C++, et les 5 phases d'implémentation a été rédigé et se trouve dans :

👉 [docs/ROADMAP_60FPS.md](file:///c:/Users/apple/Desktop/tww/SwitchWaker/docs/ROADMAP_60FPS.md)

### Résumé des 5 phases d'implémentation :
1. **Phase 1 : Cadence de Rendu & Limiteur (60 Hz)** (`pc_jfw_display.cpp`, `pc_frame.cpp`, `d_s_play.cpp`).
2. **Phase 2 : Échelle Temporelle d'Animation** (`J3DFrameCtrl.cpp`, `mDoExt_McaMorf`, animations de texture).
3. **Phase 3 : Normalisation Physique de Link & Lissage Caméra** (`d_a_player.cpp`, `d_camera.cpp`).
4. **Phase 4 : Durée de Vie Particules & Effets** (`JPAEmitter.cpp`, `d_particle.cpp`).
5. **Phase 5 : Protection des Cutscenes & Boîtes de Dialogue** (`d_event.cpp`, `d_msg_*.cpp`).
