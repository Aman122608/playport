# 2026-10-12: GOG install scripts and `.msi` redistributables (PLA-194)

**Linear:** [PLA-194](https://linear.app/playportdev/issue/PLA-194).
**Decision:** [0103](../decisions/0103-gog-install-script-and-msi.md).
**Question:** which of the owner's GOG titles need their install script, a generation-1 support
command or an `.msi` redistributable, and does Playport give them what GOG Galaxy would?
**Result:** the script's registry values are written from Play (Tomb Raider: Anniversary's seven
values reach the prefix); `.msi` redistributables now run through `msiexec`, host-tested only (the one
owned build that names one, Dishonored, was not installed); generation-1 support commands wait for
PLA-188. No owned title was found that needs its script to start: Tomb Raider: Anniversary stalls
before a window with and without its values, BioShock ends `c0000005` after its first window, and
Spelunky (whose script only runs its 0099 setup) reaches its language screen.

## Survey (workstation, GOG's open content-system endpoints, no sign-in)

The owner's 46 GOG titles (`agent-notes/speedrun/library.tsv`), each title's newest build manifest,
and the English and neutral depot manifests' file lists; script bodies read from the phone's installs
(`pp phone pull`).

- **Generation 2:** 45 titles. 43 builds say `scriptInterpreter: true`; NWN Diamond's and Spelunky's do
  not (both have a `temp_executable`, decision 0099). Builds with a `goggame-<id>.script` in a depot
  the survey read: A Plague Tale: Innocence, Baldur's Gate EE and II EE, BioShock, BioShock
  Remastered, Call of Juarez: Gunslinger, Control, Dishonored, En Garde!, Shadow of Mordor, RIOT,
  Spelunky, The Gunk, The Outer Worlds, Tomb Raider: Anniversary and Underworld. Most Unity titles
  (Coromon, Moonscars, Shogun Showdown, …) ship none, flag or not.
- **Smallest with registry values:** Tomb Raider: Anniversary (1202885143, 4.17 GB). RIOT's
  (2.39 GB) names a key with `saveGameData` and no value; Baldur's Gate EE's only a `savePath`.
- **Generation 1 only:** Quake II (Original), 1441704824: `support_commands` names
  `/galaxy_quake2_quad_damage_2.0.0.3.exe` and a DirectX redist. Not installable (PLA-188).
- **`.msi` redistributables in GOG's repository (2026-10-12):** `XNA` (`__redist/XNA/xnafx31_redist.msi /qn`),
  `XNA_40` (`xnafx40_redist.msi /qn`), `PHYSXLEGACY` (`PhysX-9.13.0604-SystemSoftware-Legacy.msi
  /passive /norestart`). Only Dishonored - Definitive Edition (1701063787, 18.0 GB) names one
  (`PHYSXLEGACY`, beside DirectX, MSVC2005/2008/2017 and PHYSX); no owned build names XNA.
