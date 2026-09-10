# Hardware-only validation

The agent environment has Visual Studio and the Windows SDK but **no WDK load** and cannot feel a physical touchpad. These checks remain for a real laptop:

1. `SmoothPad.Cli.exe inspect` lists the built-in PTP with plausible mm size.
2. `record` + `replay` of still / slow / swipe traces.
3. Mac Mode **must not** enable if the filter is absent (Degraded). Confirm no double movement.
4. With a **contributor-built** test-signed filter in a VM (not a GitHub Release asset): pass-through indistinguishable from stock; Custom + kill SmoothPad.exe restores native within 250 ms. See [byo-driver.md](byo-driver.md).
5. External USB mouse + Raw Accel: ON vs OFF movement stats unchanged.
6. Two-finger scroll, tap, click, 1→2 and 2→1: no cursor jump.
7. Sleep/resume, lock/unlock, Fn disable, UAC secure desktop.
8. Uninstall leaves a working native pad.
9. Display DPI 100–200% and 60–240 Hz: no resampling unless a bench says otherwise.
10. Confirm `CreateSyntheticPointerDevice2` on the target Windows 11 build (`inspect` prints availability).
