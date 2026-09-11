# 0.3.34 experimental changes

## Latest crash and HUD ownership

The 0.3.33 Wii journals recorded several completed loads, a successful menu
warp to Anteroom, then a final load whose saved-source and restored-copy grain
checks both passed. The first native draw crashed at `8003BB3C`, DAR `13F`.
The failing Game Boy Horror shadow wrapper references a GAME picture whose
texture pointer was `101`. That fixed wrapper was not rewound with GAME,
leaving an ownership mismatch after scene reloads. The exact picture pointer
at save time is absent from this dump, so the precise stale allocation alias
in this run is not proven.

The snapshot now captures the related GBH screen/picture/fade roots, digit
picture wrappers and health-picture wrappers together: two ranges totaling
1,152 bytes. The intervening persistent font and destructor registration
record are excluded. See [authenticated HUD proof](lm-hud-state.md).

Snapshot format is **18**. Make fresh states. This targeted change does not
prove every room, boss map or repeated-load route now works, and does not add
shared SYS archive capture or cross-reboot SD-state support.

## Door-transition guard

State readiness now follows the bounded native player/controller chain and
checks the game's actual door state: controller mode 2, state `20` hex. The
check runs during readiness/preflight, after audio quiescing, and while frozen
before save/restore writes. A door's command IDs and animation substeps are
not used as substitute state IDs. See [door proof](lm-door-transition-gate.md).

A load requested during the door action is refused with **BUSY / DOOR**.
Wait for the door sequence and ordinary stability checks to finish, then press
load again. Invalid player/controller data gives **DPLAYER**, not an assumed
idle door. This guard is separate from the HUD fix and does not cover every
other cutscene, ladder, death or boss transition.

## R-pump display

Open **Displays -> R-pump display** and toggle it ON. It is independent of the
input-timing trainer and defaults OFF. Two lines show the live hold and last
completed hold in game-update frames, not video retraces. The press frame
counts; the release frame does not. The completed value stays visible until
the next completed pump or the toggle is reset.

The raw-input predicate is digital R OR analogue R >= 30, stated in menu help.
It is the existing timing-reference threshold, not a newly verified retail
vacuum activation threshold or trick-success detector. Menus, disconnects,
player changes and savestate operations cancel partial measurements; release
R before starting a fresh measurement. Preferences remain session-local.

## Hardware retest

1. Turn R-pump ON. Compare short and long presses; release R and check that the
   final count stays visible. Open/close the menu while holding R: it should
   cancel that partial hold and wait for a release.
2. Fresh Parlor state -> normal door to Anteroom -> load -> use a door again.
   Repeat, including the menu-warp route that preceded the last failure.
3. With a known-good fresh state, request a load during door animation. Expect
   a refusal; once the door finishes, request the load again.

Stop on a crash and retain both `lm_attempt_*.bin` journals, LM crash reports
and `ndebug.log`. These journals contain diagnostics, not complete savestates.
