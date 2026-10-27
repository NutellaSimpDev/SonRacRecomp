# ESTADO DEL PROYECTO: Sonic & All-Stars Racing Transformed (SonRac)
**Fecha de actualización:** 20 de Octubre de 2026  
**Dispositivo objetivo primario:** Samsung Galaxy S25 (`SM-S931B`, Snapdragon 8 Elite, Adreno 830, Android 17, ADB ID: `RFCYA13NNXJ`)  
**Dispositivo objetivo secundario:** PC Windows (NVIDIA GeForce RTX 5060, Clang 23.1.3 + Ninja)  
**ID del Título Xbox 360:** `5345085D`  
**Nombre oficial de la APK:** **`SonRac`** (Package: `com.sonrac.game`)  
**Ícono de la aplicación:** Generado a partir de `C:\Users\EduPedu\Downloads\8841-sonic.png`  

---

## 0. ESTADO AL 20/10/2026 (noche) — EL JUEGO ARRANCA EN ANDROID

**Dispositivo de pruebas actual: nubia/RedMagic NX809J (Adreno 840), ADB `9125332100CB`.** Juego en
`/storage/emulated/0/Android/data/com.sonrac.game/files/assets` (carpeta creada por adb: hizo falta `chmod -R 777`).

- Intro con imagen y audio (~850 frames, ~6700 draws/frame). Va a tirones (compilación de shaders en caliente).
- Runtime: SDK ReXGlue **0.9.0** de `C:\Users\EduPedu\Desktop\LibertyRecompAndroid\glue\rexglue-sdk-main`
  (plugin GPU genérico `xenos`, `--gpu_plugin=xenos`). Compilación: `android_native/build_android.sh` (NDK r29).
  Todo en uno: `android_native/regen_and_build.sh` (codegen → `patch_init_h.py` → build → APK → install).
- **El código viejo (`generated_old_polluted/`) estaba roto**: ~95k funciones falsas en
  `sonicracingtransformed_config.toml`, 7478 parches `call; return;` y 375 stubs vacíos de imports del kernel
  (`src/missing_stubs.cpp`, ya fuera del build). Ni la versión PC pasaba de `main`.
- **Config nueva: `sonicracingtransformed_clean.toml`**: análisis propio de rexglue + destinos de saltos de cola
  (`add_tail_targets.py`), entry `0x824679C0`, destinos de thunks C++, punteros a código en .rdata/.data,
  direcciones `lis/addi` y **todos los trampolines de .xidata** (62,827; vtables/constructores apuntan a ellos).
- `patch_init_h.py`: el código se genera con el CLI 0.7.5; su `REX_CALL_INDIRECT_FUNC` saltaba a 0 en slots vacíos.
  Se sustituye por la versión 0.9.0 (cae a `ResolveIndirectFunction` y dice la dirección que falta).
- SDK de Liberty modificado: `REXGLUE_BUILD_GTA4_NATIVE` (opción), `REXGLUE_OUTPUT_DIR` (salida en
  `out/android-arm64/rex-out`), `seh_posix.cpp` encadena señales al handler previo, `SA_ONSTACK` en handlers.
- Diagnóstico: `src/android_crash_logger.h` (tag logcat `SonRacCrash`; los `code 2` son trampas de escritura normales).
  El nubia traía el logcat silenciado: Opciones de desarrollador → tamaño del búfer del registrador ≠ Desactivado.
- Mando táctil: `VirtualPadView.kt` → JNI `nativeSetPadState` (`src/android_virtual_pad.cpp`) → gamepad virtual SDL
  (Xbox 360). Se oculta si hay un mando físico.
- `args.txt` en `files/` añade cvars (reemplaza los por defecto). `--log_level=debug` frena mucho el juego.

