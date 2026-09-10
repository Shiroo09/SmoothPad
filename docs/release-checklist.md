# Release checklist

1. Bump `Version.h`, `CHANGELOG.md`, MSIX identity version.
2. `.\scripts\test.ps1` (x64 Release).
3. `SmoothPad.Cli.exe bench` — median process << 2 ms.
4. If filter **source** changed: VM notes in the PR. **Do not** attach an unsigned `.sys` to the GitHub release. **Do not** ship test-signed as Stable.
5. `.\scripts\package.ps1 -Version x.y.z`
6. Sign with `scripts/sign.ps1` when `SMOOTHPAD_SIGNING_PFX` is available; otherwise label artifacts **unsigned development**.
7. SHA256SUMS in `out/msix/`.
8. GitHub release on tag `vX.Y.Z` (workflow `release.yml`).
9. Store: see `docs/store/`.
10. Rollback: previous MSIX / installer; filter uninstall script; native pad must work with SmoothPad killed.
