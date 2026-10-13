# `pp ui` reports `until_reached` (PLA-102)

Host-side change (`tools/ui.py`, `tools/phonelib.py`); no app change. IPA sha256
`3b39063b3b46a8fb44d114408db51d57a61cd8254bf40f58b7dc98acd6bc8f44` (the `installed` event).

`./pp ui --play app-620 --until first-frame+10` (Portal 2): first frame at +5.74 s, the
`until` event, stop `why: until`, result `ok: true`, exit 0, `until: "first-frame+10"`,
`until_reached: true`.

The false case (a title that ends cleanly before the checkpoint: `ok: false`, exit 1,
`until_reached: false`, why "the title ended (…) before --until …") and `last-play.json`
written only after a launched play are covered by `tools/tests/test_ui.py` (`UntilReached`),
not shown on the phone.
