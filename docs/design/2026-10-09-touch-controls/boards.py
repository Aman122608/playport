#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes this directory's boards (*.dc.html), canvas.json and index.html (PLA-13).

The boards are plain HTML like the gamepad-UI boards; this script keeps their
shared styles and the scene in one place. Run it from anywhere:
`python3 docs/design/2026-10-09-touch-controls/boards.py`; notes.json holds the
canvas notes.
"""
import json, math, pathlib, sys

OUT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent

BASE_CSS = """
body{margin:0;background:#0B0E13;font-family:'Barlow',system-ui,sans-serif;color:#EEF1F5}
a{color:#F5B544;text-decoration:none}a:hover{color:#FFD08A}
.d{font-family:'Barlow Semi Condensed','Barlow',sans-serif}
.g{display:inline-flex;align-items:center;justify-content:center;min-width:20px;height:20px;padding:0 5px;box-sizing:border-box;border-radius:10px;background:#EEF1F5;color:#0B0E13;font:700 11px 'Barlow',sans-serif}
.leg{display:flex;align-items:center;gap:6px;font-size:13px;color:#C9D1DC}
a.leg{padding:4px 8px;margin:0 -8px;border-radius:14px;color:#C9D1DC}a.leg:hover{background:#1D242E;color:#EEF1F5}
.item{display:flex;align-items:center;justify-content:space-between;gap:12px;height:38px;flex-shrink:0;padding:0 14px;box-sizing:border-box;border-radius:10px;font:500 15px 'Barlow',sans-serif;color:#EEF1F5}
.v{font-size:13px;color:#9AA5B4;display:flex;align-items:center;gap:6px}
.sel{background:#EEF1F5;color:#0B0E13}
.sel .v{color:#3A4452}
.h{font:600 11px 'Barlow';letter-spacing:.1em;text-transform:uppercase;color:#9AA5B4;padding:8px 14px 2px;flex-shrink:0}
.mine{width:7px;height:7px;border-radius:4px;background:#F5B544;flex-shrink:0}
.sw{width:40px;height:22px;border-radius:11px;background:#F5B544;position:relative;flex-shrink:0}
.sw.off{background:#2A3340}
.sw i{position:absolute;top:3px;right:3px;width:16px;height:16px;border-radius:8px;background:#1A1204}
.sw.off i{right:auto;left:3px;background:#9AA5B4}
.sel .sw.off{background:#C9D1DC}
.foot{position:absolute;bottom:0;height:36px;display:flex;align-items:center;gap:18px}

/* the game behind the controls: an original scene, no game's art */
.scene{position:absolute;inset:0;overflow:hidden;background:linear-gradient(180deg,#121a2e 0%,#1f2c48 42%,#33425f 68%,#1a2236 100%)}
.scene .moon{position:absolute;left:566px;top:46px;width:64px;height:64px;border-radius:50%;background:radial-gradient(circle at 38% 38%,#f6eed8,#d6c59c 62%,#b9a57a);box-shadow:0 0 60px 22px rgba(246,238,216,.13);opacity:.82}
.scene .far{position:absolute;left:0;right:0;bottom:118px;height:150px;background:#2a3954;clip-path:polygon(0 72%,7% 48%,15% 62%,24% 30%,33% 56%,43% 24%,52% 52%,61% 36%,71% 60%,81% 26%,90% 52%,100% 38%,100% 100%,0 100%)}
.scene .mid{position:absolute;left:0;right:0;bottom:62px;height:140px;background:#1c2842;clip-path:polygon(0 58%,9% 40%,21% 66%,34% 40%,47% 70%,59% 44%,73% 66%,85% 38%,100% 60%,100% 100%,0 100%)}
.scene .near{position:absolute;left:0;right:0;bottom:0;height:84px;background:linear-gradient(180deg,#131b2c,#0a0f1a)}
.scene .glow{position:absolute;left:330px;bottom:40px;width:220px;height:140px;background:radial-gradient(ellipse at center,rgba(255,190,110,.30),rgba(255,190,110,0) 65%)}
.scene .hero{position:absolute;left:418px;bottom:82px;width:26px;height:44px;background:#090d16;clip-path:polygon(50% 0,72% 12%,74% 34%,100% 100%,0 100%,26% 34%,28% 12%)}
.scene .lamp{position:absolute;left:446px;bottom:104px;width:6px;height:9px;border-radius:2px;background:#ffd38a;box-shadow:0 0 14px 6px rgba(255,200,120,.55)}
.scene .fly{position:absolute;width:3px;height:3px;border-radius:50%;background:#ffe2a6;box-shadow:0 0 8px 2px rgba(255,220,150,.6)}
.scene .hud{position:absolute;left:60px;top:16px;display:flex;align-items:center;gap:5px}
.scene .hud b{width:10px;height:10px;transform:rotate(45deg);background:#e9eef6;opacity:.85}
.scene .hud b.x{background:none;border:1.5px solid #e9eef6;box-sizing:border-box;opacity:.5}
.scene .hud span{margin-left:8px;width:84px;height:5px;border-radius:3px;background:linear-gradient(90deg,#9fc3ff 70%,rgba(255,255,255,.18) 70%)}
.scene.warm{background:linear-gradient(180deg,#1d1424 0%,#3a2433 40%,#6a3f3b 66%,#241722 100%)}
.scene.warm .far{background:#4a2f3a}.scene.warm .mid{background:#2e1d29}.scene.warm .near{background:linear-gradient(180deg,#1c1219,#0d0910)}
.scene.warm .moon{left:250px;top:70px;width:90px;height:90px;background:radial-gradient(circle at 50% 50%,#ffc58a,#e7834e 60%,#b4513a);box-shadow:0 0 90px 40px rgba(255,150,90,.18)}
.dim{position:absolute;inset:0;background:rgba(5,7,10,.55)}
.editing .tc,.editing .stick{border-style:dashed;border-color:rgba(238,241,245,.6);background:rgba(12,16,24,.42)}

/* on-screen controls: glass over the game, amber when held */
.tc{position:absolute;box-sizing:border-box;display:flex;flex-direction:column;align-items:center;justify-content:center;border-radius:50%;background:rgba(12,16,24,.30);border:1.5px solid rgba(238,241,245,.40);-webkit-backdrop-filter:blur(10px) saturate(1.25);backdrop-filter:blur(10px) saturate(1.25);color:rgba(238,241,245,.94);font:600 18px 'Barlow',sans-serif;line-height:1;box-shadow:inset 0 1px 0 rgba(255,255,255,.10),0 4px 14px rgba(0,0,0,.28)}
.tc small{font:700 8px 'Barlow';letter-spacing:.09em;text-transform:uppercase;color:rgba(238,241,245,.62);margin-top:3px}
.tc.pill{border-radius:17px;flex-direction:row;gap:6px;font-size:13px}
.tc.pill small{margin:0;font-size:8px}
.tc.on{background:rgba(245,181,68,.92);border-color:#FFD08A;color:#1A1204;box-shadow:0 0 0 6px rgba(245,181,68,.15),0 0 28px rgba(245,181,68,.5)}
.tc.on small{color:rgba(26,18,4,.72)}
.la{color:#86DE93}.lb{color:#FF8E80}.lx{color:#86B9FF}.ly{color:#F7D766}
.tc.on .la,.tc.on .lb,.tc.on .lx,.tc.on .ly{color:#1A1204}
.stick{position:absolute;box-sizing:border-box;border-radius:50%;border:1.5px solid rgba(238,241,245,.34);background:radial-gradient(circle,rgba(12,16,24,.06) 0%,rgba(12,16,24,.36) 72%);-webkit-backdrop-filter:blur(6px);backdrop-filter:blur(6px)}
.stick .nub{position:absolute;width:50px;height:50px;border-radius:50%;background:radial-gradient(circle at 40% 35%,rgba(250,252,255,.92),rgba(200,210,224,.70));box-shadow:0 6px 16px rgba(0,0,0,.35)}
.stick.on{border-color:rgba(245,181,68,.75)}
.stick.on .nub{background:radial-gradient(circle at 40% 35%,#FFE3A8,#F5B544);box-shadow:0 0 22px rgba(245,181,68,.6),0 6px 16px rgba(0,0,0,.35)}
.stick .k{position:absolute;font:700 10px 'Barlow';color:rgba(238,241,245,.55)}
.stick .k.lit{color:#F5B544}
.handle{position:absolute;left:387px;top:8px;width:100px;height:26px;border-radius:13px;box-sizing:border-box;display:flex;align-items:center;justify-content:center;gap:6px;background:rgba(12,16,24,.42);border:1px solid rgba(238,241,245,.28);-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px);font:600 12px 'Barlow';color:#EEF1F5}
.mark{width:12px;height:12px;border-radius:4px;background:linear-gradient(135deg,#FFD08A,#F5B544);box-shadow:0 0 8px rgba(245,181,68,.6)}
.toast{position:absolute;left:50%;transform:translateX(-50%);display:flex;align-items:center;gap:8px;white-space:nowrap;padding:7px 14px;border-radius:16px;background:rgba(21,26,33,.86);border:1px solid #2A3340;font-size:12.5px;color:#C9D1DC;-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px)}
.chip{display:inline-flex;align-items:center;gap:6px;height:24px;padding:0 10px;border-radius:12px;font:600 11px 'Barlow';letter-spacing:.04em;background:rgba(12,16,24,.45);border:1px solid rgba(238,241,245,.25);color:#C9D1DC}
.chip.on{border-color:rgba(245,181,68,.8);color:#FFD08A;box-shadow:0 0 12px rgba(245,181,68,.3)}
.dot{width:7px;height:7px;border-radius:50%;background:#F5B544;box-shadow:0 0 8px #F5B544}
.seg{display:flex;padding:3px;gap:2px;border-radius:12px;background:#0B0E13;border:1px solid #2A3340}
.seg span{flex:1;text-align:center;padding:6px 8px;border-radius:9px;font:600 12px 'Barlow';color:#9AA5B4;white-space:nowrap}
.seg span.on{background:#EEF1F5;color:#0B0E13}
.slider{position:relative;height:4px;border-radius:2px;background:#2A3340}
.slider b{position:absolute;left:0;top:0;bottom:0;border-radius:2px;background:#F5B544}
.slider i{position:absolute;top:-7px;width:18px;height:18px;margin-left:-9px;border-radius:50%;background:#EEF1F5;box-shadow:0 2px 6px rgba(0,0,0,.4)}
.kc{display:inline-flex;align-items:center;justify-content:center;min-width:18px;height:18px;padding:0 5px;box-sizing:border-box;border-radius:5px;background:#1D242E;border:1px solid #3A4452;border-bottom-width:2px;font:700 10.5px 'Barlow';color:#EEF1F5}
.sel .kc{background:#D7DDE6;border-color:#9AA5B4;color:#0B0E13}
"""

HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow:wght@400;500;600;700&amp;family=Barlow+Semi+Condensed:wght@500;600;700&amp;display=swap">
<style>{css}</style>
</helmet>
<div style="width: 874px; height: 402px; position: relative; overflow: hidden; background: #0B0E13">
{body}
</div>
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":874,"height":402}}}}'>
class Component extends DCLogic {{
renderVals() {{ return {{}}; }}
}}
</script>
</body>
</html>
"""

def scene(kind="", hud=True, flies=True):
    s = f'<div class="scene {kind}"><div class="moon"></div><div class="far"></div><div class="mid"></div><div class="glow"></div><div class="near"></div><div class="hero"></div><div class="lamp"></div>'
    if flies:
        for x, y in [(352, 214), (512, 188), (286, 250), (604, 238), (470, 160)]:
            s += f'<div class="fly" style="left: {x}px; top: {y}px"></div>'
    if hud:
        s += '<div class="hud"><b></b><b></b><b></b><b class="x"></b><span></span></div>'
    return s + '</div>'

def tc(x, y, r, label, cap="", cls="", lab=""):
    """A round control centred at x,y with radius r."""
    lc = f' class="{lab}"' if lab else ""
    small = f"<small>{cap}</small>" if cap else ""
    return (f'<div class="tc {cls}" style="left: {x-r}px; top: {y-r}px; width: {2*r}px; height: {2*r}px">'
            f'<span{lc}>{label}</span>{small}</div>')

def pill(x, y, w, h, label, cap="", cls=""):
    small = f"<small>{cap}</small>" if cap else ""
    return f'<div class="tc pill {cls}" style="left: {x}px; top: {y}px; width: {w}px; height: {h}px"><span>{label}</span>{small}</div>'

def stick(cx, cy, r, nub=(0, 0), on=False, keys=None):
    s = f'<div class="stick {"on" if on else ""}" style="left: {cx-r}px; top: {cy-r}px; width: {2*r}px; height: {2*r}px">'
    if on:
        # the direction held: a soft amber arc toward the nub
        ang = math.degrees(math.atan2(nub[1], nub[0]))
        s += (f'<div style="position: absolute; inset: -1.5px; border-radius: 50%; '
              f'background: conic-gradient(from {ang + 90 - 40}deg, rgba(245,181,68,.55), rgba(245,181,68,0) 80deg, rgba(245,181,68,0)); '
              f'-webkit-mask: radial-gradient(circle, transparent {r-6}px, #000 {r-5}px); mask: radial-gradient(circle, transparent {r-6}px, #000 {r-5}px)"></div>')
    if keys:
        for k, (kx, ky), lit in keys:
            s += f'<span class="k {"lit" if lit else ""}" style="left: {r+kx-4}px; top: {r+ky-6}px">{k}</span>'
    s += f'<div class="nub" style="left: {r-25+nub[0]}px; top: {r-25+nub[1]}px"></div></div>'
    return s

def handle(href="QuickMenuControls.dc.html"):
    return f'<a class="handle" href="{href}"><span class="mark"></span>Playport</a>'

def footer(entries, left=None, right=44):
    pos = f"left: {left}px" if left is not None else f"right: {right}px"
    out = f'<div class="foot" style="{pos}">'
    for g, label, href in entries:
        if href:
            out += f'<a class="leg" href="{href}"><span class="g">{g}</span>{label}</a>'
        else:
            out += f'<span class="leg"><span class="g">{g}</span>{label}</span>'
    return out + "</div>"

MOUSE_L = '<svg width="16" height="20" viewBox="0 0 16 20"><rect x="1" y="1" width="14" height="18" rx="7" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M8 1 V8 H1 V8 A7 7 0 0 1 8 1Z" fill="currentColor"/></svg>'
MOUSE_R = '<svg width="16" height="20" viewBox="0 0 16 20"><rect x="1" y="1" width="14" height="18" rx="7" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M8 1 V8 H15 A7 7 0 0 0 8 1Z" fill="currentColor"/></svg>'
KEYBOARD = '<svg width="20" height="14" viewBox="0 0 20 14"><rect x="1" y="1" width="18" height="12" rx="2.5" fill="none" stroke="currentColor" stroke-width="1.5"/><path d="M4 4.5h1.5M7.5 4.5H9M11 4.5h1.5M14.5 4.5H16M4 7.5h1.5M7.5 7.5H9M11 7.5h1.5M14.5 7.5H16M6 10.5h8" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>'
CURSOR = '<svg width="14" height="18" viewBox="0 0 14 18"><path d="M1 1 L1 15 L4.6 11.6 L7.2 17 L9.6 16 L7 10.6 L12 10.6 Z" fill="currentColor"/></svg>'
SCROLL = '<svg width="14" height="18" viewBox="0 0 14 18"><path d="M7 1 L11 6 H3 Z M7 17 L3 12 H11 Z" fill="currentColor"/><rect x="5.5" y="7.5" width="3" height="3" rx="1" fill="currentColor"/></svg>'
GYRO = '<svg width="16" height="16" viewBox="0 0 16 16"><circle cx="8" cy="8" r="2.2" fill="currentColor"/><ellipse cx="8" cy="8" rx="7" ry="3" fill="none" stroke="currentColor" stroke-width="1.3"/><ellipse cx="8" cy="8" rx="3" ry="7" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>'
PENCIL = '<svg width="14" height="14" viewBox="0 0 14 14"><path d="M2 12 L2.6 9.2 L9.6 2.2 L11.8 4.4 L4.8 11.4 Z" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/></svg>'
PAD = '<svg width="20" height="14" viewBox="0 0 20 14"><path d="M5 2 H15 C18 2 19.5 6 19.5 9.5 C19.5 12.5 17 13.5 15.5 11.5 L14 9.5 H6 L4.5 11.5 C3 13.5 0.5 12.5 0.5 9.5 C0.5 6 2 2 5 2Z" fill="none" stroke="currentColor" stroke-width="1.4"/><path d="M5 5.5v3M3.5 7h3" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"/><circle cx="14" cy="5.6" r="1" fill="currentColor"/><circle cx="15.8" cy="7.6" r="1" fill="currentColor"/></svg>'

boards = {}

# ---------------------------------------------------------------- touch: gamepad
b = scene()
b += handle()
b += pill(330, 8, 48, 26, "⧉") + pill(496, 8, 48, 26, "≡")
b += '<div class="toast" style="top: 44px"><span class="mark" style="width: 10px; height: 10px"></span>No controller, so on-screen controls are on. They hide when one connects.</div>'
b += pill(44, 82, 66, 34, "LT") + pill(44, 124, 66, 34, "LB")
b += pill(764, 82, 66, 34, "RT") + pill(764, 124, 66, 34, "RB")
# d-pad
b += '<div style="position: absolute; left: 132px; top: 168px; width: 92px; height: 92px">'
for dx, dy, glyph, on in [(31, 0, "▲", False), (31, 62, "▼", False), (0, 31, "◀", False), (62, 31, "▶", False)]:
    cls = "tc on" if on else "tc"
    b += f'<div class="{cls}" style="left: {dx}px; top: {dy}px; width: 30px; height: 30px; border-radius: 8px; font-size: 10px">{glyph}</div>'
b += '</div>'
b += stick(110, 314, 58, nub=(16, -18), on=True)
b += '<div class="chip" style="position: absolute; left: 76px; top: 378px; height: 18px; font-size: 9.5px; border: none; background: none; color: rgba(238,241,245,.55)">STARTS WHERE YOUR THUMB LANDS</div>'
b += stick(598, 330, 46, nub=(0, 0))
cx, cy, o = 748, 290, 52
b += tc(cx, cy - o, 25, "Y", lab="ly") + tc(cx - o, cy, 25, "X", lab="lx")
b += tc(cx + o, cy, 25, "B", lab="lb") + tc(cx, cy + o, 25, "A", cls="on", lab="la")
boards["TouchGamepad.dc.html"] = ("On-screen controls · Gamepad", b)

# ---------------------------------------------------------------- touch: keyboard & mouse
b = scene("warm", flies=False)
# the game's crosshair
b += '<div style="position: absolute; left: 427px; top: 191px; width: 20px; height: 20px"><div style="position: absolute; left: 9px; top: 0; width: 2px; height: 6px; background: #fff; opacity: .8"></div><div style="position: absolute; left: 9px; bottom: 0; width: 2px; height: 6px; background: #fff; opacity: .8"></div><div style="position: absolute; top: 9px; left: 0; width: 6px; height: 2px; background: #fff; opacity: .8"></div><div style="position: absolute; top: 9px; right: 0; width: 6px; height: 2px; background: #fff; opacity: .8"></div></div>'
b += handle()
b += pill(44, 52, 52, 28, "Esc") + pill(102, 52, 52, 28, "Tab")
for i, n in enumerate("1234"):
    b += pill(606 + i * 36, 52, 32, 28, n)
b += pill(762, 52, 68, 28, KEYBOARD, cls="", cap="")
b += f'<div class="chip on" style="position: absolute; left: 718px; top: 90px">{GYRO}Gyro aim</div>'
b += pill(44, 196, 80, 34, "⇧", "Sprint") + pill(132, 196, 80, 34, "Ctrl", "Crouch")
b += stick(128, 316, 66, nub=(0, -26), on=True, keys=[("W", (0, -52), True), ("S", (0, 50), False), ("A", (-54, -1), False), ("D", (52, -1), False)])
# drag-to-look: a finger's path on the free right half
b += ('<svg style="position: absolute; left: 470px; top: 120px" width="160" height="120" viewBox="0 0 160 120">'
      '<defs><linearGradient id="tr" x1="0" x2="1"><stop offset="0" stop-color="#EEF1F5" stop-opacity="0"/><stop offset="1" stop-color="#EEF1F5" stop-opacity=".55"/></linearGradient></defs>'
      '<path d="M10 100 C 50 92, 70 60, 120 46" fill="none" stroke="url(#tr)" stroke-width="3" stroke-linecap="round" stroke-dasharray="1 7"/>'
      '<circle cx="124" cy="45" r="15" fill="rgba(238,241,245,.16)" stroke="rgba(238,241,245,.6)" stroke-width="1.5"/></svg>')
b += '<div style="position: absolute; left: 618px; top: 150px; width: 110px; font: 600 9.5px Barlow; line-height: 1.35; letter-spacing: .09em; color: rgba(238,241,245,.6)">DRAG ANYWHERE FREE TO LOOK</div>'
F = (772, 306)
def ring(dist, deg):
    a = math.radians(deg)
    return round(F[0] + dist * math.cos(a)), round(F[1] + dist * math.sin(a))
b += tc(*F, 38, MOUSE_L, "Fire")
x, y = ring(92, 185); b += tc(x, y, 28, MOUSE_R, "Aim", cls="on")
x, y = ring(92, 222); b += tc(x, y, 23, "R", "Reload")
x, y = ring(92, 262); b += tc(x, y, 28, "␣", "Jump")
x, y = ring(152, 202); b += tc(x, y, 23, "E", "Use")
boards["TouchKeyboardMouse.dc.html"] = ("On-screen controls · Keyboard & mouse", b)

# ---------------------------------------------------------------- touch: point & click
b = scene(hud=False)
b += '<div style="position: absolute; inset: 0; background: linear-gradient(90deg, rgba(5,7,10,.0), rgba(5,7,10,.45) 60%)"></div>'
b += '<div style="position: absolute; left: 470px; top: 62px; width: 260px; font-family: Georgia, serif; color: #f1e6cc">'
b += '<div style="font-size: 26px; letter-spacing: .14em; margin-bottom: 92px; text-shadow: 0 2px 12px rgba(0,0,0,.6)">THE LONG ROAD</div>'
for i, t in enumerate(["New journey", "Continue", "Options", "Quit"]):
    st = "color: #ffd38a; text-shadow: 0 0 12px rgba(255,200,120,.6)" if i == 1 else "opacity: .78"
    b += f'<div style="font-size: 17px; margin: 9px 0; {st}">{"› " if i == 1 else ""}{t}</div>'
b += '</div>'
# the cursor where the finger is, a tap ripple, and the loupe above it
b += f'<div style="position: absolute; left: 566px; top: 214px; color: #fff; filter: drop-shadow(0 1px 2px rgba(0,0,0,.7))">{CURSOR}</div>'
b += '<div style="position: absolute; left: 546px; top: 196px; width: 40px; height: 40px; border-radius: 50%; border: 2px solid rgba(245,181,68,.85); box-shadow: 0 0 0 10px rgba(245,181,68,.12)"></div>'
b += ('<div style="position: absolute; left: 520px; top: 98px; width: 92px; height: 92px; border-radius: 50%; overflow: hidden; '
      'border: 2px solid rgba(238,241,245,.75); box-shadow: 0 8px 24px rgba(0,0,0,.5); background: #1c2842">'
      '<div style="position: absolute; left: -46px; top: 24px; font: 30px Georgia, serif; color: #ffd38a; text-shadow: 0 0 14px rgba(255,200,120,.6); white-space: nowrap">› Continue</div>'
      '<div style="position: absolute; left: 50px; top: 46px; color: #fff; transform: scale(1.6); transform-origin: 0 0">' + CURSOR + '</div></div>')
b += handle()
b += '<div class="seg" style="position: absolute; right: 44px; top: 8px; width: 210px; background: rgba(12,16,24,.55); border-color: rgba(238,241,245,.2)"><span class="on">Tap to click</span><span>Trackpad</span></div>'
tools = [(MOUSE_R, "Right"), (CURSOR, "Hover"), (SCROLL, "Scroll"), (KEYBOARD, "Keys"), ("Esc", "Esc")]
b += '<div style="position: absolute; left: 44px; top: 66px; width: 58px; padding: 8px 0; border-radius: 18px; display: flex; flex-direction: column; align-items: center; gap: 8px; background: rgba(12,16,24,.38); border: 1px solid rgba(238,241,245,.22); -webkit-backdrop-filter: blur(10px); backdrop-filter: blur(10px)">'
for i, (icon, cap) in enumerate(tools):
    on = " on" if i == 1 else ""
    b += f'<div class="tc{on}" style="position: relative; width: 44px; height: 44px; font-size: 13px"><span>{icon}</span><small>{cap}</small></div>'
b += '</div>'
b += '<div class="toast" style="top: auto; bottom: 14px; left: 437px">Hover is on: your finger moves the pointer without clicking</div>'
boards["TouchPointer.dc.html"] = ("On-screen controls · Point & click", b)

# ---------------------------------------------------------------- touch keyboard
b = scene()
b += '<div style="position: absolute; left: 150px; top: 54px; width: 300px; padding: 14px 16px; border-radius: 4px; background: rgba(10,14,22,.82); border: 1px solid #5b6b86; font-family: Georgia, serif; color: #f1e6cc">'
b += '<div style="font-size: 13px; opacity: .8; margin-bottom: 8px">Name your save</div><div style="font-size: 18px; border-bottom: 1px solid #8a97ad; padding-bottom: 3px">Wanderer<span style="opacity: .8">|</span></div></div>'
b += handle()
rows = [
    [("Esc", 1.3)] + [(f"F{i}", 1) for i in range(1, 13)] + [("Del", 1.3)],
    [("`", 1)] + [(c, 1) for c in "1234567890-="] + [("⌫", 1.6)],
    [("Tab", 1.5)] + [(c, 1) for c in "QWERTYUIOP[]"] + [("\\", 1.1)],
    [("Caps", 1.8)] + [(c, 1) for c in "ASDFGHJKL;'"] + [("Enter", 1.8)],
    [("⇧", 2.3)] + [(c, 1) for c in "ZXCVBNM,./"] + [("⇧", 2.3)],
    [("Ctrl", 1.4), ("Win", 1.2), ("Alt", 1.2), ("Space", 6.4), ("Alt", 1.2), ("◀", 1), ("▼", 1), ("▲", 1), ("▶", 1)],
]
b += '<div style="position: absolute; left: 44px; right: 44px; bottom: 6px; padding: 8px; border-radius: 18px; background: rgba(12,16,24,.55); border: 1px solid rgba(238,241,245,.22); -webkit-backdrop-filter: blur(14px) saturate(1.2); backdrop-filter: blur(14px) saturate(1.2); display: flex; flex-direction: column; gap: 4px">'
for ri, row in enumerate(rows):
    h = 24 if ri == 0 else 30
    b += f'<div style="display: flex; gap: 4px; height: {h}px">'
    for k, w in row:
        st = "background: rgba(238,241,245,.10); border: 1px solid rgba(238,241,245,.20); color: #EEF1F5"
        if k == "⇧" and ri == 4 and b.count('data-shift') == 0:
            st = "background: #F5B544; border: 1px solid #FFD08A; color: #1A1204; box-shadow: 0 0 14px rgba(245,181,68,.45)"
            k = '<span data-shift>⇧ Shift</span>'
        elif k == "W":
            st = "background: rgba(245,181,68,.88); border: 1px solid #FFD08A; color: #1A1204"
        fs = 10 if ri == 0 else 12.5
        b += f'<div style="flex: {w} 1 0; border-radius: 7px; display: flex; align-items: center; justify-content: center; font: 600 {fs}px Barlow; {st}">{k}</div>'
    b += '</div>'
b += '</div>'
b += '<div class="chip" style="position: absolute; right: 44px; top: 162px; background: rgba(12,16,24,.6)">Shift stays down until the next key · ✕ Close</div>'
boards["TouchKeyboard.dc.html"] = ("Touch keyboard (any layout's ⌨)", b)

# ---------------------------------------------------------------- in-game menu: Controls
b = scene("warm", flies=False)
b += '<div style="position: absolute; inset: 0; background: rgba(5,7,10,.6)"></div>'
b += '<div style="position: absolute; top: 0; left: 0; width: 300px; height: 402px; background: #151A21; border-right: 1px solid #2A3340; padding: 22px 20px 0 44px; box-sizing: border-box; display: flex; flex-direction: column; gap: 3px">'
b += '<div style="font: 600 11px Barlow; letter-spacing: .1em; text-transform: uppercase; color: #9AA5B4; padding: 0 14px">Playing · 18 min</div>'
b += '<div class="d" style="font-size: 24px; font-weight: 700; padding: 0 14px 10px">The Long Road</div>'
b += '<div class="item"><span>Resume</span></div>'
b += '<div class="item sel"><span>Controls</span><span class="v">Touch ›</span></div>'
b += '<div class="item"><span>Screenshot</span><span class="v">Saved to Photos</span></div>'
b += '<div class="item"><span>Performance overlay</span><span class="v">Off</span></div>'
b += '<div class="item" style="color: #FF9A8A"><span>Quit game</span><span class="v">Saves first</span></div>'
b += '</div>'
b += '<div style="position: absolute; left: 324px; top: 22px; right: 44px; bottom: 46px; padding: 16px 18px; box-sizing: border-box; border-radius: 18px; background: #151A21; border: 1px solid #2A3340; display: flex; flex-direction: column; gap: 12px">'
b += '<div style="display: flex; align-items: baseline; justify-content: space-between"><div class="d" style="font-size: 20px; font-weight: 700">Controls</div><div style="font-size: 12px; color: #9AA5B4">For this game · saved as you change them</div></div>'
b += '<div style="font: 600 11px Barlow; letter-spacing: .1em; text-transform: uppercase; color: #9AA5B4">On-screen controls</div>'
b += '<div class="seg"><span>Off</span><span>Gamepad</span><span class="on">Keyboard &amp; mouse</span><span>Point &amp; click</span></div>'
b += '<div style="display: flex; align-items: center; gap: 14px; font-size: 13px; color: #C9D1DC"><span style="width: 92px">Opacity</span><div class="slider" style="flex: 1"><b style="width: 70%"></b><i style="left: 70%"></i></div><span style="width: 36px; text-align: right">70%</span></div>'
b += f'<div style="display: flex; align-items: center; justify-content: space-between; padding: 8px 12px; border-radius: 12px; background: #1D242E"><div style="display: flex; align-items: center; gap: 10px"><span style="color: #F5B544">{GYRO}</span><div><div style="font-size: 14px">Gyro aim</div><div style="font-size: 11.5px; color: #9AA5B4">Moves the mouse while you hold Aim · iPhone gyro</div></div></div><span class="sw"><i></i></span></div>'
b += '<div style="display: flex; align-items: center; gap: 14px; font-size: 13px; color: #C9D1DC"><span style="width: 92px">Sensitivity</span><div class="slider" style="flex: 1"><b style="width: 40%"></b><i style="left: 40%"></i></div><span style="width: 36px; text-align: right">2.0×</span></div>'
b += '<div style="display: flex; gap: 8px; margin-top: auto">'
for icon, t, href in [(PENCIL, "Edit layout", "LayoutEditor.dc.html"), (PAD, "Controller", "ControllerMapping.dc.html"), (GYRO, "Gyro settings", "GyroAim.dc.html")]:
    b += f'<a href="{href}" style="flex: 1; display: flex; align-items: center; justify-content: center; gap: 8px; height: 36px; border-radius: 10px; background: #1D242E; border: 1px solid #2A3340; color: #EEF1F5; font: 600 13px Barlow">{icon}{t}</a>'
b += '</div></div>'
b += footer([("A", "Choose", None), ("Y", "Defaults", None), ("B", "Back to game", "TouchKeyboardMouse.dc.html")])
boards["QuickMenuControls.dc.html"] = ("In-game menu · Controls", b)

# ---------------------------------------------------------------- layout editor
b = scene("warm", flies=False)
b += '<div style="position: absolute; inset: 0; background: rgba(5,7,10,.5)"></div>'
b += '<div style="position: absolute; inset: 0; background-image: linear-gradient(rgba(238,241,245,.05) 1px, transparent 1px), linear-gradient(90deg, rgba(238,241,245,.05) 1px, transparent 1px); background-size: 16px 16px"></div>'
b += '<div class="editing" style="position: absolute; inset: 0">'
b += pill(44, 52, 52, 28, "Esc") + pill(102, 52, 52, 28, "Tab")
for i, n in enumerate("1234"):
    b += pill(606 + i * 36, 52, 32, 28, n)
b += pill(762, 52, 68, 28, KEYBOARD)
b += pill(44, 196, 80, 34, "⇧", "Sprint") + pill(132, 196, 80, 34, "Ctrl", "Crouch")
b += stick(128, 316, 66, keys=[("W", (0, -52), False), ("S", (0, 50), False), ("A", (-54, -1), False), ("D", (52, -1), False)])
b += tc(*F, 38, MOUSE_L, "Fire")
x, y = ring(92, 185); b += tc(x, y, 28, MOUSE_R, "Aim")
x, y = ring(92, 222); b += tc(x, y, 23, "R", "Reload")
jx, jy = ring(92, 262)
x, y = ring(152, 202); b += tc(x, y, 23, "E", "Use")
# the selected control: amber frame, handles, size
b += tc(jx, jy, 28, "␣", "Jump")
b += '</div>'
b += f'<div style="position: absolute; left: {jx-34}px; top: {jy-34}px; width: 68px; height: 68px; border: 2px solid #F5B544; border-radius: 10px; box-sizing: border-box"></div>'
for hx, hy in [(-34, -34), (34, -34), (-34, 34), (34, 34)]:
    b += f'<div style="position: absolute; left: {jx+hx-5}px; top: {jy+hy-5}px; width: 10px; height: 10px; border-radius: 3px; background: #F5B544; border: 1.5px solid #1A1204"></div>'
b += f'<div style="position: absolute; left: {jx-24}px; top: {jy-58}px; padding: 2px 7px; border-radius: 8px; background: #F5B544; color: #1A1204; font: 700 10px Barlow">56 pt</div>'
# snap guide: Jump's centre lines up with Fire's
b += f'<div style="position: absolute; left: {F[0]}px; top: {jy+36}px; width: 0; height: {F[1]-38-jy-36}px; border-left: 1.5px dashed #F5B544"></div>'
b += f'<div style="position: absolute; left: {jx-1}px; top: {jy-1}px; width: 2px; height: 2px"></div>'
# toolbar
b += '<div style="position: absolute; left: 50%; transform: translateX(-50%); top: 8px; display: flex; align-items: center; gap: 4px; padding: 4px; border-radius: 16px; background: rgba(21,26,33,.92); border: 1px solid #2A3340; white-space: nowrap">'
for t, st in [("＋ Add", ""), ("Keyboard &amp; mouse ▾", ""), ("Opacity 70%", ""), ("↶", ""), ("↷", "opacity: .4"), ("Reset", "")]:
    b += f'<span style="padding: 6px 11px; border-radius: 12px; font: 600 12.5px Barlow; color: #EEF1F5; {st}">{t}</span>'
b += '<a href="QuickMenuControls.dc.html" style="padding: 6px 14px; border-radius: 12px; background: #F5B544; color: #1A1204; font: 700 12.5px Barlow">Done</a></div>'
# inspector
b += '<div style="position: absolute; left: 44px; top: 54px; width: 262px; padding: 14px; box-sizing: border-box; border-radius: 18px; background: rgba(21,26,33,.96); border: 1px solid #2A3340; display: flex; flex-direction: column; gap: 8px; box-shadow: 0 12px 40px rgba(0,0,0,.45)">'
b += '<div style="display: flex; align-items: center; gap: 10px"><div class="tc" style="position: relative; width: 36px; height: 36px; font-size: 14px">␣</div><div><div class="d" style="font-size: 18px; font-weight: 700">Jump</div><div style="font-size: 11.5px; color: #9AA5B4">Button · right side</div></div></div>'
b += '<a class="item sel" href="ActionPicker.dc.html" style="height: 34px"><span>Presses</span><span class="v"><span class="kc">Space</span> ›</span></a>'
b += '<div class="item" style="height: 34px; background: #1D242E"><span>Label</span><span class="v">Jump</span></div>'
b += '<div style="display: flex; align-items: center; gap: 12px; padding: 4px 14px; font-size: 13px; color: #C9D1DC"><span style="width: 44px">Size</span><div class="slider" style="flex: 1"><b style="width: 45%"></b><i style="left: 45%"></i></div></div>'
b += '<div class="seg"><span class="on">Hold</span><span>Toggle</span><span>Repeat</span></div>'
b += '<div style="display: flex; gap: 6px"><span style="flex: 1; text-align: center; padding: 7px; border-radius: 10px; background: #1D242E; font: 600 12.5px Barlow">Duplicate</span><span style="flex: 1; text-align: center; padding: 7px; border-radius: 10px; background: #1D242E; font: 600 12.5px Barlow; color: #FF9A8A">Remove</span></div>'
b += '</div>'
b += '<div class="toast" style="top: auto; bottom: 12px; left: 560px; font-size: 11.5px">Drag to move · pinch to resize · snaps to the grid and to other controls</div>'
boards["LayoutEditor.dc.html"] = ("Editing a layout, over the paused game", b)

# ---------------------------------------------------------------- action picker
b = scene("warm", hud=False, flies=False)
b += '<div style="position: absolute; inset: 0; background: rgba(5,7,10,.78)"></div>'
b += '<div style="position: absolute; left: 107px; top: 20px; width: 660px; height: 340px; padding: 16px 18px; box-sizing: border-box; border-radius: 20px; background: #151A21; border: 1px solid #2A3340; display: flex; flex-direction: column; gap: 10px; box-shadow: 0 20px 60px rgba(0,0,0,.6)">'
b += '<div style="display: flex; align-items: baseline; justify-content: space-between"><div class="d" style="font-size: 20px; font-weight: 700">What Jump presses</div><div style="font-size: 12px; color: #9AA5B4">Now: <span class="kc">Space</span></div></div>'
b += '<div style="display: flex; align-items: center; gap: 10px"><span class="g">LB</span><div class="seg" style="flex: 1"><span class="on">Keyboard</span><span>Mouse</span><span>Controller</span><span>Playport</span></div><span class="g">RB</span></div>'
krows = [
    [("Esc", 1.2)] + [(c, 1) for c in "1234567890"] + [("⌫", 1.4)],
    [("Tab", 1.4)] + [(c, 1) for c in "QWERTYUIOP"] + [("Enter", 1.2)],
    [("Caps", 1.6)] + [(c, 1) for c in "ASDFGHJKL"] + [("'", 1), ("", 0.0)],
    [("Shift", 2)] + [(c, 1) for c in "ZXCVBNM"] + [(",", 1), (".", 1), ("Shift", 1.6)],
    [("Ctrl", 1.3), ("Alt", 1.2), ("Space", 6), ("Alt", 1.2), ("←", 1), ("↑", 1), ("↓", 1), ("→", 1)],
]
b += '<div style="display: flex; flex-direction: column; gap: 4px">'
for row in krows:
    b += '<div style="display: flex; gap: 4px; height: 30px">'
    for k, w in row:
        if w == 0:
            continue
        if k == "Space":
            st = "background: #EEF1F5; color: #0B0E13; outline: 3px solid #F5B544; outline-offset: 2px"
        else:
            st = "background: #1D242E; border: 1px solid #2A3340; border-bottom-width: 2px; color: #C9D1DC"
        b += f'<div style="flex: {w} 1 0; border-radius: 7px; display: flex; align-items: center; justify-content: center; font: 600 12px Barlow; {st}">{k}</div>'
    b += '</div>'
b += '</div>'
b += '<div style="display: flex; align-items: center; gap: 8px; font-size: 12.5px; color: #9AA5B4; margin-top: 2px"><span>With</span>'
for m in ["Shift", "Ctrl", "Alt"]:
    b += f'<span class="chip" style="background: #1D242E">{m}</span>'
b += '<span style="margin-left: auto">Recent</span>'
for k in ["E", "R", "F", "Shift"]:
    b += f'<span class="kc">{k}</span>'
b += '</div></div>'
b += footer([("A", "Choose", "LayoutEditor.dc.html"), ("X", "Press a real key", None), ("B", "Back", "LayoutEditor.dc.html")])
boards["ActionPicker.dc.html"] = ("What a control presses", b)

# ---------------------------------------------------------------- game options: Controls
b = '<div style="position: absolute; left: 0; top: 0; width: 874px; height: 230px; background: linear-gradient(120deg,#3a2433,#6a3f3b)"></div>'
b += '<div class="d" style="position: absolute; left: 44px; top: 96px; font-size: 48px; font-weight: 700">The Long Road</div>'
b += '<div style="position: absolute; inset: 0; background: rgba(5,7,10,.72)"></div>'
b += '<div style="position: absolute; top: 0; right: 0; width: 410px; height: 402px; background: #151A21; border-left: 1px solid #2A3340; padding: 18px 28px 0 20px; box-sizing: border-box; display: flex; flex-direction: column; gap: 2px; overflow: hidden">'
b += '<div class="d" style="font-size: 22px; font-weight: 700; padding: 0 14px 2px">Game options</div>'
b += '<div class="h">Controls</div>'
b += '<div class="item sel"><span>On-screen controls</span><span class="v"><span class="mine"></span>Keyboard &amp; mouse ›</span></div>'
b += '<a class="item" href="LayoutEditor.dc.html"><span>Edit on-screen layout</span><span class="v">Over the game ›</span></a>'
b += '<a class="item" href="ControllerMapping.dc.html"><span>Controller</span><span class="v"><span class="mine"></span>As keyboard &amp; mouse ›</span></a>'
b += '<a class="item" href="GyroAim.dc.html"><span>Gyro aim</span><span class="v"><span class="mine"></span>Mouse · 2.0× ›</span></a>'
b += '<div class="h">Graphics</div>'
b += '<div class="item"><span>Aspect ratio</span><span class="v">Default · Phone ›</span></div>'
b += '<div class="item"><span>Resolution</span><span class="v">Default · 720p ›</span></div>'
b += '<div class="item"><span>Frame rate limit</span><span class="v">Default · 60 fps ›</span></div>'
b += '</div>'
b += '<div style="position: absolute; left: 44px; top: 160px; width: 380px; display: flex; flex-direction: column; gap: 8px; font-size: 13px; color: #C9D1DC">'
b += '<div style="display: flex; align-items: center; gap: 8px"><span class="mine"></span>Changed for this game. The rest follow Settings › Controls.</div>'
b += '<div style="color: #9AA5B4; font-size: 12.5px; line-height: 1.4; padding-left: 15px">On-screen controls show only while no controller is connected. This game reads only a keyboard and mouse, so Playport suggested those.</div></div>'
b += footer([("A", "Choose", None), ("Y", "Back to default", None), ("B", "Close", None)], left=44)
boards["GameOptionsControls.dc.html"] = ("Game options · Controls", b)

# ---------------------------------------------------------------- controller mapping
OX, OY = 287, 96
b = '<div style="position: absolute; left: 44px; top: 18px"><div style="font: 600 11px Barlow; letter-spacing: .1em; text-transform: uppercase; color: #9AA5B4">The Long Road · Controls</div><div class="d" style="font-size: 24px; font-weight: 700">Controller</div></div>'
b += '<div class="seg" style="position: absolute; right: 44px; top: 22px; width: 400px"><span>As a controller</span><span class="on">As keyboard &amp; mouse</span><span>Custom</span></div>'
svg = f'<svg style="position: absolute; left: 0; top: 0" width="874" height="402" viewBox="0 0 874 402">'
svg += f'<g transform="translate({OX} {OY})">'
svg += '<rect x="58" y="2" width="56" height="16" rx="6" fill="#1D242E" stroke="#3A4452" stroke-width="1.5"/><rect x="186" y="2" width="56" height="16" rx="6" fill="#1D242E" stroke="#3A4452" stroke-width="1.5"/>'
svg += '<rect x="50" y="16" width="76" height="14" rx="6" fill="#1D242E" stroke="#3A4452" stroke-width="1.5"/><rect x="174" y="16" width="76" height="14" rx="6" fill="#1D242E" stroke="#3A4452" stroke-width="1.5"/>'
svg += '<path d="M70 30 Q150 20 230 30 C262 34 282 62 292 112 C302 164 290 196 262 194 C238 192 226 166 210 150 L90 150 C74 166 62 192 38 194 C10 196 -2 164 8 112 C18 62 38 34 70 30 Z" fill="#151A21" stroke="#3A4452" stroke-width="2"/>'
svg += '<circle cx="84" cy="80" r="23" fill="#0B0E13" stroke="#3A4452" stroke-width="1.5"/><circle cx="84" cy="80" r="14" fill="#1D242E" stroke="#4A5566"/>'
svg += '<circle cx="190" cy="124" r="21" fill="#0B0E13" stroke="#3A4452" stroke-width="1.5"/><circle cx="190" cy="124" r="13" fill="#1D242E" stroke="#4A5566"/>'
svg += '<path d="M104 116 h10 v-10 h10 v10 h10 v10 h-10 v10 h-10 v-10 h-10 Z" fill="#1D242E" stroke="#4A5566" stroke-width="1.2"/>'
svg += '<rect x="126" y="70" width="14" height="9" rx="3" fill="#1D242E" stroke="#4A5566"/><rect x="160" y="70" width="14" height="9" rx="3" fill="#1D242E" stroke="#4A5566"/>'
svg += '<circle cx="150" cy="52" r="9" fill="#1D242E" stroke="#F5B544" stroke-width="1.2"/>'
for (bx, by, l, c) in [(222, 62, "Y", "#F7D766"), (204, 80, "X", "#86B9FF"), (240, 80, "B", "#FF8E80"), (222, 98, "A", "#86DE93")]:
    fill = "#F5B544" if l == "A" else "#1D242E"
    tcol = "#1A1204" if l == "A" else c
    svg += f'<circle cx="{bx}" cy="{by}" r="9" fill="{fill}" stroke="#4A5566"/><text x="{bx}" y="{by+3.5}" text-anchor="middle" font-family="Barlow" font-weight="700" font-size="10" fill="{tcol}">{l}</text>'
svg += '</g>'
left = [("LT", 86, (86, 10)), ("LB", 120, (88, 23)), ("L stick", 176, (62, 80)), ("D-pad", 240, (110, 126)), ("View", 300, (133, 75))]
right = [("RT", 70, (214, 10)), ("RB", 104, (212, 23)), ("Y", 138, (231, 62)), ("X", 172, (204, 89)), ("B", 206, (249, 80)), ("A", 240, (231, 98)), ("R stick", 274, (211, 124)), ("Menu", 308, (167, 75))]
labels = {"LT": ("Right", "Aim", MOUSE_R), "LB": ("Q", "Ability", None), "L stick": ("W A S D", "Move", None), "D-pad": ("1 2 3 4", "Items", None), "View": ("Tab", "Map", None),
          "RT": ("Left", "Fire", MOUSE_L), "RB": ("E", "Use", None), "Y": ("F", "Torch", None), "X": ("R", "Reload", None), "B": ("C", "Crouch", None), "A": ("Space", "Jump", None), "R stick": ("Mouse", "Look", None), "Menu": ("Esc", "Pause", None)}
html = ""
for side, items in (("l", left), ("r", right)):
    for name, y, (px, py) in items:
        sel = name == "A"
        col = "#F5B544" if sel else "#3A4452"
        if side == "l":
            x0 = 230
        else:
            x0 = 644
        svg += f'<line x1="{x0}" y1="{y+13}" x2="{OX+px}" y2="{OY+py}" stroke="{col}" stroke-width="{1.6 if sel else 1}"/><circle cx="{OX+px}" cy="{OY+py}" r="2.5" fill="{col}"/>'
        key, what, icon = labels[name]
        keyhtml = f'<span style="display: inline-flex; color: {"#1A1204" if sel else "#EEF1F5"}">{icon}</span>' if icon else ""
        if " " in key and not icon:
            keyhtml += "".join(f'<span class="kc">{k}</span>' for k in key.split())
        elif not icon:
            keyhtml += f'<span class="kc">{key}</span>'
        else:
            keyhtml += f'<span style="font-weight: 600">{key}</span>'
        bg = "background: #EEF1F5; color: #0B0E13" if sel else "background: #151A21"
        x = 44 if side == "l" else 644
        html += (f'<div class="{"sel" if sel else ""}" style="position: absolute; left: {x}px; top: {y}px; width: 186px; height: 26px; box-sizing: border-box; padding: 0 10px; border-radius: 9px; {bg}; display: flex; align-items: center; gap: 6px; font-size: 12.5px">'
                 f'<span style="width: 48px; flex-shrink: 0; white-space: nowrap; color: {"#3A4452" if sel else "#9AA5B4"}; font-size: 11.5px">{name}</span>{keyhtml}<span style="margin-left: auto; font-size: 11px; color: {"#3A4452" if sel else "#9AA5B4"}">{what}</span></div>')
svg += '</svg>'
b += svg + html
b += f'<div style="position: absolute; left: {OX+150}px; top: {OY+50}px; transform: translate(-50%,-150%); font: 600 9.5px Barlow; letter-spacing: .08em; color: #F5B544; white-space: nowrap">HOME · PLAYPORT MENU</div>'
b += '<div class="toast" style="bottom: 46px; top: auto; left: 437px; border-color: rgba(245,181,68,.5)"><span class="dot"></span>Press a button on the controller to change it, or choose one</div>'
b += footer([("A", "Change", "ActionPicker.dc.html"), ("Y", "Reset all", None), ("B", "Back", "GameOptionsControls.dc.html")])
boards["ControllerMapping.dc.html"] = ("Controller · as keyboard & mouse", b)

# ---------------------------------------------------------------- gyro aim
b = '<div style="position: absolute; left: 44px; top: 18px"><div style="font: 600 11px Barlow; letter-spacing: .1em; text-transform: uppercase; color: #9AA5B4">The Long Road · Controls</div><div class="d" style="font-size: 24px; font-weight: 700">Gyro aim</div></div>'
# visual: the phone in perspective, with its rotation arcs
b += '<div style="position: absolute; left: 44px; top: 70px; width: 330px; height: 290px; border-radius: 20px; background: radial-gradient(ellipse at 50% 30%, #1d2638, #11161e 70%); border: 1px solid #1D242E; overflow: hidden">'
b += '<div style="position: absolute; left: 0; right: 0; top: 18px; height: 120px; perspective: 700px; display: flex; justify-content: center; align-items: center">'
b += ('<div style="width: 176px; height: 84px; border-radius: 18px; background: linear-gradient(135deg, #2a3445, #161c26); border: 2px solid #4A5566; '
      'transform: rotateX(22deg) rotateY(-26deg) rotateZ(-7deg); box-shadow: 0 30px 40px rgba(0,0,0,.5); position: relative">'
      '<div style="position: absolute; inset: 6px; border-radius: 12px; background: linear-gradient(180deg, #3a2433, #6a3f3b)"></div>'
      '<div style="position: absolute; left: 50%; top: 50%; width: 10px; height: 10px; margin: -5px; border-radius: 50%; border: 1.5px solid #fff"></div></div></div>')
b += ('<svg style="position: absolute; left: 0; top: 0" width="330" height="160" viewBox="0 0 330 160">'
      '<defs><marker id="ah" markerWidth="8" markerHeight="8" refX="4" refY="4" orient="auto"><path d="M0 0 L8 4 L0 8 Z" fill="#F5B544"/></marker></defs>'
      '<path d="M70 122 C 120 150, 210 150, 262 120" fill="none" stroke="#F5B544" stroke-width="1.8" marker-end="url(#ah)" opacity=".9"/>'
      '<path d="M286 104 C 300 80, 300 50, 284 30" fill="none" stroke="#86B9FF" stroke-width="1.8" marker-end="url(#ah)" opacity=".9"/>'
      '<text x="150" y="156" font-family="Barlow" font-weight="700" font-size="9" letter-spacing="1" fill="#F5B544">TURN</text>'
      '<text x="300" y="70" font-family="Barlow" font-weight="700" font-size="9" letter-spacing="1" fill="#86B9FF">TILT</text></svg>')
# the test area: the dot is your aim
b += '<div style="position: absolute; left: 16px; right: 16px; bottom: 16px; height: 112px; border-radius: 14px; background: #0B0E13; border: 1px solid #2A3340; overflow: hidden">'
b += '<div style="position: absolute; inset: 0; background-image: radial-gradient(circle, rgba(238,241,245,.10) 1px, transparent 1.5px); background-size: 16px 16px"></div>'
b += '<div style="position: absolute; left: 196px; top: 30px; width: 40px; height: 40px; border-radius: 50%; border: 1.5px solid rgba(238,241,245,.4)"><div style="position: absolute; left: 13px; top: 13px; width: 10px; height: 10px; border-radius: 50%; border: 1.5px solid rgba(238,241,245,.6)"></div></div>'
b += ('<svg style="position: absolute; left: 0; top: 0" width="298" height="112" viewBox="0 0 298 112"><path d="M40 86 C 80 90, 110 40, 150 52 S 200 58, 214 50" fill="none" stroke="#F5B544" stroke-width="2.5" stroke-linecap="round" stroke-dasharray="0.1 6" opacity=".7"/></svg>')
b += '<div style="position: absolute; left: 206px; top: 42px; width: 14px; height: 14px; border-radius: 50%; background: #F5B544; box-shadow: 0 0 14px #F5B544"></div>'
b += '<div style="position: absolute; left: 12px; top: 8px; font: 600 10px Barlow; letter-spacing: .08em; color: #9AA5B4">TRY IT · THE DOT IS YOUR AIM</div>'
b += '</div></div>'
b += '<div style="position: absolute; left: 394px; right: 44px; top: 64px; display: flex; flex-direction: column; gap: 2px">'
rows = [
    ("Gyro aim", '<span class="mine"></span>Mouse ›', True),
    ("Gyro from", "Auto · iPhone ›", False),
    ("On", "While holding Aim ›", False),
    ("Sensitivity", '<div class="slider" style="width: 96px"><b style="width: 40%"></b><i style="left: 40%"></i></div><span style="width: 30px; text-align: right">2.0×</span>', False),
    ("Up and down", '0.8× of across ›', False),
    ("Turn with", 'Player space ›', False),
    ("Invert up and down", '<span class="sw off"><i></i></span>', False),
    ("Calibration", '<span style="color: #6FCF7F">●</span> Automatic · when still', False),
]
for t, v, sel in rows:
    b += f'<div class="item {"sel" if sel else ""}" style="height: 35px"><span>{t}</span><span class="v">{v}</span></div>'
b += '</div>'
b += footer([("A", "Change", None), ("Y", "Back to default", None), ("B", "Back", "GameOptionsControls.dc.html")])
boards["GyroAim.dc.html"] = ("Gyro aim", b)

# ---------------------------------------------------------------- settings: controls
b = '<div style="position: absolute; inset: 0; display: flex; flex-direction: column">'
b += '<div style="flex-grow: 1; display: flex; gap: 22px; padding: 22px 44px 0">'
b += '<div style="width: 190px; display: flex; flex-direction: column; gap: 3px">'
b += '<div class="d" style="font-size: 26px; font-weight: 700; padding: 0 14px 10px">Settings</div>'
for n in ["Accounts", "Graphics", "Downloads", "Controls", "Storage", "Setup check", "About"]:
    on = "background: #EEF1F5; color: #0B0E13; font-weight: 600" if n == "Controls" else "color: #C9D1DC"
    b += f'<div style="display: flex; align-items: center; height: 37px; padding: 0 14px; border-radius: 10px; font: 500 15px Barlow; {on}">{n}</div>'
b += '</div><div style="flex-grow: 1; display: flex; flex-direction: column; gap: 7px; padding-top: 6px">'
def srow(t, s, right, focus=False):
    f = "outline: 3px solid #F5B544; outline-offset: 3px;" if focus else ""
    return (f'<div style="{f} display: flex; align-items: center; justify-content: space-between; gap: 12px; min-height: 44px; padding: 5px 14px; box-sizing: border-box; border-radius: 10px; background: #151A21">'
            f'<div style="display: flex; flex-direction: column; font-size: 15px">{t}<span style="font-size: 12px; color: #9AA5B4">{s}</span></div>{right}</div>')
b += srow("On-screen controls", "Shown while no controller is connected", '<span class="v">Automatic ›</span>', True)
b += srow("Starting layout", "Each game can pick its own", '<span class="v">Suggested for the game ›</span>')
b += srow("Opacity", "How much of the game shows through", '<div style="display: flex; align-items: center; gap: 10px; font-size: 13px; color: #C9D1DC"><div class="slider" style="width: 120px"><b style="width: 60%"></b><i style="left: 60%"></i></div>60%</div>')
b += srow("A tap you can feel", "Haptics when a control is pressed", '<span class="sw"><i></i></span>')
b += srow("DualSense Wireless Controller", "Player 1 · battery 80% · has a gyro", f'<span style="display: flex; color: #6FCF7F">{GYRO}</span>')
b += '</div></div>'
b += '<div style="height: 36px; flex-shrink: 0; display: flex; align-items: center; justify-content: flex-end; gap: 18px; padding: 0 44px; border-top: 1px solid #1D242E">'
b += '<span class="leg"><span class="g">A</span>Change</span><span class="leg"><span class="g">B</span>Back</span></div></div>'
boards["SettingsControls.dc.html"] = ("Settings · Controls", b)

# ---------------------------------------------------------------- write
for name, (title, body) in boards.items():
    (OUT / name).write_text(HEAD.format(title=title, css=BASE_CSS.strip(), body=body))

W, H, GAP = 874, 402, 80
rows = [
    ("Playing without a controller", ["TouchGamepad.dc.html", "TouchKeyboardMouse.dc.html", "TouchPointer.dc.html", "TouchKeyboard.dc.html"]),
    ("Changing controls while you play", ["QuickMenuControls.dc.html", "LayoutEditor.dc.html", "ActionPicker.dc.html"]),
    ("A controller, gyro and defaults", ["GameOptionsControls.dc.html", "ControllerMapping.dc.html", "GyroAim.dc.html", "SettingsControls.dc.html"]),
]
canvas = {"v": 3, "attachments": {}, "boards": {}, "createdOnFiles": {"at": "2026-10-09T12:00:00Z", "v": 1},
          "designSystems": [], "launch": {"view": "canvas"}, "notes": {}, "order": [], "pages": [],
          "title": "Playport touch controls, mapping and gyro (PLA-13)"}
for ri, (rt, names) in enumerate(rows):
    y = ri * (H + 300)
    canvas["notes"][f"row{ri}"] = {"kind": "title1", "maxW": 4 * (W + GAP), "text": rt, "w": 240, "x": 0, "y": y - 100}
    for i, n in enumerate(names):
        canvas["boards"][n] = {"h": H, "is_interactive": True, "radius": 28, "title": boards[n][0], "w": W, "x": i * (W + GAP), "y": y}
        canvas["order"].append(n)
notes = json.loads((pathlib.Path(__file__).parent / "notes.json").read_text())
x0 = 3 * (W + GAP)
for i, (key, fill, text) in enumerate(notes):
    canvas["notes"][key] = {"fill": fill, "maxH": H, "text": text, "w": 420, "x": x0 + (i % 2) * 460, "y": (H + 300) + (i // 2) * 330 - 0}
(OUT / "canvas.json").write_text(json.dumps(canvas, indent=2, sort_keys=True) + "\n")

# a plain page that shows every board, for a browser without the canvas tool
idx = ['<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Touch controls, mapping and gyro · boards</title>',
       '<style>body{margin:0;padding:32px;background:#05070a;color:#EEF1F5;font-family:system-ui,sans-serif}h1{font-weight:600;font-size:22px}h2{font-weight:600;font-size:15px;color:#9AA5B4;margin:28px 0 10px}',
       '.row{display:flex;flex-wrap:wrap;gap:24px}figure{margin:0}figcaption{font-size:13px;color:#9AA5B4;margin-top:6px}iframe{width:874px;height:402px;border:0;border-radius:28px;background:#0B0E13}</style></head><body>',
       '<h1>Touch controls, mapping and gyro (PLA-13)</h1><p style="color:#9AA5B4">See README.md for the model and the decisions behind these boards.</p>']
for rt, names in rows:
    idx.append(f'<h2>{rt}</h2><div class="row">')
    for n in names:
        idx.append(f'<figure><iframe src="{n}" scrolling="no"></iframe><figcaption>{boards[n][0]}</figcaption></figure>')
    idx.append('</div>')
idx.append('</body></html>\n')
(OUT / "index.html").write_text("\n".join(idx))
print("wrote", len(boards), "boards")
