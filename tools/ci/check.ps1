param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

Write-Host "[CI-CHECK] configure..."
cmake -S . -B $BuildDir

Write-Host "[CI-CHECK] build..."
cmake --build $BuildDir --config $Config

Write-Host "[CI-CHECK] test..."
ctest -C $Config --test-dir $BuildDir --output-on-failure

Write-Host "[CI-CHECK] run perf baseline..."
powershell -ExecutionPolicy Bypass -File "tools/perf/run_stress.ps1" -BuildDir $BuildDir -Config $Config -SoftThresholdEnabled $true

Write-Host "[CI-CHECK] done"

