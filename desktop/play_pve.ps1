$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
$playExe = Join-Path $PSScriptRoot "..\build\wallship_pve_$PID.exe"
$gameExitCode = 1
try {
    New-Item -ItemType Directory -Path ..\build -Force | Out-Null
    & gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror -I. -I..\core wallship_pve.c -o $playExe
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed with exit code $LASTEXITCODE" }
    & $playExe @args
    $gameExitCode = $LASTEXITCODE
} finally {
    if (Test-Path -LiteralPath $playExe) {
        Remove-Item -LiteralPath $playExe -Force
    }
    Pop-Location
}
exit $gameExitCode