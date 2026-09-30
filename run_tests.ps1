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
    Invoke-Gcc -Arguments @('-x', 'c', '-fsyntax-only', 'wallship_types.h')
    Invoke-Gcc -Arguments @('-fstack-usage', '-c', 'test_wallship_ai.c', '-o', 'build/test_wallship_ai.o')
    Invoke-Gcc -Arguments @('build/test_wallship_ai.o', '-o', 'build/test_wallship_ai.exe')
    Invoke-Gcc -Arguments @('test_header_scoring.c', '-o', 'build/test_header_scoring.exe')
    Invoke-Gcc -Arguments @('test_game_rules.c', '-o', 'build/test_game_rules.exe')
    Invoke-Gcc -Arguments @('-DMAX_SHIP_PLACEMENT_ATTEMPTS=0',
                 'test_placement_failure.c', '-o', 'build/test_placement_failure.exe')
    Invoke-Gcc -Arguments @('-fstack-usage', '-c', 'examples/wallship_game.c', '-o', 'build/wallship_game.o')
    Invoke-Gcc -Arguments @('build/wallship_game.o', 'test_wallship_game.c',
                 '-o', 'build/test_wallship_game.exe')
    Invoke-Gcc -Arguments @('wallship_play.c', '-o', 'build/wallship_play_test.exe')
    Invoke-Test '.\build\test_wallship_ai.exe'
    Invoke-Test '.\build\test_header_scoring.exe'
    Invoke-Test '.\build\test_game_rules.exe'
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
    $winningShots = @('0 0', '1 0', '2 0', '3 0', '3 1', '3 2',
                      '7 1', '7 2', '7 3', '1 6', '6 6', '7 6')
    $victory = $winningShots | & .\build\wallship_play_test.exe balanced 123
    if ($LASTEXITCODE -ne 0 -or
        @($victory | Where-Object { $_ -eq 'You win!' }).Count -ne 1 -or
        @($victory | Where-Object { $_ -match 'You fired at .*: HIT' }).Count -ne 6 -or
        @($victory | Where-Object { $_ -match 'You fired at .*: SUNK' }).Count -ne 6) {
        throw 'Interactive HIT/SUNK victory regression failed'
    }
    Write-Output 'Interactive HIT/SUNK victory passed.'
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
    Get-Content -LiteralPath build/test_wallship_ai.su, build/wallship_game.su
} finally {
    Pop-Location
}
