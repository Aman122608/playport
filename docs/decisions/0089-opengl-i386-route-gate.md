# 0089: OpenGL for i386: the route gate

**Status:** budget set 2026-10-11, before the measurement; outcome pending. This carries out the
OpenGL plan's chunk 3 gate ([the plan](../plans/2026-10-11-opengl-zink.md), "Route decision"). The
worker chose the conservative options (owner away).
[PLA-162](https://linear.app/playportdev/issue/PLA-162). Amends
[0087](0087-opengl-through-zink-on-the-host.md), which left i386 open until this gate.

## Context

On x86-64, OpenGL is Mesa's Zink in the app, reached through Wine's opengl32 unix thunks
([0087](0087-opengl-through-zink-on-the-host.md), [0088](0088-opengl-window-surfaces-and-pacing.md)).
For an i386 game, those thunks are WoW64 thunks: they convert pointers through the child's window
(`patches/wine-unix` 0029). Every GL call then pays for an emulator exit, a WoW64 thunk and a unix
call. Immediate-mode engines (GoldSrc and its era) make tens of thousands of GL calls a frame.

The other route is the guest-side fallback: Mesa's WGL and Zink built as an i686 `opengl32.dll`
over Wine's i386 `winevulkan`. GL calls stay inside the emulated Mesa there, and only Vulkan calls
cross.

## The budget (set before measuring)

- **Measurement.** The `gl21` probe's `stress` mode (`tools/compat/probes/gl21.c`), in its
  "immediate" phase: each frame is a clear, 20,000 `glVertex3f` calls between one
  `glBegin`/`glEnd`, and `SwapBuffers`. The probe logs the phase's mean frame time
  (`gl21: STRESS immediate: … ms per frame`).
  - It runs on the phone from the imported title, with the game layer's frame limit Off. Pacing
    then caps it at the display's 120 Hz (8.3 ms) rather than 60 FPS.
  - It runs twice per machine on the same IPA.
- **Budget: at most 16 ms per frame for i386**, the higher of its two runs.
- **If i386 meets the budget,** host-side Zink serves i386 as well as x86-64.
- **If it does not,** the guest-side i386 fallback is the route for i386, and x86-64 stays
  host-side. That fallback is a separate chunk, not built here.
- **Reported, without a budget:**
  - the same phase on x86-64;
  - the "client-array" and "vbo" phases (the same 20,000 vertices as one `glDrawArrays`, from
    client memory and from a buffer object);
  - the time the draw calls took within each frame.

## Outcome

Pending: the measurement and the route it decides go here.
