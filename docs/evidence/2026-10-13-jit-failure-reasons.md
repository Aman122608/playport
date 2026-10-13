# JIT setup failure reasons (PLA-8, sweep 15f)

IPA `7e95a2c083c6d74f65be5ded7df5a0b22b4ecf31ff2b90c4b74c09e5462c8948` (dev), installed in place on the working phone (iOS 27.0).

- `PlayportKit/JitFailureReason.swift` sorts the built-in readiness check's reason into
  helper missing, pairing not accepted, disk image download failed, Developer Mode off,
  mount failed, or other (generic text with the first line of the raw reason, 100 characters
  at most). Seven unit tests in `JitFailureReasonTests` cover each class.
- The setup sheet and the release Setup check show that text. Both detect a missing
  `PlugIns/PlayportJIT.appex` before any check.
- Each failed check writes one `jit: check failed [class]: reason` line through
  `AppLog`, release included (decision 0009: the one-line append is within the size cap).
- Phone: `pp ui --action open:settings#setup --action wait:8 --shot-each-action` shows Setup check
  with JIT `ready (TXM present)`: no regression on a working phone. Failure texts were not
  shown on the phone: no failing phone is available here (a stripped-extension install needs a
  spare slot and the owner). Not run: `pp verify --variant release` (needs a release build).
- Open: the reporters' `jit:` lines, to learn which class their failure is.