**Rendimiento (medido en el nubia, driver Qualcomm del sistema; Turnip descartado: no funcionaba en el S25):**
menús ~25 FPS, carrera ~7–9 FPS (~2000 draws, 35 resolves/frame), CPU casi libre → límite en la GPU.
Núcleos a ~100 °C en carrera. Causa de fondo: Sonic usa el plugin `xenos` (emulación completa de la GPU de
Xbox 360, EDRAM y resolves), mientras GTA IV en Liberty usa un renderizador nativo (`gta4_native`).
Aplicado sin pruebas, desde Liberty: ADPF (`src/android_power.cpp`, sin la parte de distancia de dibujado de GTA;
loguea headroom y temperatura de GPU cada 10 s en logcat `SonRac`), `-O3` + ThinLTO en libmain,
`--audio_maxqframes=64` (el audio se quedaba sin bloques), `--readback_resolve=fast` +
`--vulkan_readback_resolve=true` (sin ellos la escena 3D salía blanca: exposición automática),
pausa por calor (`getThermalHeadroom` ≥ 0.9 y estado térmico severo) y modo de rendimiento sostenido.
Pendiente: perfil de GPU/CPU en carrera (posible espera activa en `SlRenderer`, ~70% de un núcleo), coche y
pista muy oscuros (iluminación), y valorar un renderizador nativo para Sonic.

## 1. RESUMEN EJECUTIVO PARA CLAUDE

Este proyecto consiste en la **recompilación estática nativa (Static Recompilation)** del juego de Xbox 360 *Sonic & All-Stars Racing Transformed* hacia **Android (ARM64-v8a)** y **PC (x86_64)** utilizando el framework ReXGlue / Xenia Recomp.

