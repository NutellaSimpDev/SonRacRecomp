# tools/mimo/probar.ps1 - compile, package, install and run SonRac with SHORT, filtered output.
param(
  [ValidateSet("todo", "compilar", "apk", "instalar", "correr", "pc", "pc_compilar")] [string]$Paso = "todo",
  [int]$Segundos = 25,
  [string[]]$Juego = @(
    "--guest_vblank_multiplier=1",
    "--sonrac_cri_audio_poll_us=100",
    "--audio_pace_callback=true"
  ),
  [string]$Filtro = "",
  [int]$Lineas = 35
)
$ErrorActionPreference = "Continue"
$Repo     = (Resolve-Path "$PSScriptRoot\..\..").Path
$Ninja    = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$Strip    = "C:\Users\EduPedu\AppData\Local\Android\Sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe"
$Adb      = "C:\Users\EduPedu\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$Pkg      = "com.sonrac.game"
$Device   = "9125332100CB"
$OutArm64 = "$Repo\out\android-arm64"
$OutPc    = "$Repo\out\pc-native"
$Logs     = "$Repo\out\mimo_logs"
New-Item -ItemType Directory -Force $Logs | Out-Null

function Fallo($msg) { Write-Output "FALLO: $msg"; exit 1 }

function Compilar {
  $t = Get-Date
  cmd /c "`"$Ninja`" -C `"$OutArm64`" SonRac > `"$Logs\ninja.txt`" 2>&1"
  $code = $LASTEXITCODE
  if ($code -ne 0) {
    Select-String -Path "$Logs\ninja.txt" -Pattern "error:|FAILED:|undefined reference" |
      Select-Object -First 20 | ForEach-Object { $_.Line.Trim() }
    Fallo "ninja exit=$code (log: $Logs\ninja.txt)"
  }
  Write-Output ("[compilar] OK en {0:N1} s" -f ((Get-Date) - $t).TotalSeconds)
  $jni = "$Repo\android\app\src\main\jniLibs\arm64-v8a"
  New-Item -ItemType Directory -Force $jni | Out-Null
  $libMain = "$OutArm64\libmain.so"
  if (-not (Test-Path $libMain)) {
    $found = Get-ChildItem -Path $OutArm64 -Filter "libmain.so" -Recurse | Select-Object -First 1
    if ($found) { $libMain = $found.FullName }
  }
  if (Test-Path $libMain) {
    Copy-Item $libMain "$jni\libmain.so" -Force
    if (Test-Path $Strip) {
      & $Strip --strip-unneeded "$jni\libmain.so" 2>&1 | Out-Null
    }
  } else {
    Fallo "libmain.so no encontrado en $OutArm64"
  }
}

function Apk {
  $env:JAVA_HOME = "C:\Users\EduPedu\.gradle\jdks\eclipse_adoptium-21-amd64-windows.2"
  if (-not (Test-Path $env:JAVA_HOME)) {
    $env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
  }
  $env:Path = "$env:JAVA_HOME\bin;$env:Path"
  Remove-Item Env:\NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue

  $gradleBat = Get-ChildItem -Path "$env:USERPROFILE\.gradle\wrapper\dists" -Filter "gradle.bat" -Recurse |
    Select-Object -ExpandProperty FullName -First 1
  if (-not $gradleBat) { Fallo "No se encontro gradle.bat en .gradle/wrapper/dists" }

  cmd /c "cd /d `"$Repo\android`" && `"$gradleBat`" assembleDebug --no-daemon -q < NUL > `"$Logs\gradle.txt`" 2>&1"
  $code = $LASTEXITCODE
  if ($code -ne 0) { Get-Content "$Logs\gradle.txt" -Tail 20; Fallo "gradle exit=$code (log: $Logs\gradle.txt)" }
  Write-Output "[apk] OK"
}

function Instalar {
  $apkPath = "$Repo\android\app\build\outputs\apk\debug\app-debug.apk"
  $r = & $Adb -s $Device install -r $apkPath 2>&1 | Out-String
  if ($r -notmatch "Success") { Fallo "adb install: $($r.Trim())" }
  Write-Output "[instalar] OK"
}

