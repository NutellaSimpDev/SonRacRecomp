param(
  [switch]$CompilarPrimero = $true
)
$Repo = $PSScriptRoot
$OutPc = "$Repo\out\pc-native"
$Ninja = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

if ($CompilarPrimero) {
  Write-Host "[Compilando PC Native...]" -ForegroundColor Cyan
  cmd /c "`"$Ninja`" -C `"$OutPc`" sonicracingtransformed"
  if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error en compilacion Ninja PC]" -ForegroundColor Red
    exit 1
  }
}

Write-Host "[Lanzando Sonic Racing Transformed en PC...]" -ForegroundColor Green
Push-Location $OutPc
try {
  $env:SONRAC_GPU_BACKEND = "vulkan"
  & ".\sonicracingtransformed.exe" `
    "--gpu_plugin=xenos" `
    "--game_data_root=$Repo\out\pc-game" `
    "--user_data_root=$Repo\out\pc-user-native" `
    "--guest_vblank_multiplier=1" `
    "--audio_pace_callback=true" `
    "--readback_resolve=fast" `
    "--vulkan_readback_resolve=true" `
    "--vulkan_native_depth_restore_copy=true" `
    "--vulkan_dynamic_rendering=false" `
    "--diagnostics=true" `
    "--diagnostics_categories=logging"
} finally {
  Pop-Location
}
