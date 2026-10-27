# SonRacRecomp

Recompilación de **Sonic & All-Stars Racing Transformed** (Xbox 360) para PC y Android.

## Aviso legal / Legal notice

**Español**

- **No fomentamos la piratería.** Este repositorio no contiene el juego, ni sus recursos, ni el
  ejecutable original, ni su código. Todo material protegido por derechos de autor pertenece a sus
  respectivos propietarios.
- Para compilar y ejecutar este proyecto necesitas **tu propia copia legal** del juego, y hacer tú
  mismo el *dump* (volcado) de tu disco/ejecutable. No pedimos, no compartimos ni ayudamos a
  conseguir copias no autorizadas.
- *Sonic & All-Stars Racing Transformed* es propiedad de SEGA / Sumo Digital. Este es un proyecto de
  fans sin ánimo de lucro, sin relación alguna con SEGA, Sumo Digital ni Microsoft.
- El código generado a partir del juego (`generated/`) y los datos del juego (`assets/`,
  `android_assets/`) **no se suben a este repositorio**: cada usuario los produce en local desde su
  propia copia.

**English**

- **We do not condone piracy.** This repository does not contain the game, its assets, the original
  executable, or its code. All copyrighted material belongs to its respective owners.
- To build and run this project you need **your own legally purchased copy** of the game and must
  dump it yourself. We do not request, share, or help obtain unauthorized copies.
- *Sonic & All-Stars Racing Transformed* is © SEGA / Sumo Digital. This is a non-profit fan project,
  not affiliated with SEGA, Sumo Digital, or Microsoft.
- Game-derived code (`generated/`) and game data (`assets/`, `android_assets/`) are **not** part of
  this repository: each user produces them locally from their own copy.

## Estado del proyecto

- **Recompilación (PC):** el juego se ejecuta nativamente en Windows a partir del código PowerPC
  recompilado, con la GPU de Xbox 360 emulada por el plugin `xenos`.
- **Render nativo:** en reconstrucción **desde cero**, pensado solo para Sonic. El código heredado de
  otros proyectos (el renderizador nativo de NFSMW y sus herramientas de shaders) fue descartado
  por completo: cada juego tiene su propia pipeline gráfica y lo que sirve para uno no sirve para
  otro.
- **Android:** en progreso (build en `android_native/`).

## Cómo se construye

1. Consigue tu propia copia del juego y haz el dump del ejecutable (`default.xex`).
2. Coloca tu dump en `assets/default.xex` (esta carpeta está en `.gitignore`, no se sube).
3. Genera el código recompilado con la configuración de ReXGlue
   (`sonicracingtransformed_config.toml` → salida en `generated/`).
4. Compila con CMake (Ninja + clang): preset Windows en `CMakePresets.json`, o bien
   `pc_native/CMakeLists.txt` (PC) / `android_native/CMakeLists.txt` (Android).
5. Coloca los datos del juego en `out/pc-game` y ejecuta con `jugar_pc.bat` / `jugar_pc.ps1`.

## Estructura

| Carpeta | Contenido |
| --- | --- |
| `src/` | Código del programa: arranque, vídeo CRI, trazas, sondeo de frames |
| `generated/` | Código recompilado del juego — **se genera localmente, no se sube** |
| `tools/` | Utilidades: codegen, automatización en el teléfono, análisis |
| `pc_native/` | Build de Windows (misma base de SDK que Android) |
| `android_native/` | Build de Android |
| `assets/` | Tu dump del juego — **no se sube** |
