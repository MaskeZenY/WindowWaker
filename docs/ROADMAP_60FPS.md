# Feuille de Route Technique : Vrai 60 FPS & Framerate Élevé (SwitchWaker)

Ce document détaille l'architecture et les étapes requises pour implémenter un **vrai 60 FPS (et framerate débloqué/haut taux de rafraîchissement)** dans le code source de *The Legend of Zelda: The Wind Waker* (portage natif SwitchWaker).

---

## 1. Pourquoi le "Vrai 60 FPS" et le refus de l'interpolation vidéo

L'interpolation vidéo externe (type Lossless Scaling, LSFG, AFMF, DLSS Frame Generation) est inadaptée pour *The Wind Waker* pour plusieurs raisons critiques :
1. **Artefacts visuels sévères sur le Cel-Shading** : Les contours noirs marqués du moteur de Wind Waker bavent et se déchirent lors de la synthèse d'images par IA.
2. **Déformation de l'eau et des particules** : Les textures d'eau animées procédurales et les effets de vent créent des traînées floues et du ghosting prononcé.
3. **Latence d'entrée (Input Lag)** : L'interpolation vidéo ajoute au minimum 1 à 2 trames de buffer de latence (16 à 33 ms supplémentaires).
4. **Physique et réactivité inchangées** : Les entrées manette continuent d'être échantillonnées à 30 Hz.

**L'objectif ici est une modification native du moteur C++**, où chaque trame est réellement calculée, avec échantillonnage d'entrées réactif et géométrie mise à jour à chaque frame.

---

## 2. Analyse comparative des grands portages de décompilation

### A. L'approche *Ship of Harkinian* / *2Ship2Harkinian* (Ocarina of Time / Majora's Mask)
* **Architecture** : Découplage strict entre la boucle logique (**Tick Loop** à 20/30 Hz fixe) et la boucle de rendu (**Render Loop** à 60/120/144 Hz ou débloqué).
* **Fonctionnement** :
  - Le monde, la physique, les collisions et l'IA avancent à cadence fixe (garantie mathématique de zéro glitch de physique, zéro désynchronisation de quête).
  - Le pipeline de rendu interpole les matrices de transformation (`Mtx`, os squelettiques, position de caméra) entre l'état précédent et l'état actuel avec un ratio temporel $\alpha \in [0, 1]$.
  - Les contrôles de la caméra et les entrées peuvent être sondés à la fréquence d'affichage.

### B. L'approche Dolphin 60 FPS Gecko (Moteur forcé à 60 Hz)
* **Architecture** : La boucle de jeu entière s'exécute à 60 Hz.
* **Fonctionnement** :
  - Le temps entre deux ticks passe de 33.33 ms à 16.66 ms.
  - Toutes les vitesses scalaires ($v$) sont divisées par 2.
  - La gravité ($g$) est recalculée pour l'intégration de Verlet/Euler à double fréquence ($g / 4$).
  - La cadence d'animation squelettique (`J3DFrameCtrl`) est divisée par 2.
  - **Défis majeurs** : Wind Waker contient 454 acteurs, des compteurs entiers de frames (`s16 mTimer`) et des scripts de cinématiques `.stb` synchronisés à la frame près avec l'audio.

---

## 3. Cartographie des sous-systèmes dans le code source de SwitchWaker

