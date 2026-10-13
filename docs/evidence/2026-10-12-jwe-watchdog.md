# Evidence: what Jurassic World Evolution waits on before its WatchDog (PLA-41)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) and a static read of `JWE.exe`. **Linear:**
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution, Epic
`epic-373a4372c20540aca7fdd880d27fa49a`). The owner was away; the conservative option was taken at
each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`. All JWE runs were on Vulkan for
the session only (decision 0100).

**IPAs (dev):** `6ef835583acb160b3f294a3cc14cd6c0a870e8b85399525982047c642b4048ad` (first wait log),
`7db26fe6dc9c4e30b6187e39679e821c8bbe9d6d568a2d976d061d93fe9a767c` (wait ends),
`ddb09c3ec9b758f7acaad705ef5557c2db7f7289c271aeb7fe162d0363c3c6a0` (the committed wine-unix 0033;
installed at the end).

## Where 2.11 left it

On today's runtime JWE passes its anti-tamper, starts its threads and its 1564x720 window, presents
no frame, and at about 47 s its `WatchDog` thread crashes it on purpose (`mov dword [1], 0` at
`JWE.exe+0xa6c820`), while `Main` and `Render` wait in `NtWaitForMultipleObjects` and no thread is
busy ([anti-tamper replay](2026-10-12-anti-tamper-replay.md)).

## The wait log (wine-unix 0033)

`patches/wine-unix` 0033 is a dev diagnostic, off unless `WINEDEBUG` names `warn+waitlog` (set for
one session through the title's launch setting `runtime: env.WINEDEBUG=warn+waitlog`). A wait with
no timeout, or one of a second or more, logs its objects (type, name, an event's kind and state, a
semaphore's count, a thread's id) and the module-relative return addresses on the caller's stack; a
wait that took a second or more logs its end. A thread logs a wait only when it differs from its
last logged one, and the log stops at 4000 lines. Off, it costs one debug-channel test per wait.

## Runs

| Run | IPA | Session setting | Outcome |
| --- | --- | --- | --- |
| `s11c-jwe-wait` | `6ef83558` | `warn+seh,warn+waitlog,+winsock,+winhttp,+wininet` | `exit=0xc0000005` after 48 s; objects logged, no callers (module list read failed) |
| `s11c-jwe-wait2` | `7db26fe6` | `warn+seh,warn+waitlog,warn+secur32,warn+schannel` | the same after 48 s; wait ends logged, still no callers |
| `s11c-jwe-wait3` | `ddb09c3e` | `warn+waitlog` | the same after 47 s; objects, callers and ends |

All three end the same way as 2.11's runs, at the WatchDog, about 47 s after the game starts.

## Findings

**The network and Epic online-services step completes.** With `+winsock`, the EOS SDK on
`fTask W3` resolves `api.epicgames.dev` (two Cloudflare addresses) and its `HttpManagerThread` opens
three TCP connections to port 443, sends and receives on them (non-blocking connects completing as
usual), and its last socket call is at +7.5 s, before every other thread goes idle. No DNS failure,
no refused connection and no socket error was logged. `HttpManagerThread` and `LibwebsocketsThread`
then only sleep in their polling loops. `secur32`'s lsass message (`start_samss Failed to open service
manager`) is the known one without effect on Schannel (decision 0065); `warn+schannel` logged
nothing.

**Main and Render are idle in the engine's thread-affinity job loop.** In `s11c-jwe-wait3`:

- `Main` (tid `004c`) waits, with no timeout, on two auto-reset events: `0x78` (its own) and `0x80`.
  `Render` (`0080`) waits on `0x7c` (its own) and the same `0x80`. The four `fTask` workers wait on
  their own event, `0x80` and `0x84`. All are unsignaled when logged.
- Main's wait returned once after 1.7 s (`status 0x1`: the shared event `0x80`), Render's twice
  (1.6 s on `0x80`, 1.2 s on its own); shorter waits log no end. Their last waits, on the same
  objects from the same call site, never end.
- Main's guest callers, read against the `JWE.exe` copy from 2026-10-07: `WaitForMultipleObjects`
  through the import at `JWE.exe+0xa3a113` (in the wait function at `+0xa39e80`), called from
  `+0xa39dc0`, called at `+0xa3dff0` by the function at `+0xa3df20`. That function takes a role
  (`0x40000000` names the thread `Main`, `0x40000001` `Render`, others `ERROR`), names the thread
  and then serves jobs for that role until its loop ends; Main's thread function calls it at
  `+0x7884c`. Render's callers are the same loop with the `Render` role.
- `fIO 0`..`7` wait in `NtRemoveIoCompletion` (thread samples, every run): their completion port
  is empty. The
  last new file read before the stall is `fIO 0`'s 32 KiB read on an overlapped handle (`[read-src]
  #13`, about +9 s).
- `Message Pump` waits on handle `0x14c` with a timeout and returns; the WatchDog waits with a
  timeout; nothing else runs.

So nothing is blocked on a lock or a missing object: the engine's job graph has no job ready for
Main, Render or the workers, and what should produce the next one never reports. The candidates
the log leaves are a file read whose completion is never queued or never reaches the job system,
an EOS result the game waits for (EOS callbacks need a job that ticks the platform), or a window
message.

## Not changed

No fix was made: the cause is not contained by what this sweep found. The time box ended here.
Hollow Knight (`s11c-hk`, first frame +9.59 s, its main menu) and Portal 2 (`s11c-p2`, first frame
+5.68 s) reach their first frames on `ddb09c3e`, the IPA with 0033 (a runtime change, off by
default).

## Next

1. Extend the wait log to the I/O path for one session: `NtRemoveIoCompletion`/`Ex` results and, for
   reads on handles bound to a completion port, the immediate status, whether a packet is queued,
   and `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS`. A read whose packet is never queued, or queued for a
   synchronous success the game does not expect, would explain an idle `fIO` pool.
2. If the I/O path is clean, log `NtSetEvent` on the three role events (`0x78`, `0x7c`, `0x80`) with
   their callers, to see which subsystem last fed Main, and compare EOS's callbacks (the game's
   `EOS_Platform_Tick`).
