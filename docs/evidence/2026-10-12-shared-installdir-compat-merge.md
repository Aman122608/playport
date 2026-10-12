# Steam apps sharing an install folder; a compatibility default's keys under the player's own

**Date:** 2026-10-12. **Kind:** phone runs (dev app), host tests.
**Linear:** [PLA-192](https://linear.app/playportdev/issue/PLA-192) (shared install folder),
[PLA-123](https://linear.app/playportdev/issue/PLA-123) (compat default lost under the player's keys);
titles [PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike),
[PLA-25](https://linear.app/playportdev/issue/PLA-25) (Half-Life),
[PLA-52](https://linear.app/playportdev/issue/PLA-52) (Duck Paradox).
Decisions [0091](../decisions/0091-compat-default-keys-merge.md) and
[0092](../decisions/0092-steam-shared-install-folders.md).
**IPAs:** `Playport-26.5-ecb83c3b.ipa` (dev), sha256
`ecb83c3b57a59adfe8e4070503f22a3961673b34587d54016f0bbc74e4e97098`, built from ee2b96e: the
Counter-Strike and Half-Life runs and the first Duck Paradox run.
`Playport-26.5-88613c10.ipa` (dev), sha256
`88613c108b6cd548477615221a4f75aec41d19fcd38feadfb11616bbb3cdee21`, built from 8fa6792 (ee2b96e plus
the game page's subtitle text): the game page, the second Duck Paradox run, and the Hollow Knight and
Portal 2 gates. Both were installed in place. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

## Summary

- **Half-Life (70) installs into Counter-Strike's (10) `Half-Life` folder** beside it. Only the
  124 files Counter-Strike's depots do not list were downloaded (25 MB of 590 MB). Both games reach a
  map, and uninstalling Half-Life leaves Counter-Strike whole: it verifies clean and reaches de_dust2.
- **Half-Life's AccessDenied ownership ticket is expected.** The account owns no licence for app 70
  itself. Its depots come through another package. Nothing was changed for it.
- **Duck Paradox keeps its compatibility default** `env.MADEIRA_KEEP_SMC=1` with a player's own
  Runtime key `env.FOO=1`: the game gets both variables and reaches its menu.
- **Hollow Knight and Portal 2** reach first frame on the final IPA.

## Counter-Strike and Half-Life in one folder (0092)

Before: `install:70` stopped with `Games/Half-Life already exists and is not this app's Playport
install` while Counter-Strike was installed (`20261011T175814`).

- **`install:70`** (`20261012T065745`, IPA `ecb83c3b…`): `shared folder: Games/Half-Life also holds
  app(s) 10; 4364 of 4488 files are theirs too, 124 to download`, then
  `install: 124 files renamed into the shared Games/Half-Life; record written`. Depots 1, 2 and 3 used
  Counter-Strike's retained manifests (`retained copy`); 71 and 96 were fetched. No file conflicted:
  depots 71 and 96 override nothing in depots 1 to 3. The Downloads entry and the driver's line name
  it "Half-Life", taken from its PICS record, where they used to say "app 70".
- **Half-Life**, `--settings 'app-70:{"arguments":"-gl +map c1a0"}' --play app-70 --until
  first-frame+10 --shot` (`20261012T065835`): ok. The VC++ prerequisite ran and Playport restarted;
  then the game started at +5.07 s and drew its first frame at +5.45 s. The screenshot shows the tram
  corridor in c1a0 with "ANOMALOUS MATERIALS". No Cloud conflict this time.
- **Counter-Strike**, `--settings 'app-10:{"arguments":"-gl -full +map de_dust2"}' --play app-10`
  (`20261012T065958`): ok, first frame at +6.79 s. The screenshot shows de_dust2 with the buy-zone cart.
  The phone's 07:00 alarm banner covers the top left.
- **`uninstall:app-70`** (`20261012T070109`): `app 70: 124 of its 4488 files removed from the shared
  Games/Half-Life; the rest are app(s) 10's too`. The driver logs
  `folder present=true (shared) record present=false stage present=false catalogued=false`.
- **Counter-Strike after it** (`20261012T070149`): ok, first frame at +5.47 s, on de_dust2 again.
  **`--verify app-10`** (`20261012T070252`): `5611/5611 OK, 3 unlisted` (files the game wrote).

Counter-Strike stays installed (0.9 GB).

### Half-Life's ownership ticket

- The account's library is `107 licenses, 489 apps, 90 games`. App 70 is not among the owned apps:
  Downloads had no name for it, and the driver now reads the name from PICS.
- Depot selection uses the depots the account's packages list (`Library.ownedDepotIDs`). It kept
  Half-Life's own depots 71 and 96, and Steam gave their keys (`depot keys: 5 obtained`). So a package
  the account owns lists Half-Life's depots, but no licence grants app 70.
- Steam answers app 70's ticket requests with `ClientRequestEncryptedAppTicket 70: EResult
  AccessDenied` and `ClientGetAppOwnershipTicket 70: EResult AccessDenied`, as it did on 2026-10-11.
  That is the right answer for an app the account does not own, not a failure for an app sharing a
  package. Counter-Strike's ownership ticket (178 bytes) is issued. Half-Life runs with the emulated
  Steam API's own tickets. Not changed.

## A compatibility default under the player's keys (0091)

Duck Paradox (GOG 1816031813), installed through the UI (`install:gog-1816031813`,
`20261012T070422`, 1.0 GB), with `--settings 'gog-1816031813:{"runtime":"env.FOO=1"}'`:

- **`20261012T070516`** (IPA `ecb83c3b…`): the driver logs the effective
  `runtime=["env.FOO=1", "env.MADEIRA_KEEP_SMC=1"]`, and the launch logs
  `config: environment FOO=1` and `config: environment MADEIRA_KEEP_SMC=1`. The game started at
  +3.37 s, drew its first frame at +5.94 s and was alive at +10 s; the screenshot shows its main menu.
  Before, the player's text replaced the default's, which drops SMC tracking: the game then closes
  1.5 s after start (decision 0074's entry, PLA-52).
- **`20261012T071042`** (IPA `88613c10…`): the same, first frame at +5.79 s.
- **The game page** (`20261012T070941`, `open:gog-1816031813#developer` and seven `pad:down`): the
  Runtime keys row reads "None", with "ⓘ For this game: env.MADEIRA_KEEP_SMC=1; your own key…" under it.
- **Uninstalled** through the UI (`20261012T071143`).

## Gates on `88613c10…`

- **Hollow Knight** (`app-367520`, `20261012T071226`): `--until first-frame+10 --shot` ok, first frame
  at +10.19 s, the main menu on the screenshot.
- **Portal 2** (`app-620`, `20261012T071334`): ok, first frame at +6.33 s, the "powered by Source" intro.

## Host tests

`pp test --swift SteamClient` (187 tests) and `pp test --swift PlayportKit` (216 tests) pass,
including `testSharedInstallDirStagesOnlyOwnFilesAndRefusesAnotherAppsContent`,
`testSharedFolderInstallAndUninstallKeepTheOtherAppsFiles`,
`testSharedFolderUpdateRemovesOnlyItsOwnDroppedFiles`,
`testSharedFolderUninstallWithAMissingManifestKeepsEveryFile`,
`testANewInstallStillRefusesAFolderNoSteamRecordNames`,
`testSteamAppsSharingAnInstallFolderAreOneTitleEach` and
`testOwnKeysMergeWithTheCompatibilityDefaultsPerKey`. `pp test --quick` passes.

## Not covered

- **An update of a shared depot** (Valve moving depot 1) was not seen. Under 0092 it is refused while
  the other app keeps the old content (follow-up).
- **A kill part-way** through a shared commit or uninstall was not tested on the phone.
- **The Uninstall row of a shared title** still says "Frees" its whole size.
- No input was driven in any game.