### 1. La cadence de boucle (`Frame Pacing` & VSync)
* **[`native/src/pc/game_hooks/pc_jfw_display.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/native/src/pc/game_hooks/pc_jfw_display.cpp)** :
  - Fonction `pc_wait_for_tick(u32 p1, u16 p2)`. C'est le cœur du pacing sur PC.
  - Actuellement configuré pour un cycle de retraces GameCube classique (33.33 ms pour 30 FPS via `PC_RETRACE_PERIOD_NS`).
* **[`game/src/d/d_s_play.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/d/d_s_play.cpp#L1374)** :
  - `mDoGph_gInf_c::setTickRate((OS_BUS_CLOCK / 4) / 30);`
  - La scène de jeu principale `d_s_play` fixe explicitement 30 ticks par seconde. (À noter : le logo et les menus sont déjà configurés à `/ 60` !).
* **[`native/src/pc/runtime/pc_frame.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/native/src/pc/runtime/pc_frame.cpp)** :
  - Le régulateur d'attente `sLimiter.Sleep(periodNs)` gère la synchronisation de la boucle de jeu.

### 2. Le moteur d'animation squelettique (`J3D`)
* **[`game/src/JSystem/J3DGraphAnimator/J3DFrameCtrl.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/JSystem/J3DGraphAnimator/J3DFrameCtrl.cpp)** :
  - `J3DFrameCtrl::update()` : avance les animations squelettiques via `mCurFrame += mRate`. Par défaut, `mRate = 1.0f`.
  - À 60 FPS, si `mRate` reste à 1.0f, toutes les animations (Link, ennemis, PNJ) jouent à double vitesse.
* **[`game/src/m_Do/m_Do_ext.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/m_Do/m_Do_ext.cpp)** (`mDoExt_McaMorf`) :
  - Contrôle l'interpolation des poses osseuses et les transitions d'animation.

### 3. La physique, la gravité et les collisions
* **[`game/src/d/actor/d_a_player.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/d/actor/d_a_player.cpp)** & **`d_a_player_main.cpp`** :
  - Vélocité de Link (`mVelocity`, `mSpeedF`).
  - Gravité (`mGravity`) et vitesse de chute max (`mMaxFallSpeed`).
  - Intégration d'Euler :
    $$v_{t+1} = v_t + g$$
    $$x_{t+1} = x_t + v_{t+1}$$
  - Si le nombre de pas double, le déplacement par pas doit être réduit proportionnellement.

### 4. La caméra (`d_camera.cpp`)
* **[`game/src/d/d_camera.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/d/d_camera.cpp)** :
  - La caméra utilise un lissage exponentiel par frame pour suivre Link :
    $$\text{pos} \leftarrow \text{pos} + (\text{cible} - \text{pos}) \times \alpha$$
  - À 60 FPS, le coefficient de damping $\alpha$ doit être ajusté pour éviter une caméra trop rigide :
    $$\alpha_{60} = 1 - \sqrt{1 - \alpha_{30}}$$

### 5. Les particules & effets spéciaux (`JParticle` / `JPA`)
* **[`game/src/JSystem/JParticle/`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/JSystem/JParticle/)** :
  - Taux d'émission des particules par frame et durée de vie (`life` exprimée en nombre de frames entières).
  - Traînées d'épée, écume sur l'océan, fumée des bombes.

### 6. Les cinématiques & le système d'événements
* **[`game/src/d/d_event.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/game/src/d/d_event.cpp)** & **`d_event_manager.cpp`** :
  - Les archives `Demo*.arc` et fichiers `.stb` (ex: `title.stb`, cinématiques de boss) déclenchent des actions à des numéros de frame absolus.
  - L'audio (`JAZelAudio` / `m_Do_audio.cpp`) joue en continu ; un avancement accéléré des cutscenes désynchroniserait les voix et les musiques interactives.

---

## 4. Feuille de Route en 5 Phases d'Implémentation

```
┌─────────────────────────────────────────────────────────────┐
│ Phase 1 : Cadence de Rendu & Contrôle du Limiteur (60 Hz)   │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Phase 2 : Échelle Temporelle d'Animation (J3DFrameCtrl)     │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Phase 3 : Normalisation Physique de Link & Lissage Caméra   │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Phase 4 : Durée de Vie Particules & Entités Globales         │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│ Phase 5 : Isolation des Cutscenes & Menus (Mode Protégé)    │
└─────────────────────────────────────────────────────────────┘
```

### Phase 1 : Cadence de Rendu & Contrôle du Limiteur (60 Hz)
- **Objectif** : Permettre au moteur de s'exécuter à 60 FPS (16.66 ms) au lieu de 30 FPS (33.33 ms).
- **Actions** :
  1. Ajouter un paramètre `COS_TARGET_FPS` (`30`, `60`, `120`, `0` = débloqué) dans `pc_settings.h` et dans le menu F1 (`pc_menu.cpp`).
  2. Adapter `pc_wait_for_tick` dans [`pc_jfw_display.cpp`](file:///c:/Users/apple/Desktop/tww/SwitchWaker/native/src/pc/game_hooks/pc_jfw_display.cpp) pour calculer dynamiquement :
     ```cpp
     periodNs = 1000000000ull / targetFps;
     ```
  3. Mettre à jour `mDoGph_gInf_c::setTickRate` dans `d_s_play.cpp` pour refléter la fréquence cible.

### Phase 2 : Échelle Temporelle d'Animation (`J3DFrameCtrl`)
- **Objectif** : Éviter que le jeu ne tourne en avance rapide 2x.
- **Actions** :
  1. Définir un coefficient temporel global :
     ```cpp
     float g_animSpeedMultiplier = 30.0f / (float)targetFps; // 0.5f à 60 FPS
     ```
  2. Modifier `J3DFrameCtrl::update()` pour multiplier `mRate` par `g_animSpeedMultiplier`.
  3. Corriger les animations de textures (`btk`, `btp`, `brk`, `bpk`) dans `J3DGraphAnimator`.

### Phase 3 : Normalisation Physique de Link & Lissage Caméra
- **Objectif** : Conserver des sauts, des courses et des trajectoires strictement identiques à l'original.
- **Actions** :
  1. Dans `d_a_player_main.cpp`, appliquer le pas de temps $dt$ sur :
     - La vélocité horizontale (`mSpeedF`).
     - L'accélération gravitationnelle : $g_{\text{eff}} = g \times dt$.
     - Les frottements et la vitesse de rouleau.
  2. Dans `d_camera.cpp`, recalculer les constantes de lerp pour que le suivi de Link reste naturel sans saccade.

### Phase 4 : Durée de Vie Particules & Entités Globales
- **Objectif** : Assurer que les flammes, l'écume et la fumée persistent la même durée en secondes réelles.
- **Actions** :
  1. Adapter le décompte de vie des émetteurs dans `JPAEmitter.cpp`.
  2. Ajuster la vitesse de déplacement des particules selon $dt$.

### Phase 5 : Isolation des Cutscenes & Menus (Mode Protégé)
- **Objectif** : Zéro désynchronisation entre les cinématiques narratives et l'audio.
- **Actions** :
  1. Détecter l'état cinématique (`dComIfGp_event_runCheck()`).
  2. Lorsque le moteur lit un script d'événement `.stb`, verrouiller temporairement la cadence logique à 30 ticks/s (ou diviser par 2 l'incrémentation du pointeur de frame de l'événement).
  3. Vérifier que la vitesse d'apparition du texte dans les bulles de dialogue (`d_msg_*.cpp`) reste confortable pour la lecture.

---

## 5. Synthèse & Prochaines Étapes

Cette feuille de route permet d'implémenter le 60 FPS proprement et par couches vérifiables :
1. **La Phase 1 + 2** apporte immédiatement 80% du bénéfice visuel (fluidité d'affichage 60 FPS et animations synchronisées).
2. **La Phase 3** garantit la précision du gameplay et des sauts.
3. **Les Phases 4 et 5** assurent la finition complète et la fidélité sans bug de l'aventure.
