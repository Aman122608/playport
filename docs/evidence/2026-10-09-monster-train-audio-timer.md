# Monster Train's intro video: the audio timer thread killed by the server

**Date:** 2026-10-09. **Branch:** `dev/compat-speedrun`. **Linear:** PLA-54.
**Phone:** iPhone Air (`iPhone18,4`), iOS 27.0. **Title:** Monster Train (GOG
`1304291300`), x86-64, Unity 2019.4.4f1, AVPro Video 1.11.4 through Media
Foundation's media engine.

**Result:** the intro video plays on Vulkan at 16:9, and the game goes on to its
own screens with no input. Before the fix it stayed black on both backends.

## Before

IPA `62b62a7c004c860a3f06e0f0530475e19be8f9c02614567c6a4c878dfcd9bdc4`, Vulkan and
DXMT, 16:9. `Player.log` has `[AVProVideo] Using playback path:
MF-MediaEngine-Hardware (1920x1080@0.00)`. `logfile.log` shows the game on its
`IntroFmv` screen for the rest of the run. The screen is black. A pad A skips the
video, and then the game's screens draw normally on Vulkan.

The trace (`runtime`: `env.WINEDEBUG=+mfplat,+winegstreamer`) shows the media
engine polling `OnVideoStreamTick` about 1,200 times and never calling
`TransferVideoFrame`. The video sink got two samples and the audio renderer six.
The presentation clock's time source is the audio renderer, and its position
never moved. `[ios_audio] timer_loop #1` was never logged in any run. The two
`audio_client_timer` threads died at birth:

    [fdtrace] pipes tid=00c4 peb=0x0 master=32 ...
    [srv-own] receive_fd BAD tid=00c4 on pid=0020 ... cause=WRONG-PROCESS rev=ml586
    [srv-kill] kill_thread tid=00c4 pid=0028 violent=1 rev=ml586

Since wine-11.18 the mmdevapi driver starts its timer thread itself, with
`PsCreateSystemThread`. That thread has no TEB, so `ios_jit_current_peb` returned
NULL for it. The thread then sent its pipes on the root process's master socket,
and the in-process server killed it as a thread of the wrong process. So no
event-driven WASAPI client in a game's process was ever signalled.

## Fix

- `patches/wine-unix` 0023: `thread_data` gets `ios_owner_peb`.
- `patches/madeira-unix` 0100: `PsCreateSystemThread` records the creator's
  pseudo-process there. `ios_jit_current_peb` returns it for a thread without
  a TEB.
- The app exports a title config's `env.NAME` keys (the game's `runtime`
  setting) before the runtime starts, as Madeira's app did. The app wrote them to
  `madeira.cfg`, but nothing read them back. The trace above needed this.

## After

IPA `02fca070f2148ac98a0d35d45618b5d3a1f97b66145289bfa207f45954727342`, Vulkan,
16:9, no input:

- `[ios_audio] timer_loop #1` is logged, with no `WRONG-PROCESS` line.
- From about 15 s the screenshots show the intro video's frames, with
  subtitles at 30 s.
- At 60 s the video is still playing. By 100 s the game has gone on to its own
  screen ("Welcome to Hell!", Let's Go!), which is still up at 150 s.

A Phone-aspect run (1564x720) also plays the video. Its `Player.log` has no
`Sequence contains no elements`, so the title needs no compatibility default.
Screenshots stay in the run directories under `.work`.
