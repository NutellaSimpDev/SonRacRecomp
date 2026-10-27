@echo off
setlocal
cd /d "%~dp0out\pc-native"
set SONRAC_GPU_BACKEND=d3d12
echo ========================================================
echo   Iniciando Sonic & All-Stars Racing Transformed (PC - Direct3D 12)
echo ========================================================
start "" ".\sonicracingtransformed.exe" ^
  --gpu_plugin=xenos ^
  --game_data_root="%~dp0out\pc-game" ^
  --user_data_root="%~dp0out\pc-user-native" ^
  --guest_vblank_multiplier=1 ^
  --audio_pace_callback=true ^
  --d3d12_resolution_scale=1 ^
  --diagnostics=true ^
  --diagnostics_categories=logging
