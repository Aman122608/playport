# 0073: The screen setting is an aspect ratio, then a resolution in it

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

- **Two choices.** Settings › Graphics and a game's page ask for an **aspect ratio**
  (Phone, 16:9, 16:10, 4:3) and a **resolution**. Every aspect ratio offers the same
  resolutions, with the same labels in the same order (Native, 1080p, 900p, 720p, 540p);
  each is computed for the shape (HostIOKit `Display.guestSize`, spec `A:B@level`): a level
  is that many rows at the shape's width (16:9 at 720p is 1280x720, 4:3 at 720p 960x720),
  and Native is the largest rectangle of the shape inside the panel (16:9 on the reference
  phone 2240x1260). Width and height are even (Native rounds down, inside the panel).
- **Stored as two fields** of `LaunchSettings`: `screen` (the level, as before) and a new
  `aspect` (`phone`, `16:9`, `16:10`, `4:3`; nil inherits; PlayportKit `ScreenAspect`).
  Settings saved before have no `aspect` and keep their meaning (Phone, that level); an old
  `WxH` or `4:3` screen stays that size whatever the aspect ratio. The `--settings` JSON
  takes `aspect` beside `screen`.
- **Phone is the default**, at Playport's 720p, so nothing changes for anyone who does not
  touch it. Each choice inherits on its own: a game's own value overrides Settings', and
  "Default" (follow Settings) stays a game's default.
- **Another shape is aspect-fitted** on the panel, with bars at the sides or above and
  below, as every swap chain already is (HostIOKit `Display.aspectFit`).
- Dev and release apps alike; controller and touch, as every row of those pages.

## Why

The shape is a property of the game's rendering, not of sharpness or battery: a game that
assumes 16:9 at most needs 16:9 at whatever resolution the person wants, so the two are
separate choices, and one list of resolutions keeps the pickers simple.
