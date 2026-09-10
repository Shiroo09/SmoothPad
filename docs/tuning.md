# Tuning

Units: finger speed is **mm/s** on the physical pad. Gain is a scalar applied to the 2D delta so X/Y never use different curves (no diagonal distortion).

## Curve

Piecewise smooth Hermite / smoothstep:

- below `curveStartMmS` → `lowGain` (precision)
- `curveStart`–`curveMid` → rise to `midGain`
- `curveMid`–`curveEnd` → rise to `maxGain`
- above `curveEnd` → clamp `maxGain`

UI sliders remap into these without exposing raw names in Basic Settings.

| Slider | Maps to |
| --- | --- |
| Pointer Speed | overall scale on all gains |
| Acceleration | high-speed gain + curve shape |
| Smoothness | One Euro / EMA strength; high speed reduces lag |
| Precision | low-speed gain + noise threshold |

## Profiles

| Profile | Intent |
| --- | --- |
| Mac Balanced | default: precise slow, usable swipe, light One Euro |
| Mac Smooth | more low-speed rejection, still low latency |
| Mac Fast | higher max gain, precision kept at low speed |
| Windows Native | real bypass (filter pass-through, engine idle) |

Values are perceptual starting points, **not** “Apple exact”.

## Smoothing

Default One Euro (`minCutoff`, `beta`, `dCutoff`). Fast motion lowers effective strength. Pointer **inertia is forbidden**: zero input delta ⇒ zero output; lift ⇒ stop.

## Prediction / resampling

Default off. Prediction 0–4 ms experimental. No fake input at display refresh.

## User-feedback map

| Complaint | First knobs |
| --- | --- |
| Slow movement feels heavy | lowGain / precision / smoothness |
| Fast swipe goes too far | maxGain / acceleration |
| Takes too long to accelerate | curveStart / shape |
| Jitter at slow speed | noise threshold / minCutoff |
| Feels delayed | smoothness / prediction / velocity window |
| Jump after lifting | contact reset (bug, not a slider) |

## Calibration

Wizard records still / slow / normal / swipe / full-pad traces and writes noise floor + speed breakpoints. Skip keeps Balanced defaults.
