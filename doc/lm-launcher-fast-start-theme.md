# Darkmoonshine launcher startup and themes

The loader-only changes adapt the startup sequence from Moonshine V2.3.0,
commit `06cb7965032539a5ac88611b9c5aade7385daec9`. They do not replace
Darkmoonshine's Nintendont kernel, memory layout, Japanese game validation,
injection, memory-card handling, crash service or SD-state protocol.

## Startup

- Probe only the launcher's own device once for its background before the IOS
  reload, without the USB retry loop or a fallback-device search. Release the
  mount immediately, retaining only the decoded texture across the reload.
- Mount the launcher's storage normally after Nintendont is ready. This still
  permits a missing-device fallback and the existing normal USB startup timeout.
- Do not mount unused storage to show the menu. Check the remembered game's
  device only when Launch/Auto Boot validates that exact path, or when the user
  selects a storage device in the file browser. Failed browser checks can retry.
- Initialize and load MP3 playback only if the menu is actually needed, after
  the IOS reload. Auto Boot retains its B-to-cancel window but skips menu music.
- Remove the startup fade and unused per-boot HBC metadata inspection. Keep
  necessary IOS waits, controller initialization order, IPL font loading and
  every game-ID safety check.

The biggest removed delay is the unconditional unused-USB probe before the
menu (the old loader's retry window was ten seconds). No hardware speedup
number is claimed until measured on Wii. The driver itself may still block
inside a single initialization attempt.

## Theme files

The launcher reads these optional files from its own resolved storage device:

```
sd:/Darkmoonshine_Theme/background.png
sd:/Darkmoonshine_Theme/bgm.mp3
```

Use `usb:/` instead when launching from USB. The game may be on the other
device; that does not move its theme or settings. The application directory
does not affect these paths. `Moonshine_Theme` is never read, renamed or edited
by Darkmoonshine; users may copy its assets into the separate folder.

Every boot, including Auto Boot, performs an idempotent check after stable
storage mounting. A missing `Darkmoonshine_Theme` directory is created. An
existing directory is not written, and a regular file with that name is not
replaced. No default assets, configuration or subfolders are needed or written.
Read-only storage and creation errors remain non-fatal.

The existing decoder requires a valid **1024×480 PNG, at most 2 MiB**, and
valid **MP3 music, at most 4 MiB**. Missing assets use the embedded background
and silence. Invalid assets or memory failure retain safe fallback handling.
Music shutdown still stops/joins the decoder before releasing buffers or
handing control to the game.

## Verification

`scripts/test_lm_launcher_startup.py` executes the actual shared path/creation
helper against mocked FAT results, including SD/USB, missing/existing folders,
name conflicts, permission/I/O failures, creation races, invalid input and
truncated paths. Source-order checks bind lazy mounts, creation before Auto
Boot, menu-only audio, teardown and game validation to the actual loader.
The Wii loader cross-compiles successfully; existing unrelated path-truncation
warnings remain in inherited Nintendont code.

Focused hardware checks:

1. Boot with the copied background/music and an all-SD game, without USB attached.
2. Launch the game; music must stop completely at handoff.
3. Enable Auto Boot, reboot, then separately verify holding B returns to the menu.
4. With no theme folder, boot once: the empty folder is created and startup works.
5. Browse/select USB only when needed; an absent device can retry without losing
   the remembered game path. Confirm the original Moonshine theme is unchanged.

## Upstream references

- [V2.3.0 main.c](https://github.com/panther03/moonshine/blob/V2.3.0/launcher/loader/source/main.c)
- [V2.3.0 global.c](https://github.com/panther03/moonshine/blob/V2.3.0/launcher/loader/source/global.c)
- [V2.3.0 SusamuneMenu.c](https://github.com/panther03/moonshine/blob/V2.3.0/launcher/loader/source/SusamuneMenu.c)
- [V2.3.0 SusamuneThemeFiles.c](https://github.com/panther03/moonshine/blob/V2.3.0/launcher/loader/source/SusamuneThemeFiles.c)
