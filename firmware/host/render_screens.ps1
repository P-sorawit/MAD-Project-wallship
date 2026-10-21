$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $out = '..\..\build\screens'
    New-Item -ItemType Directory -Path $out -Force | Out-Null
    $modules = @('app', '1_switch', '2_led', '3_game', '4_tft', '5_lcd', '6_ai', 'sound')
    $inc = @($modules | ForEach-Object { "-I..\$_" }) + @('-I..\..\core')
    $src = @('render_screens.c', 'host_stubs.c',
             '..\3_game\game.c', '..\3_game\placement.c', '..\6_ai\ai_player.c',
             '..\2_led\ws2812.c', '..\4_tft\gfx.c', '..\4_tft\font5x7.c', '..\4_tft\art.c', '..\4_tft\tft_ui.c')
    & gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow `
          -Wstrict-prototypes -Werror @inc @src -o ..\..\build\render_screens.exe
    if ($LASTEXITCODE -ne 0) { throw "gcc failed with exit code $LASTEXITCODE" }
    Push-Location $out
    try { & ..\render_screens.exe } finally { Pop-Location }
} finally {
    Pop-Location
}
