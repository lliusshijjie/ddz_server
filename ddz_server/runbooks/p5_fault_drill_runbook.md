# P5 Fault Drill Runbook

## Covered Drills

1. Storage transaction failure rollback
2. Session/token invalidation handling
3. Illegal player action rejection

## Automated Test

```bash
ctest -C Debug --test-dir build -R p5_fault_drills --output-on-failure
```

## Expected Results

- Transaction failpoint does not corrupt coin balances or room state.
- Expired token returns `TOKEN_EXPIRED`; missing session returns `NOT_LOGIN`.
- Out-of-turn and invalid-combo actions are rejected without state corruption.

