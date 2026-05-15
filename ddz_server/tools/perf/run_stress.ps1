param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [bool]$SoftThresholdEnabled = $true,
    [int]$SoftThresholdSeconds = 5
)

$ErrorActionPreference = "Stop"

$start = Get-Date
ctest -C $Config --test-dir $BuildDir -R "p5_match_manager_stress|p5_full_flow_integration" --output-on-failure
$end = Get-Date

$elapsed = [int][Math]::Round(($end - $start).TotalSeconds)
$warn = $false
if ($SoftThresholdEnabled -and $elapsed -gt $SoftThresholdSeconds) {
    $warn = $true
    Write-Host "PERF_WARN: elapsed_seconds=$elapsed threshold_seconds=$SoftThresholdSeconds"
}

$artifactDir = Join-Path $BuildDir "artifacts/perf"
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
$result = [ordered]@{
    timestamp = (Get-Date).ToString("s")
    elapsed_seconds = $elapsed
    soft_threshold_enabled = $SoftThresholdEnabled
    soft_threshold_seconds = $SoftThresholdSeconds
    warn = $warn
}

$result | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $artifactDir "stress_result.json") -Encoding utf8
Write-Host "perf result saved to $artifactDir/stress_result.json"

