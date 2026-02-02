# GitHub Copilot Refund Request

**Date**: January 7, 2026
**Account Holder**: Johannes Wilms
**GitHub Username**: drJanW
**Email**: janwilms@gmail.com
**Plan**: GitHub Copilot Pro+ Individual ($39.00/month)

---

## Request Summary

I am requesting a refund of **$60.00** for unproductive AI usage caused by repetitive Copilot failures on a single bug fix.

---

## Current Billing (as of January 7, 2026)

| Item | Amount |
|------|--------|
| Gross metered usage (Jan 1-7) | $100.00 |
| Included usage discount | $59.92 |
| Net overage | ~$40.00 |

In just 7 days, I have consumed nearly my entire monthly allowance plus overage.

---

## Incident Description

**Dates**: January 3-6, 2026 (4 days)
**Task**: Fix a brightness slider in an ESP32 embedded project
**Actual fix required**: Comment out 1 line (`#define DISABLE_SHIFTS`)

### What Happened

Copilot spent 4 days and an estimated $60+ in tokens failing to fix a simple slider bug:
1. **Root cause was trivial**: A debug flag `DISABLE_SHIFTS` was left enabled in `HWconfig.h`, bypassing all brightness calculations. One line comment fix.
2. **AI blind spot**: Copilot searched for calculation errors but never searched for `#ifdef`, `DISABLE_*`, or guard conditions that skip code entirely.
3. **Repeated identical mistakes**: The AI was corrected multiple times for the same errors but continued making them:
   - Inventing terminology not in the project glossary
   - Ignoring explicit rules documented in the project
   - Making the same mapping errors after being shown the correct formula
   - Running commands without permission

4. **Token multiplier**: Each mistake required 3x tokens to correct (re-prompts, force-feeding context, re-explaining).

---

## Evidence

I maintain a mistake log in my project documenting AI failures:
**File**: `docs/mistake_excuses.txt` (802 lines)
**Repository**: Private ESP32 project (can provide excerpts on request)

### Key Logged Entries (timestamped)

```
[2026-01-05 ~21:00] NO NAMING DISCIPLINE - INVENTED TERMS WITHOUT GLOSSARY
Result: 4 days, $150+ tokens debugging because sloppy terminology led to sloppy code.

[2026-01-05 ~16:00] BLIND TO OWN DEBUG FLAGS - 3 DAYS WASTED
Description: DISABLE_SHIFTS was uncommented in HWconfig.h line 52. This debug flag caused setBrightnessShiftedHi() to be skipped entirely. Copilot spent 3 days debugging correct code, never searched for guards/flags.
Result: 3 days of token burn. One line comment fix.

[2026-01-05 ~14:00] TUNED PARAMETER WITHOUT DATA - BROKE LUX MAPPING
Description: Changed luxMaxLux: 800 → 52 based on ONE test, broke entire feature.

[2026-01-05 ~21:30] STILL INVENTING NAMES AFTER BEING CORRECTED
Result: More wasted tokens correcting the same mistake twice in 10 minutes.
```

### Additional Documentation

- `docs/brightness_slider_postmortem.md` - Technical analysis of the bug
- `docs/glossary_slider_semantics.md` - Terminology rules that were ignored
- `docs/rules.txt` - Project rules that were repeatedly violated

---

## Why This Warrants a Refund

1. **No productive output**: 4 days of AI interaction produced no working solution. The fix was found manually.
2. **AI loop behavior**: The same mistakes were made repeatedly despite explicit corrections, burning tokens unproductively.
3. **Documented pattern**: This is not a vague complaint—I have timestamped logs showing the failure pattern.
4. **Disproportionate cost**: A 1-line fix should not cost $60+ in AI tokens.

---

## Requested Resolution

- **Amount**: $60.00 (refund or billing credit)
- **Basis**: Unproductive AI loops on January 3-6, 2026

I am happy to provide additional documentation, log excerpts, or git history if needed.

---

## Contact

- **Email**: janwilms@gmail.com
- **GitHub**: @drJanW

Thank you for reviewing this request.
