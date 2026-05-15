# P5 Perf Baseline

## Command

```powershell
tools/perf/run_stress.ps1 -BuildDir build -Config Debug -SoftThresholdEnabled $true
```

## Current Soft Threshold

- `SoftThresholdSeconds=5`
- Behavior: warn only (`PERF_WARN`), does not fail CI.

## Notes

- Baseline output file: `build/artifacts/perf/stress_result.json`
- Suggested next step: collect 2-3 weeks of CI data before enabling hard fail.