El usuario solicitó explícitamente:
1. **La aplicación NO puede llamarse Sonic:** Se llama estrictamente **`SonRac`**.
2. **Ícono personalizado:** Utiliza el avatar oficial de Sonic [`8841-sonic.png`](file:///C:/Users/EduPedu/Downloads/8841-sonic.png).
3. **Menú previo / Launcher integrado con estética de Sonic Racing Transformed:**
   - Opciones con bordes cian neón, degradados azul sónico, acentos en oro y tarjetas de fibra de carbono.
   - Soporte para cargar el dump en `.iso` (con extractor nativo de Xbox 360 XDVDFS/XGD3) o seleccionar la carpeta de archivos extraídos.
   - Selector de resolución (720p, 1080p, 1440p y nativa 2340x1080 del S25).
   - Selector de drivers Vulkan (Adreno del sistema o drivers personalizados Turnip/Mesa `.zip`).
   - Soporte para mandos Bluetooth/USB y **Virtual Pad en pantalla táctil con la distribución exacta de Xbox 360** (A, B, X, Y con colores oficiales, gatillos LT/RT, stick analógico y botones de sistema).
   - **Respuesta háptica vibratoria:** Cada pulsación del mando táctil emite vibración física mediante la API háptica de Android.

---

## 2. ESTADO ACTUAL Y PROGRESO REALIZADO

### A. Versión PC (`build-clang/sonicracingtransformed.exe`)
- **Estado:** Compilado y enlazado con éxito (Binario de ~155 MB).
- **Parches aplicados:** Se eliminaron los 26,918 fatal traps y ramas no resueltas usando [`patch_all_remaining_fatals.py`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/patch_all_remaining_fatals.py) y [`patch_unresolved_branches.py`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/patch_unresolved_branches.py).
- **Menú de configuración PC:** Implementado en [`src/sonic_config_dialog.h`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/src/sonic_config_dialog.h), accesible al inicio o pulsando `F1`. Permite cambiar resolución, VSync y testear mandos SDL3 en tiempo real.
- **Punto de detención actual en PC:** En tiempo de ejecución se genera un fallo en una llamada no resuelta:
  ```
  [critical] [core] [FATAL] Unresolved call from 0x82CA6090 to 0x82CA5FD8
  ```
  Requiere mapear o stubbear esta función guest en `sonicracingtransformed_config.toml`.

### B. Aplicación Android (`android/` -> `app-debug.apk`)
- **Estructura del Proyecto:**
  - `android/app/src/main/java/com/sonrac/game/`
    - [`LauncherActivity.kt`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/android/app/src/main/java/com/sonrac/game/LauncherActivity.kt): Launcher principal y pantalla de ajustes.
    - [`GameActivity.kt`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/android/app/src/main/java/com/sonrac/game/GameActivity.kt): Actividad de juego en pantalla completa con `SurfaceView` Vulkan y `VirtualPadView`.
    - [`VirtualPadView.kt`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/android/app/src/main/java/com/sonrac/game/VirtualPadView.kt): Pad táctil con distribución Xbox 360, vibración por hardware (`Vibrator` / `VibrationEffect`) y renderizado canvas vectorial.
    - [`IsoExtractor.kt`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/android/app/src/main/java/com/sonrac/game/IsoExtractor.kt): Parser y extractor directo de imágenes ISO Xbox 360 (particiones XDVDFS).
  - Íconos generados en `android/app/src/main/res/mipmap-*` para todas las densidades de pantalla.
- **Instalación:**
  - Compilado con Gradle 9.6.0 + OpenJDK 21.
  - Instalado en el dispositivo físico Samsung Galaxy S25 (`RFCYA13NNXJ`).

### C. Transferencia de Assets del Juego al S25
- Se transfirieron exitosamente **4.9 GB** de archivos del juego al almacenamiento del teléfono vía `adb push`:
  - **Ruta en Android:** `/storage/emulated/0/Android/data/com.sonrac.game/files/assets/`
  - **Contenido verificado:**
    - `default.xex` (18.7 MB, ejecutable decompreso)
    - `AvatarAssetPack`, `nxeart`
    - `Data/Frontend.M00` (400 MB), `Frontend.toc`
    - `Data/GameAssets.M00` (3.1 GB), `GameAssets.toc`
    - `Data/GameData.M00`, `GameData.toc`
    - `Data/Movies/` (`credits.usm`, `Drifting.usm`, `Gameclear.usm`, `Logos.usm`)
    - `Data/sound/` (`SoundBanks.pck`, `StreamsEng.pck`, `StreamsSpa.pck`, `StreamsMulti.pck`, etc.)

---

## 3. ¿QUÉ OCURRIÓ AL PULSAR "INICIAR CARRERA"? (Diagnóstico del Cierre)

El usuario reportó: **"cuando le doy a iniciar, se cierra"**.

> **RESUELTO (20/10/2026):** El logcat real mostró `NullPointerException ... DecorView.getWindowInsetsController()`.
> `GameActivity.onCreate()` llamaba a `hideSystemUI()` **antes** de `setContentView()`, cuando la ventana aún no tenía DecorView.
> Se movió la llamada después de `setContentView()` y se re-oculta la UI en `onWindowFocusChanged`. Verificado en el S25:
> `GameActivity` queda abierta mostrando el pad virtual (pantalla negra porque aún no hay motor nativo — Prioridad 2).
> El diagnóstico original de abajo (tema / librería nativa / exported) era incorrecto y se conserva solo como historial.

### Causa raíz:
1. **Actualmente `GameActivity` es un contenedor UI en Kotlin:**
   `GameActivity` inicializa el `SurfaceView` y el `VirtualPadView`, pero **aún no se ha compilado ni enlazado la librería nativa C++ (`libsonrac.so` o `librexglue.so`) para Android (ARM64-v8a)** que ejecuta el motor de juego real.
2. **Permisos y ciclo de vida de la Activity:**
   Al lanzar `GameActivity`, si hay un crash de tema en `Theme.SonRac.Fullscreen` o si la superficie no tiene backend nativo que responda, Android devuelve la app al Launcher o genera un cierre forzado. Además, `GameActivity` estaba marcada como `android:exported="false"`.
3. **Falta de integración JNI / NDK:**
   El código recompilado (`generated/`) y el runtime ReXGlue actualmente están construidos para Windows `x86_64` con Clang. Para que el juego corra en el Galaxy S25, deben compilarse con el **Android NDK r27** apuntando a `target: aarch64-linux-android` con backend Vulkan.

---

## 4. LISTA EXACTA DE TAREAS PENDIENTES (ROADMAP PARA CLAUDE)

### Prioridad 1: Arreglar el cierre inmediato de `GameActivity`
1. **Verificar y corregir el inicio de `GameActivity`:**
   - Revisar `Theme.SonRac.Fullscreen` en `android/app/src/main/res/values/themes.xml` para asegurar que herede de `Theme.MaterialComponents.DayNight.NoActionBar` sin atributos conflictivos de ventana.
   - En `LauncherActivity.kt`, asegurar que el `Intent` a `GameActivity` no falle por extras nulos.
   - Agregar bloques `try-catch` y logs detallados (`Log.e("SonRac", ...)`) en `GameActivity.onCreate()`, `hideSystemUI()` y `surfaceCreated()`.
   - Modificar `AndroidManifest.xml` para asegurar que `GameActivity` tenga configurado `android:exported="true"` o `false` correctamente con sus flags de orientación y resize.
2. **Actualizar el layout del Launcher:**
   - Aplicar los cambios en [`activity_launcher.xml`](file:///C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project/android/app/src/main/res/layout/activity_launcher.xml) usando `AppCompatButton` con `paddingVertical="4dp"` para que los textos de los botones no se recorten en pantallas de alta densidad.

### Prioridad 2: Compilación Nativa NDK (ARM64-v8a) para Android
1. **Configurar CMake en el proyecto Android:**
   - Configurar `externalNativeBuild` en `android/app/build.gradle.kts` apuntando al NDK r27:
     ```
     NDK Path: C:\Users\EduPedu\AppData\Local\Android\Sdk\ndk\27.0.12077973
     ABI: arm64-v8a
     ```
2. **Crear el puente JNI (`sonrac_jni.cpp`):**
   - Implementar funciones nativas:
     - `nativeInit(String assetDir, Surface surface, int width, int height)`
     - `nativeSendInput(int buttons, float leftTrigger, float rightTrigger, float thumbLX, float thumbLY)`
     - `nativePause()`, `nativeResume()`, `nativeDestroy()`
   - Conectar `ANativeWindow_fromSurface(env, surface)` al backend Vulkan de ReXGlue.
3. **Cross-compilar ReXGlue + Código Recompilado:**
   - Compilar las unidades de código estático de `generated/` hacia una librería compartida `libsonrac.so`.
   - Enlazar con `libvulkan.so`, `libandroid.so`, `liblog.so` y `libaaudio.so` / `libOpenSLES.so` para audio.

### Prioridad 3: Resolver la llamada no resuelta en PC
1. **Analizar la dirección `0x82CA6090 -> 0x82CA5FD8`:**
   - Ejecutar script de análisis sobre `sonicracingtransformed.xex` / `default.xex` para identificar qué función guest reside en `0x82CA5FD8`.
   - Agregar el stub correspondiente en `sonicracingtransformed_config.toml` o en `src/stubs.cpp` para permitir que el juego pase a la secuencia de intro y menús en PC.

### Prioridad 4: Mapeo del Pad Virtual a Input Guest de Xbox 360
1. Conectar las llamadas de `VirtualPadView.kt` al callback de `GameActivity`:
   - Mapear botones táctiles a las constantes de `XINPUT_GAMEPAD`:
     - `XINPUT_GAMEPAD_A = 0x1000`
     - `XINPUT_GAMEPAD_B = 0x2000`
     - `XINPUT_GAMEPAD_X = 0x4000`
     - `XINPUT_GAMEPAD_Y = 0x8000`
     - `XINPUT_GAMEPAD_LEFT_SHOULDER = 0x0100`
     - `XINPUT_GAMEPAD_RIGHT_SHOULDER = 0x0200`
     - `XINPUT_GAMEPAD_START = 0x0010`
     - `XINPUT_GAMEPAD_BACK = 0x0020`
   - Mapear `thumbLX` y `thumbLY` a valores normalizados de `[-32768, 32767]`.
   - Mapear `leftTrigger` y `rightTrigger` a `[0, 255]`.

---

## 5. RUTAS Y ARCHIVOS CLAVE

| Componente | Ruta Absoluta |
|---|---|
| **Proyecto Raíz** | `C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project` |
| **Proyecto Android** | `C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project\android` |
| **APK Debug** | `android/app/build/outputs/apk/debug/app-debug.apk` |
| **Binario PC** | `build-clang/sonicracingtransformed.exe` |
| **Assets en PC** | `C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed\Data` |
| **Assets en Galaxy S25** | `/storage/emulated/0/Android/data/com.sonrac.game/files/assets/` |
| **Ícono Sonic** | `C:\Users\EduPedu\Downloads\8841-sonic.png` |
| **Android NDK 27** | `C:\Users\EduPedu\AppData\Local\Android\Sdk\ndk\27.0.12077973` |
| **Android SDK / ADB** | `C:\Users\EduPedu\AppData\Local\Android\Sdk\platform-tools\adb.exe` |
| **JDK 21** | `C:\Users\EduPedu\.gradle\jdks\eclipse_adoptium-21-amd64-windows.2` |
