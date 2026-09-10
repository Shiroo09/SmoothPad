# Third-party notices

SmoothPad source is MIT-licensed. The following projects were **studied as conceptual references**. Their source was **not copied** into this repository.

| Project | Author | License | URL | Reused code | Notes |
| --- | --- | --- | --- | --- | --- |
| emoacht/RawInput.Touchpad | emoacht | MIT (LICENSE.txt verified) | https://github.com/emoacht/RawInput.Touchpad | No | Conceptual: PTP discovery via Raw Input, Usage Page 0x0D / Usage 0x05, HidP_GetCaps / HidP_GetValueCaps / HidP_GetUsageValue, Contact ID / X / Y / Contact Count / Scan Time. Reimplemented in native C++23. |
| ClementGre/ThreeFingerDragOnWindows | Clément Grennerat | MIT (LICENSE verified) | https://github.com/ClementGre/ThreeFingerDragOnWindows | No | Conceptual: production PTP utility architecture, tray, WinUI packaging, RawInput.Touchpad integration. C# / WinUI; not reused. |
| tasos14/ScrollStrip | tasos14 | No LICENSE file in repo as of 2026-09 | https://github.com/tasos14/ScrollStrip | No | Conceptual: message-only window, Raw Input, HidP parsing, event timing. **Does not intercept native motion.** Inertia algorithm explicitly **not** used for pointer motion. |
| ZiMiaoWorkshop/Touchpad-Shield | ZiMiaoWorkshop | Apache-2.0 (LICENSE verified) | https://github.com/ZiMiaoWorkshop/Touchpad-Shield | No | Conceptual: WinUI 3 C++/WinRT tray app, SPI/touchpad settings, external-mouse policy. Apache-2.0 is compatible with MIT **attribution**; no source was copied. |
| jxleyo/PTP_HidFilter | jxleyo | No LICENSE file | https://github.com/jxleyo/PTP_HidFilter | No | Conceptual only. Not production quality. Not copied. |
| microsoft/Windows-driver-samples hid/firefly, input/moufiltr | Microsoft | MS-PL / sample license | https://github.com/microsoft/Windows-driver-samples | No | Conceptual KMDF filter / INF / PnP. Sample code is not treated as production-ready. |
| Microsoft Windows SDK / Win32 APIs | Microsoft | SDK EULA | https://learn.microsoft.com/windows/win32 | Headers/libs only | Raw Input, HID, SPI_GETTOUCHPADPARAMETERS, CreateSyntheticPointerDevice / CreateSyntheticPointerDevice2. |
| One Euro Filter | Géry Casiez et al. | Research paper / commonly implemented | https://cristal.univ-lille.fr/~casiez/1euro/ | Independent implementation | Signal-processing algorithm, not a source drop. |

GPL/copyleft source was not incorporated.

If you vendor any third-party file in a later commit, add a new row here **before merge**.