function Correr {
  $expanded = $Juego | ForEach-Object { $_ -split ',' } | Where-Object { $_ }
  $quoted = ($expanded | ForEach-Object { "'$_'" }) -join ' '
  & $Adb -s $Device shell "printf '%s\n' $quoted > /sdcard/Android/data/$Pkg/files/args.txt"
  & $Adb -s $Device shell am force-stop $Pkg
  & $Adb -s $Device logcat -c
  & $Adb -s $Device shell monkey -p $Pkg -c android.intent.category.LAUNCHER 1 | Out-Null
  Start-Sleep -Seconds 4
  Start-Sleep -Seconds $Segundos
  $gamePid = (& $Adb -s $Device shell pidof "${Pkg}:game") -as [string]
  $vivo = [bool]($gamePid -and $gamePid.Trim())
  $raw = "$Logs\logcat.txt"
  cmd /c "`"$Adb`" -s $Device logcat -d > `"$raw`" 2>&1"
  & $Adb -s $Device shell screencap -p /sdcard/sonrac_cap.png 2>&1 | Out-Null
  & $Adb -s $Device pull /sdcard/sonrac_cap.png "$Logs\sonrac_screen.png" 2>&1 | Out-Null
  $ruido = "DisplayManager|VelocityTracker|InputTransport|ART-GC|FuseDaemon|AHAL_|PAL:|NetworkPolicy|ProcessInternal|ObjName|NodeName"
  $base = "SonRac|\[nativo\]|Fatal signal|SIGSEGV|SIGABRT|backtrace|ShaderDump|AudioRate|\[error\]|VK_ERROR|FrameProbe|GpuTiming"
  $pat = if ($Filtro) { "$base|$Filtro" } else { $base }
  $sel = Select-String -Path $raw -Pattern $pat | Where-Object { $_.Line -notmatch $ruido } |
    ForEach-Object { $l = $_.Line; if ($l.Length -gt 220) { $l.Substring(0, 220) + "..." } else { $l } }
  Write-Output ("[correr] args: {0} | proceso :game vivo={1} | {2} lineas coinciden (mostrando ultimas {3}; crudo: {4})" -f ($Juego -join ' '), $vivo, @($sel).Count, $Lineas, $raw)
  $sel | Select-Object -Last $Lineas
}

function PcCompilar {
  $t = Get-Date
  cmd /c "`"$Ninja`" -C `"$OutPc`" sonicracingtransformed > `"$Logs\ninja_pc.txt`" 2>&1"
  $code = $LASTEXITCODE
  if ($code -ne 0) {
    Select-String -Path "$Logs\ninja_pc.txt" -Pattern "error:|FAILED:" |
      Select-Object -First 20 | ForEach-Object { $_.Line.Trim() }
    Fallo "ninja pc exit=$code"
  }
  Write-Output ("[pc compilar] OK en {0:N1} s" -f ((Get-Date) - $t).TotalSeconds)
}

function Pc {
  PcCompilar
  Push-Location $OutPc
  try {
    $argsPc = @(
      "--game_data_root=$Repo\out\pc-game",
      "--user_data_root=$Repo\out\pc-user-native",
      "--diagnostics=true",
      "--diagnostics_categories=logging"
    ) + ($Juego | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
    $argsString = ($argsPc | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
    Write-Output "[pc correr] args: $argsString"
    $p = Start-Process -FilePath ".\sonicracingtransformed.exe" -ArgumentList $argsString -PassThru
    Start-Sleep -Seconds $Segundos
    if (-not $p.HasExited) {
      Stop-Process -Id $p.Id -Force
      Write-Output "[pc correr] detenido tras $Segundos s (OK)"
    } else {
      Write-Output ("[pc correr] salio con codigo {0:X}" -f $p.ExitCode)
    }
  } finally {
    Pop-Location
  }
}

switch ($Paso) {
  "compilar"    { Compilar }
  "apk"         { Apk }
  "instalar"    { Instalar }
  "correr"      { Correr }
  "todo"        { Compilar; Apk; Instalar; Correr }
  "pc"          { Pc }
  "pc_compilar" { PcCompilar }
}
