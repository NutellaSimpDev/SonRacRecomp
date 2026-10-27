@echo off
setlocal
cd /d "%~dp0out\pc-native"
set SONRAC_GPU_BACKEND=vulkan
echo ========================================================
echo   Iniciando Sonic & All-Stars Racing Transformed (PC - Vulkan)
echo ========================================================
start "" ".\sonicracingtransformed.exe" ^
  --gpu_plugin=xenos ^
  --game_data_root="%~dp0out\pc-game" ^
  --user_data_root="%~dp0out\pc-user-native" ^
  --guest_vblank_multiplier=1 ^
  --audio_pace_callback=true ^
  --snorm16_render_target_full_range=false ^
  --vulkan_readback_resolve=true ^
  --vulkan_native_depth_restore_copy=true ^
  --vulkan_dynamic_rendering=false ^
  --diagnostics=true ^
  --diagnostics_categories=logging
