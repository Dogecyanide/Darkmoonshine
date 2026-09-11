# Moonshine Luigi's Mansion 0.3.41 — reboot archive candidate

This release targets reusable SD states after a soft game reset and a full Wii
reboot. It is an experimental candidate, not a claim of universal compatibility.
The controlled .40 exports confirmed the same build on two cold boots, with
matching retained heap geometry but different legitimate GAME camera addresses.

## Changed

- New format-25 states can carry a 1,440-byte retained-service compatibility
  record. Load checks the current native owners after draining audio, then
  checks them again at the frozen restore boundary. OS, audio allocator, GX FIFO,
  framebuffers and SYS heap machinery remain live; they are not raw-rewound.
- The three main cameras may move when each saved/live image independently
  proves their exact GAME allocations. The existing whole GAME/manager restore
  restores their references together.
- Reboot-ready SD archives use an SD-persistent authentication key, bind both
  the header and payload, and require the same mod build and launcher setup.
  Fresh process IDs still protect in-flight storage requests from resets.
- Export explicitly says **EXPORTED: REBOOT READY** or **EXPORTED: THIS BOOT
  ONLY**. The latter is a session backup, not a reboot-compatible file. The SD
  browser labels compatible persistent files **REBOOT ARCHIVE**.
- `lm_dumps` now retains eight rotating attempt journals, `lm_attempt_a.bin`
  through `lm_attempt_h.bin`, with additional camera/profile refusal details.
  These are logs, not playable savestates. New successful saves rotate attempts;
  the first load after a boot also starts a log if none is open, so reboot-load
  refusals are recorded even without a new save. Further loads stay within an
  attempt. A long session can overwrite
  its oldest logs. The two exception crash reports remain separate.

## Installation and archive care

Install the whole matching .41 app folder. Both launcher and payload changed.
Use fresh .41 states: .40 and older archives remain preserved and visible but
are not compatible with this build. Names and rename controls are unchanged.

Back up the entire `lm_states` folder, including `.lms`, `.name0`, `.name1`,
and the new `archive_key0.bin` / `archive_key1.bin` files. The game creates the
key copies when needed. Do not delete, edit or regenerate them: a different
key cannot authenticate existing reboot archives. Damaged or conflicting keys
are refused instead of silently replaced. Interrupted files are not adopted.

Keep the same launcher/video settings, progressive-mode choice, controller
port and completed Japanese Hidden Mansion file across this first reboot test.
Cheat/debug-enabled boots are not supported for persistent archives. Import
fills the memory slot; D-pad Right/Load separately restores gameplay.

The initial portable profile is limited to the ordinary mansion map/scene.
Boss arenas, Dojo/rush modes, transfers between different configurations and
cross-build migration are not newly enabled. A mismatched native owner may
still safely refuse with EPOCH. Do not bypass a refusal or edit archive IDs.
In-game preferences are still session-local. No new gameplay features or
general menu redesign are included in this focused release.

The included checklist prioritizes actual cold-boot reuse and the first door,
movement and menu warp afterward. Host tests do not substitute for that Wii
test. Stop at the first crash and preserve the logs before further testing.
