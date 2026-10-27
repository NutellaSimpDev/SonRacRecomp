# tools/latigo-mimo.ps1 - Delegate heavy compile/test/edit tasks to Xiaomi MiMo via Claude Code CLI.
param(
  [Parameter(Mandatory = $false)]
  [string]$Tarea = "Lee y ejecuta al pie de la letra ORDEN-ACTUAL.md en este directorio."
)
$ErrorActionPreference = "Continue"
$MainRepo = (Resolve-Path "$PSScriptRoot\..").Path
$ClaudeExe = "C:\Users\EduPedu\AppData\Roaming\npm\node_modules\@anthropic-ai\claude-code\bin\claude.exe"

# Resolve API Key
$ApiKey = $env:MIMO_API_KEY
if (-not $ApiKey) { $ApiKey = [Environment]::GetEnvironmentVariable("MIMO_API_KEY", "User") }
if (-not $ApiKey) { $ApiKey = [Environment]::GetEnvironmentVariable("MIMO_API_KEY", "Machine") }
if (-not $ApiKey -and (Test-Path "$MainRepo\.env")) {
  $line = Get-Content "$MainRepo\.env" | Where-Object { $_ -match '^\s*MIMO_API_KEY\s*=\s*(.+)$' } | Select-Object -First 1
  if ($line -match '^\s*MIMO_API_KEY\s*=\s*(.+)$') {
    $ApiKey = $Matches[1].Trim().Trim('"').Trim("'")
  }
}
if (-not $ApiKey) {
  Write-Output "ERROR: No se encontro MIMO_API_KEY."
  exit 1
}

Push-Location $MainRepo
try {
  $env:ANTHROPIC_BASE_URL             = "https://token-plan-sgp.xiaomimimo.com/anthropic"
  $env:ANTHROPIC_AUTH_TOKEN           = $ApiKey
  $env:ANTHROPIC_MODEL                = "mimo-v2.6-pro[1m]"
  $env:ANTHROPIC_DEFAULT_SONNET_MODEL = "mimo-v2.6-flash[1m]"
  $env:ANTHROPIC_DEFAULT_OPUS_MODEL   = "mimo-v2.6-pro[1m]"
  $env:ANTHROPIC_DEFAULT_HAIKU_MODEL  = "mimo-v2.6-flash[1m]"
  $env:CLAUDE_CODE_MAX_CONTEXT_TOKENS = "1000000"
  $env:CLAUDE_CODE_DISABLE_UNKNOWN_MODEL_WINDOW_ENFORCEMENT = "1"

  $Prompt = @"
Eres el obrero ejecutor de Antigravity trabajando en $MainRepo para el proyecto SonRac (Sonic & All-Stars Racing Transformed Android ARM64 y PC Native).
INSTRUCCION DEL ARQUITECTO JEFE:
$Tarea

REGLAS ESTRICTAS DE EJECUCION:
1. Modifica unicamente los archivos solicitados sin meter hacks que oculten errores.
2. Para compilar, empaquetar APK, instalar o probar en el RedMagic 11 Pro (com.sonrac.game) o PC, usa SIEMPRE:
   powershell -ExecutionPolicy Bypass -File tools/mimo/probar.ps1 -Paso <todo|compilar|apk|instalar|correr|pc>
   NO ejecutes ninja, gradlew ni adb logcat sin filtrar directamente.
3. Tienes maximo 4 intentos de compilacion/prueba.
4. CRITICO: Tu respuesta final debe tener MAXIMO 20 LINEAS resumiendo archivos tocados, resultado de compilacion/logs y diagnostico obtenido.
"@

  $Prompt | & $ClaudeExe -p --dangerously-skip-permissions --allowedTools "Bash,Edit,Write,Read,Glob,Grep"
} finally {
  Pop-Location
}