- **Script actions seen:** `setRegistry`, `savePath`, `setIni` (BioShock), `installSDB` and
  `Execute` (Tomb Raider: Anniversary's `{supportDir}/add/gameux.cmd`, which writes two
  `HKCR\Local Settings\…\GameUX\ServiceLocation` values; BioShock's `setres.exe`; NWN Diamond's and
  Spelunky's setup). GOG's interpreter (`ISI`, `__redist/ISI/scriptinterpreter.exe`, 1,253,424 bytes)
  is an i386 Inno Setup package.
- **Tomb Raider: Anniversary's `tra.exe`** (i386) names `SOFTWARE\Crystal Dynamics\Tomb Raider:
  Anniversary`, `FullInstall` and `ExtendedDialog`: it reads the script's values.

## Phone

iPhone18,4, iOS 27.0. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

| Run | IPA | What | Result |
| --- | --- | --- | --- |
| `20261012T214538` | `884a68ab…` (0165c74, before) | install Spelunky and RIOT (`install:gog-…`) | installed |
| `20261012T214734` | `884a68ab…` | install Tomb Raider: Anniversary | installed, 4.2 GB |
| `20261012T215000` | `884a68ab…` | play TRA, `--until first-frame+30 --shot --wait 90` | no window, no frame; `tra.exe` alive, low CPU, an `mprotect_exec` loop |
| `20261012T220035` | `6194e3e1…` (0d9177a's tree) | play TRA | script values written (below); the same stall |
| `20261012T220340` | `6194e3e1…` | play TRA, screenshots at 25 and 60 s | black both; no `[guest-window]` line; `uxtheme.dll` loaded |
| `20261012T220749` | `6194e3e1…` | play Spelunky `--until first-frame+30 --shot` | 0099's setup exit 0 in 1 s; first frame 5.01 s; its language screen |
| `20261012T220912` | `6194e3e1…` | install BioShock (2022341186) | installed, 5.9 GB |
| `20261012T221337` | `d3424096…` (474934a) | play BioShock `--until first-frame+30 --shot` | MSVC2005 downloaded (2.7 MB) and run, exit 0; first window 4.78 s; `c0000005` at 14 s, no frame |
| `20261012T221443` | `d3424096…` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame 9.98 s, main menu |
| `20261012T221543` | `d3424096…` | Portal 2 `app-620 --until first-frame+10` | first frame 6.04 s |
| `20261012T221652` | `d3424096…` | uninstall RIOT (`uninstall:gog-1989052770`) | removed (its script read; not played) |

IPA sha256s: `6194e3e19b6ca5c056ca84678285b6d20b31fcae3695ec46f8d7ea4c02af581d` (built from the tree
committed as 0d9177a), `d34240963adabaf07030d02e42a73600cb803831e8e07c79eeacef1e24ca8f5e` (474934a, final).

Tomb Raider: Anniversary's Play (`20261012T220035`):

    library: gog: Tomb Raider: Anniversary install script goggame-1202885143.script: 1202885143 saveFolders: savePath is for GOG's cloud saves; left out
    library: gog: … registryKey: registry key Software\Crystal Dynamics\Tomb Raider: Anniversary without a value; nothing to write
    library: gog: … install_sdb: installSDB (an application-compatibility database) has no Wine counterpart; left out
    library: gog: … regUX: Execute {supportDir}/add/gameux.cmd is not run from the script; left out
    library: gog: Tomb Raider: Anniversary install script registry: system Software\Crystal Dynamics\Tomb Raider: Anniversary "InstallPath", system Software\Wow6432Node\… "InstallPath", … "FullInstall", … "Version", user Software\Crystal Dynamics\Tomb Raider: Anniversary "ExtendedDialog"
    [wine_host] registry playport-registry-system.reg: 2 of 5 seed keys added
    [wine_host] registry playport-registry-user.reg: 1 of 1 seed keys added

The stall is the same with and without the values (`20261012T215000`, before the change), so it is
not the script's; it is filed as a runtime follow-up, not fixed here. BioShock's script has no
registry action; its crash is likewise a follow-up.

BioShock's MSVC2005 entry shows a 0090 issue: GOG's arguments `/q:a /c:"VCREDI~3.EXE /q:a
/c:""msiexec /i vcredist.msi /qb"" "` reach the installer split at every space with the quotes
removed, and it exits 0 at once. A follow-up.

## Host

`pp test --swift PlayportKit --filter "GOGInstallScript|Prerequisite"` (GOGInstallScriptTests: the
real scripts of Tomb Raider: Anniversary, RIOT, Baldur's Gate EE, BioShock and Spelunky; value forms,
both registry views, placeholders, languages, bitness; PrerequisitesTests'
`testAWindowsInstallerPackageRunsThroughMsiexec`), `pp test --swift GOGClient --filter
GOGDependencies` (XNA_40 and PHYSXLEGACY now run), `pp test --quick`, `pp check`: all pass.

## Not covered

- An `.msi` redistributable on the phone: Dishonored (18 GB, the only owned build naming one) was not
  installed: the sweep's time box ran out (it needs about 11 GB more than the phone had free, so
  finished titles would be removed first). A follow-up.
- Generation-1 `support_commands` (PLA-188's install first).
- `setIni`, `installSDB`, the scripts' `Execute`: left out (decision 0103).
