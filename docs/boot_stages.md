# Boot Stages Design

## Status64 Gating

Each 4-bit field in status64:
- **0** = OK → can use
- **1-14** = retrying → cannot use
- **15** = failed → cannot use

Gating doesn't care WHY it's not OK. Only that it's not OK.

## Stages (descriptive, not code)

| Stage | Name | Description | Exit condition |
|-------|------|-------------|----------------|
| 0 | INIT | RNG, I2C, GPIO - no external deps | Always passes |
| 1 | PROBE | First attempt each component | All components tried once |
| 2 | RETRY | Components retrying with growing interval | All retry counts exhausted OR all OK |
| 3 | STABLE | status64 frozen, no more changes | Permanent |

## Output Gating

| Output | Required Components | Available from Stage |
|--------|---------------------|----------------------|
| Heartbeat LED | none | 0 (INIT) |
| RGB patterns | SC_SD | 2 (RETRY) |
| TTS | SC_WIFI + SC_AUDIO | 2 (RETRY) |
| MP3 words | SC_SD + SC_AUDIO | 2 (RETRY) |
| Fragment | SC_SD + SC_AUDIO | 2 (RETRY) |
| PCM ping | SC_SD + SC_AUDIO | 2 (RETRY) |
| Fetch | SC_WIFI | 2 (RETRY) |
| Web interface | SC_WIFI + SC_SD | 2 (RETRY) |

Outputs become available when:
1. Stage >= 2 (all components probed at least once)
2. Required components are OK (field = 0)

## Stage Transitions

| Transition | Trigger | Action |
|------------|---------|--------|
| 0→1 | After I2C init | Start component probes |
| 1→2 | All components probed once | Queue welkom, enable gating checks |
| 2→3 | All retries exhausted | status64 frozen |

## Implementation Notes

See [plan_boot_stages_impl.md](plan_boot_stages_impl.md) for code changes.
