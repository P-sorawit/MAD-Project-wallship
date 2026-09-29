$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $flags = @('-std=c99', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion',
               '-Wshadow', '-Wstrict-prototypes', '-Werror', '-I.')
    function Invoke-Gcc {
        param([string[]] $Arguments)
        & gcc @flags @Arguments
        if ($LASTEXITCODE -ne 0) { throw "gcc failed with exit code $LASTEXITCODE" }
    }
    function Invoke-Test {
        param([string] $Path)
        & $Path
        if ($LASTEXITCODE -ne 0) { throw "$Path failed with exit code $LASTEXITCODE" }
    }
    Invoke-Gcc -Arguments @('-fstack-usage', '-c', 'wallship_ai.c', '-o', 'build/wallship_ai.o')
    Invoke-Gcc -Arguments @('build/wallship_ai.o', 'test_wallship_ai.c', '-o', 'build/test_wallship_ai.exe')
    Invoke-Gcc -Arguments @('-DMAX_SHIP_PLACEMENT_ATTEMPTS=0', 'wallship_ai.c',
                 'test_placement_failure.c', '-o', 'build/test_placement_failure.exe')
    Invoke-Gcc -Arguments @('-fstack-usage', '-c', 'examples/wallship_game.c', '-o', 'build/wallship_game.o')
    Invoke-Gcc -Arguments @('build/wallship_ai.o', 'build/wallship_game.o', 'test_wallship_game.c',
                 '-o', 'build/test_wallship_game.exe')
    Invoke-Gcc -Arguments @('build/wallship_ai.o', 'wallship_play.c', '-o', 'build/wallship_play_test.exe')
    Invoke-Test '.\build\test_wallship_ai.exe'
    Invoke-Test '.\build\test_placement_failure.exe'
    Invoke-Test '.\build\test_wallship_game.exe'
    $demo = @('bad', '8 6', '0 0', '0 0', 'q') | & .\build\wallship_play_test.exe balanced 123
    if ($LASTEXITCODE -ne 0 -or
        -not ($demo -match 'Use x=0\.\.7 and y=0\.\.6').Count -or
        -not ($demo -match 'already attacked that cell').Count -or
        -not ($demo -match 'Game ended by player').Count) {
        throw 'Interactive input smoke test failed'
    }
    $moves = for ($y = 0; $y -lt 7; $y++) {
        for ($x = 0; $x -lt 8; $x++) { "$x $y" }
    }
    foreach ($profile in @('random', 'balanced', 'aggressive')) {
        $transcript = $moves | & .\build\wallship_play_test.exe $profile 123
        if ($LASTEXITCODE -ne 0 -or
            @($transcript | Where-Object { $_ -eq 'You win!' -or $_ -eq 'AI wins!' }).Count -ne 1) {
            throw "Interactive game failed for profile $profile"
        }
        Write-Output "Interactive $profile game finished."
    }
    $first = @('q') | & .\build\wallship_play_test.exe balanced 123 --reveal
    $repeat = @('q') | & .\build\wallship_play_test.exe balanced 123 --reveal
    $different = @('q') | & .\build\wallship_play_test.exe balanced 124 --reveal
    $firstBoard = @($first | Where-Object { $_ -match '^ [0-6]  ' })
    $repeatBoard = @($repeat | Where-Object { $_ -match '^ [0-6]  ' })
    $differentBoard = @($different | Where-Object { $_ -match '^ [0-6]  ' })
    if ($firstBoard.Count -ne 7 -or $repeatBoard.Count -ne 7 -or
        $differentBoard.Count -ne 7 -or
        ($firstBoard -join "`n") -ne ($repeatBoard -join "`n") -or
        ($firstBoard -join "`n") -eq ($differentBoard -join "`n")) {
        throw 'Seed/reveal regression test failed'
    }
    Write-Output 'Reveal mode: same seed repeats; changed seed changes visible fleets.'
    Get-Content -LiteralPath build/wallship_ai.su, build/wallship_game.su
} finally {
    Pop-Location
}
