# 0091: A compatibility default's keys stay under the person's own

**Status:** accepted, 2026-10-12, by the compatibility speedrun's worker with the owner away (the
conservative option, recorded for review); [PLA-123](https://linear.app/playportdev/issue/PLA-123),
found with Duck Paradox ([PLA-52](https://linear.app/playportdev/issue/PLA-52)). Amends
[0074](0074-per-title-compatibility-defaults.md)'s precedence for the two key=value text settings.
[Evidence](../evidence/2026-10-12-shared-installdir-compat-merge.md).

## Context

0074 gives each launch setting one precedence: the person's own choice, then the title's
compatibility default, then Settings. For the text settings that meant a person's own text
replaced the default's whole. Duck Paradox's default is `runtime: env.MADEIRA_KEEP_SMC=1`: a
person who typed any other Runtime key (`env.FOO=1`) lost it, and the game closed at start again.

## Decision

- **Runtime keys and Graphics options merge by key.** The effective text is the default's items
  whose key the person's text does not name, then the person's text as typed
  (`LaunchSettings.keyItems`, used by `LaunchSettings.over`). The key is the text before the
  first `=`; an item with no `=` is its own key. A person's key wins, so `env.MADEIRA_KEEP_SMC=0`
  still turns that default off; every other key of the default stays. For a Graphics option that
  repeats a variable (`VKD3D_CONFIG`), the person's items for it replace the default's for it.
- **Arguments are unchanged:** free text with no keys, so a person's own Arguments replace the
  default's. Prepending the default's would leave no way to remove one, and no shipped default
  sets Arguments.
- **The game page** shows a default's keys under the Runtime keys and Graphics options rows:
  "ⓘ For this game, unless you set the same key: …". The rows' own values stay the person's text.

## Cost

A person cannot remove a default key without naming it, only change its value. Reset on the row
still clears only the person's text.
