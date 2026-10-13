# 0092: Steam apps that share an install folder

**Status:** accepted, 2026-10-12, by the compatibility speedrun's worker with the owner away (the
conservative option at each choice, recorded for review);
[PLA-192](https://linear.app/playportdev/issue/PLA-192).
[Evidence](../evidence/2026-10-12-shared-installdir-compat-merge.md).

## Context

Counter-Strike (Steam 10) and Half-Life (70) both name Steam's `Half-Life` folder and share
Half-Life's depots 1, 2 and 3 (the same manifests). Steam installs both into the one folder.
Playport refused the second: `Games/Half-Life already exists and is not this app's Playport
install`. Each install was one rename of a staged tree, and an uninstall moved the whole folder out.

## Decision

- **A folder another Steam app's record names is joined** (`SharedInstallDir`,
  `TitleInstaller.destination`). Each app keeps its own record and retained manifests. A folder
  that no Steam record names is still refused, as before.
- **Only the files no other app lists are downloaded**, then renamed into the folder file by file
  (`commitShared`). A file another app lists with the same size and SHA-1 is already there.
- **A file another app lists with other content stops the install or update before anything is
  written.** Steam writes over it; Playport refuses, so one app's update never breaks the other's
  files. Until both apps can move together, an update that changes a shared depot waits
  (follow-up).
- **An update in a shared folder** removes the files its previous build listed that neither its
  new build nor another app lists.
- **Uninstalling one app** removes the files of its plan that no other app's plan lists, then the
  directories that leaves empty (unless another app lists them), then its record. A retained
  manifest another record names stays. Files no plan lists (saves, configs the games wrote) stay.
  When a retained manifest of this app or another is missing, which files are whose is unknown:
  every file stays and only the record goes (logged). A game's files are worth more than the space.
- **The last app in the folder** is uninstalled as before: one rename out, then deleted.
- **The library lists one title per Steam record** in a folder. A second app joining a folder
  inherits nothing from the first's catalogue entry.

## Cost

A shared commit and a shared uninstall are not one rename. A kill part-way leaves files of both
builds, or some files gone with the record still there; the next install or uninstall finishes.
A shared title's Uninstall row still says "Frees" its whole size.
