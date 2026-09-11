# DarkMoonshine — V1.0.0 Frozen in Time

Authors: **Dogecyanide, Nintendont Team**.

DarkMoonshine brings Moonshine-style practice tools to **Luigi's Mansion
(Japan, GLMJ01 revision 0)** on Wii through a custom Nintendont launcher.
The launcher injects the mod at boot; an unmodified, legally dumped Japanese
game image is all that is needed. No patched ISO, retail game save or private
runner archive is distributed.

## Release acceptance

Dogecyanide reports that **all ten RC4 Wii checklist items worked**. Those
checks exercised repeated same-room saves, overwrites and busy-room operation;
cross-floor returns; the Parlor → Storage → load → Boneyard → load → Anteroom
route; safe door-transition refusals; named SD archive switching; preservation
of the RAM state after failed import; reuse after soft and full Wii reboots;
and a normal practice session with displays, resets and menu warps.

V1.0.0 promotes the RC4 gameplay implementation with final release branding;
it adds no further gameplay feature beyond that tested candidate. The user's
report is hardware acceptance of the exercised routes, not a new test run of
the final-branded binary, an exhaustive mansion/boss proof, or a measured Wii
speed multiplier. Historical candidate notes remain available separately.

## Included

- **Savestates:** one complete resident RAM slot; guarded cross-room/floor and
  post-menu-warp restore; shorter state processing through bounded LZ4 blocks,
  denser Deflate fallback when capacity requires it, and equivalent faster
  checksums. Failed imports retain the previous RAM state.
- **SD archive library:** export with a Moonshine-style naming keyboard,
  browse/import, rename and confirmed permanent deletion. Reboot-eligible
  exports explicitly say `REBOOT READY`; `THIS BOOT ONLY` is not portable
  across a reboot. Import and Load remain separate actions.
- **Warps and room practice:** 68 named room/boss destinations, supported
  native room reset/clear recipes, and a recordable Reset Room combination.
  Ordinary door/transition and resource safety gates remain in force.
- **Sunshine-style timer and Creation:** the original slanted/angled visual
  layout, visibility/TIME-icon controls, per-character colours, position,
  size, opacity, brightness and background styling, plus separate streak
  editing. Taps allow precise placement; held movement accelerates.
- **Native-menu timer option:** choose whether active time continues in the
  native pause/map/Game Boy Horror menus. The D-pad Down mod menu always
  counts active game time; it does not pause gameplay. Scripted timer stops
  remain authoritative, and the native 36-counted-minute rollover remains.
- **Displays and timing:** Moonshine-style controller display, position,
  angle and horizontal-speed metadata, lag counting, R-pump hold-frame
  popup, and recorded input-timing references.
- **Practice options:** normal/Hidden Mansion selection, Boo/blackout and
  health presets, door/trap options, supported unlocks and plant presets,
  Poltergust tank selection, and native BGM controls. Tank contents do not
  grant element medals.
- **Luigi colour and persistence:** targeted shirt/cap RGB recolouring;
  in-game display, timer, colour and timing preferences persist on Wii in
  `moonshine_lm.ini`, outside the state timeline.
- **Launcher and diagnostics:** on-demand storage startup, Auto Boot with
  B-to-cancel, a separate root `Darkmoonshine_Theme` folder, brief action
  notices, retained heap checks, crash reports and eight attempt journals.

## Install or upgrade

Use **`DarkMoonshine-1.0.0-Frozen-in-Time.zip`**. Place its
`moonshine_luigis_mansion` folder inside SD `Apps`, and its separate
`Darkmoonshine_Theme` folder on the SD root. Install the matching `boot.dol`
and `mod_lmj.bin` together. Keep existing custom theme files if desired.

The app directory and data paths deliberately stay unchanged. Preferences,
normal game saves, archive names, private SD keys and logs are not reset by
the rebrand. Back up normal saves and the whole `lm_states` directory before
upgrading. Never publish the authentication-key files.

**Create fresh V1.0.0 states.** Snapshot format is still **28**, storage
protocol **6**, preferences version **2**, but the final version string changes
the authenticated build identity. RC4 and earlier states cannot be imported
into the final build simply because their schema matches. Old archives are
not deleted or converted; renaming does not make them compatible.

D-pad Left saves to RAM, Right loads, and Down opens the mod menu. Import
loads an archive into memory; the separate Load action restores gameplay.
Wait for storage completion and preference-save acknowledgements before
removing the SD or shutting down. Archive deletion is permanent after
confirmation and does not erase an already imported RAM state.

## Known limits

- Secret Altar can introduce event resources not covered by a saved earlier
  mansion state's ownership proof; that load safely refuses. A successful
  warp into a room is not evidence that all state loads out of it are supported.
- Every checksum/authentication pass, full decode validation, native
  owner/allocator proof and cache synchronization remains. `BUSY`, `EPOCH`
  and capacity refusals must not be bypassed. Wait until transitions settle.
- One RAM slot is intentional. Three measured complete dense states exceeded
  the safe separate inactive region by about 1–1.2 MB. Another resident slot
  requires a storage/transaction redesign; named SD archives provide the
  current multi-state library.
- Dojo, Boss Rush and Portrait Rush are deferred. The warp list is not an
  implementation of those modes. Input references and R-pump measurements
  do not yet label pearl dupes or Chauncey one-cycles as successful.
- Infinite health is not the same feature as the included health presets.
  Precise ghost-capture angle guidance remains future work.
- The Dolphin build is a separate development target. Its BPS alone does not
  provide Wii SD archive services or persistent in-game preferences.

The host benchmark found substantially lower compression work, but its
23–31.5× compression-step result is **not** an end-to-end Wii speed claim.
Raw core copying, native synchronization and SD I/O still contribute.

After a fault, send the entire `lm_dumps` folder, available
`luigis_mansion_crash_a/b.bin` and `.txt` reports, and the exact route/actions.
Identify whether failure occurred during Import, Load, or later gameplay.
The eight journals rotate; collect them promptly. Keep private keys off
public reports. Follow the packaged `TESTING.md` for focused reproduction.

## References and credits

Built on Moonshine, Nintendont and Better Nintendont, with reverse-engineering
references from Yasiki, Booldozer and the Luigi's Mansion decompilation
community. Timer artwork originates from Super Mario Sunshine, and the
supplied launcher-theme assets retain their original provenance. Existing
source/license notices remain; the distribution includes miniz and LZ4 notices.

[Storage contract](lm-state-storage.md) ·
[compression measurements](lm-compression-benchmark.md) ·
[Secret Altar evidence](lm-secret-altar-rc1-limit.md) ·
[remaining priorities](lm-current-priorities.md) ·
[RC4 history](darkmoonshine-1.0.0-rc4.md).
