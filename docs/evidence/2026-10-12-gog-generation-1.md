# Evidence: Quake II on GOG has only a generation-1 build (PLA-188)

**Date:** 2026-10-12. **Kind:** GOG content-system reads on the workstation (no sign-in needed for
these), no phone run. **Linear:** [PLA-188](https://linear.app/playportdev/issue/PLA-188) (Quake II,
GOG 1441704824). The owner was away; the conservative option was taken: the design is recorded,
nothing is built.

## What GOG returns

The install failed with "GOG has no Windows build of 1441704824 this client can install" (run
`$PLAYPORT_BUILD/ui-runs/20261011T172754`).

- `content-system.gog.com/products/1441704824/os/windows/builds?generation=2` lists one item:
  `build_id 48442691445289595`, `version_name 3.20`, **`generation: 1`**, `legacy_build_id
  50078743`, link `gog-cdn-fastly.gog.com/content-system/v1/manifests/1441704824/windows/50078743/repository.json`.
  `?generation=1` lists the same. `GOGContent.parseBuilds` keeps only `generation == 2` items, so
  nothing is left: the product id is right, and there is no other build.
- For comparison NWN Diamond (1207658890) lists two generation-2 builds and an older
  generation-1 one; Playport takes the newest generation-2 build.

The generation-1 build (`repository.json`, open):

- `product`: `installDirectory "Quake II"`, `rootGameID 1441704824`, one `gameIDs` entry, and
  `support_commands`: `{executable: "/galaxy_quake2_quad_damage_2.0.0.3.exe", gameID, systems
  [Windows], languages [Neutral]}` (GOG's Inno setup for the game, as 0099's `temp_executable`).
- `depots`: `Neutral` (1.28 MB: the setup) and `English` (1.26 GB, 1,492 files), each a
  `manifest` named `<uuid>.json` beside the repository, plus `{redist: "DirectX", executable:
  "__redist/DirectX/DXSETUP.exe", argument: "/silent"}`.
- A depot manifest lists `files`: `{path, size, hash (md5 of the file), offset, url:
  "1441704824/main.bin"}`, and `support: true` on support files (their paths start with the
  product id: `1441704824/save/…`, `1441704824/galaxy_…exe`; game files start with `/`). There are
  no chunks: each file is a byte range of one uncompressed `main.bin` per product.

The game itself is i386 OpenGL (`quake2.exe`, `ref_gl.dll`, plus mod `gamex86.dll`s).

## Design (not built)

A generation-1 install in `GOGClientKit`, beside the generation-2 one, reusing ContentKit's engine:

1. `parseBuilds` keeps generation-1 items too; `build()` prefers the newest generation-2 build and
   falls back to generation 1 only when there is none.
2. `GOGContent.repositoryV1` reads `repository.json` and the depot manifests (language `English`
   for `en-US`, `Neutral` always; DirectX as 0090's skipped id). Files become `GOGDepotFile`s with
   synthetic range parts (at most 16 MB each, so a 600 MB `pak` is not one buffer), the whole-file
   md5 checked after the last part; `support` files keep the product-id prefix stripped, placed
   as generation 2's are.
3. A `GOGRangeSource` (a `ContentChunkSource`): the product's generation-1 secure link (the
   `secure_link` request with `generation=1`, to be confirmed with a signed-in session, which the
   workstation does not have), then `GET main.bin` with `Range: bytes=offset-end`, no inflate.
4. `support_commands` feed 0099's setup run. Verify and repair use the file md5s.

About a day with tests and a phone install; the GOG setup it would run hits the same i386 Inno
Setup failures as NWN Diamond's ([evidence](2026-10-12-gog-support-setup.md)), so Quake II would
still need those fixed. Next step: a Linear issue for the generation-1 install (this design),
blocked on nothing but scheduling.
