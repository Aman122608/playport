# Epic update check without the sidecar refresh (PLA-69)

Commit `1f12076`. `EpicAccount.checkUpdate` stored the newest build only after
`refreshLaunchMetadata` succeeded, so a missing or corrupt receipt hid the Update button.
`EpicInstaller.updateCheck(_:asset:)` now returns the build and the refresh error separately;
the page records the build first and logs `sidecar refresh failed` on its own.

- Host: `EpicClientKitTests.testASidecarRefreshFailureStillReportsTheNewestBuild` (missing receipt,
  corrupt receipt, no record). `./pp test` and `--quick` pass.
- Phone: dev IPA sha256 `259651843bc5e676913e08d3404caec05801457d6934bb15ffc9af4c6aa9cba0`
  installed in place; `pp ui --action open:library --shot-each-action` showed the Library
  (206 titles, 34 Epic) loading. No Epic title had a pending update and the refresh failure cannot be
  forced from the UI (decision 0012), so the failure path is covered by the host test only.
