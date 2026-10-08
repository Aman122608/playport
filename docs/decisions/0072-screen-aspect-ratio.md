# 0072: The screen setting is an aspect ratio, then a resolution in it

**Status:** accepted, 2026-10-08, by the owner (through the supervisor of the PLA-108
session); [PLA-108](https://linear.app/playportdev/issue/PLA-108). Adds a launch setting
to [0034](0034-a-gamepad-first-ui.md)'s list (resolution, frame rate limit, Direct3D,
launch arguments).

## Context

Every resolution Playport offered (Native, 1080p, 900p, 720p, 540p) kept the phone's own
shape, 2.17:1 on the reference phone. Super Meat Boy draws nothing on a screen wider than
16:9: under desktop Proton it renders at 1280x720, 1152x720 and 1600x900 and shows only
black from 1312x720 to 1600x720, and on the phone it renders when given 1280x720
([evidence](../evidence/2026-10-08-super-meat-boy.md)). A person had no way to give a game
another shape; only a hidden `4:3` spec (1024x768) and `WxH` through a dev build's
`pp ui --settings` existed.

## Decision

- **Two choices, one value.** Settings › Graphics and a game's page ask for an **aspect
  ratio** (Phone, 16:9, 16:10, 4:3), then a **resolution** in it: Phone keeps today's list
  (Native, 1080p, 900p, 720p, 540p, rows at the panel's shape); 16:9 offers 1920x1080,
  1600x900, 1280x720, 960x540; 16:10 1920x1200, 1680x1050, 1440x900, 1280x800; 4:3
  1600x1200, 1280x960, 1024x768, 800x600. Picking another shape gives its resolution
  nearest 720 rows. Both are the one screen spec `LaunchSettings.screen` already held
  (`native`/`<rows>` for Phone, `WxH` for the others; PlayportKit `ScreenAspect`), so
  settings saved before keep their meaning (`720` is Phone at 720p) and the `--settings`
  JSON is unchanged; `4:3` still reads as 1024x768.
- **Phone is the default**, at Playport's 720p, so nothing changes for anyone who does not
  touch it. A game's own choice overrides Settings'; "Default" (follow Settings) stays a
  game's default, and choosing Phone on a game's page stores it, to override a global shape.
- **Another shape is aspect-fitted** on the panel, with bars at the sides or above and
  below, as every swap chain already is (HostIOKit `Display.aspectFit`).
- Dev and release apps alike; controller and touch, as every row of those pages.

## Why

The shape is a property of the game's rendering, not of sharpness or battery: a game that
assumes 16:9 at most needs 16:9 at whatever resolution the person wants. Keeping one spec
avoids a migration and a second field that could disagree with the first.
