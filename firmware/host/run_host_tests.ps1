$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path ..\..\build -Force | Out-Null
    $modules = @('app', '1_switch', '2_led', '3_game', '6_ai', 'sound')
    $inc = @($modules | ForEach-Object { "-I..\$_" }) + @('-I..\..\core')
    # HAL-free modules + PC stubs. HAL files (keypad.c, ws2812_port_stm32.c,
    # buzzer.c, app.c) need STM32CubeIDE.
    $src = @('host_test.c', 'host_stubs.c',
             '..\3_game\game.c', '..\3_game\placement.c', '..\6_ai\ai_player.c',
             '..\1_switch\keypad_core.c', '..\2_led\ws2812.c', '..\2_led\led_view.c')
    & gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow `
          -Wstrict-prototypes -Werror @inc @src -o ..\..\build\fw_host_test.exe
    if ($LASTEXITCODE -ne 0) { throw "gcc failed with exit code $LASTEXITCODE" }
    & ..\..\build\fw_host_test.exe
    if ($LASTEXITCODE -ne 0) { throw "host tests failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}
