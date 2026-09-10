# Win32 Store installer (driver edition)

When a kernel filter is required, ordinary MSIX cannot deploy it. Current Microsoft path (verify at submission):

```
Microsoft Store Win32 listing
        │
        ▼
versioned HTTPS offline installer (exe or msi)
        │
        ├── SmoothPad.exe
        └── WHCP-signed SmoothPadFilter package
```

Expected rules (re-check Partner Center):

- `.exe` or `.msi`, not an online stub
- HTTPS immutable versioned URL
- digitally signed PE
- silent install / uninstall
- UAC only for driver install
- installer only installs SmoothPad

Fail-safe: before upgrade/uninstall, set filter pass-through, stop the engine, then remove the binding. A failed install must leave the native pad working.

WiX is a reasonable MSI vehicle; a signed bootstrapper is acceptable if MSI cannot stage the driver package cleanly.

**External action required:** EV Authenticode cert, Hardware Dashboard, Store developer account, hosting for the immutable installer URL.
