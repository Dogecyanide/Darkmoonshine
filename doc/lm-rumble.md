# Savestate rumble reconciliation (.44)

Loading a quiet snapshot during a live rumble rewinds the game's last-motor-command
latches but not the physical motor. The game's next update sees its restored
"already stopped" latches and sends no stop. The motor stays on until a later
rumble event supplies a new command.

The successful-load tail now cancels transient rumble at both software layers,
then sends hard-stop command 2 through the real `PADControlMotor` entry point on
all four ports. This happens only after restore and heap validation have succeeded.
Refused loads, saves, archive imports and exports do not cancel rumble.

## Authenticated Japanese retail anchors

All addresses are checked against clean GLMJ01 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee` in `scripts/test_lm_rumble.py`.

- `80070EC4` allocates the `0x44`-byte game manager at global `804A1758`.
  Its constructor allocates four `0xD8`-byte rumble controllers, stored at offsets
  `4, 8, C, 10`. Native setup `80071470` gives the first controller the persistent
  gamepad at `804A0BF8`; the other three receive null.
- Controller update `80085648` only starts the motor when byte `+4` was clear and
  only hard-stops when byte `+5` was set. This is the mismatched edge bookkeeping.
- Native reset `800852CC` restores the pad binding, clears both latches and all
  eight active wave slots/counters. It makes no calls and does not touch input,
  SDK queues or user settings. Calling it also prevents a snapshot captured
  during rumble from reviving an old wave after the hard stop.
- `80005854` allocates a `0x98`-byte JUTGamePad. Constructor `801D1E9C` identifies
  vtable `8038925C`, CRumble `+64..+74`, and signed port at `+74`.
  Only CRumble's four words are cleared; the live connection/error fields,
  buttons, sticks, callbacks, and remaining pad data are preserved.
- JUT's four motor-status bytes at `804A2064` are cleared. Its user-enabled mask
  at `804A2068` is deliberately untouched. Native `CRumble::clear` is **not** used:
  it re-enables rumble on every port as a side effect.
- `PADControlMotor` is `801E4CE4`; hard stop is command `2`. Retail routes this
  through `SISetCommand`/`SITransferCommands`, preserving the SDK's connection
  checks. Nintendont's `Patch.c` replaces this same function with
  `kernel/asm/PADControlMotor.S`, a bounded write to its per-port motor mailbox.
  Calling the entry point therefore works with either native SI or emulated
  controller transport; the mod does not write that mailbox directly.

## Safety and tests

Normalization validates full object ranges and allocated-block headers before
following pointers. The persistent pad additionally requires its correct vtable,
SYS owner and valid/temporarily unassigned port. All four game controllers must
be distinct, non-overlapping allocations with the native pad bindings before any
of them is reset. Invalid object graphs are not dereferenced or modified; the
bounded hardware hard stop still runs independently.

The host harness covers quiet and active saved waves, new legitimate rumble after
load, disabled and disconnected pads, untouched input/connection bytes, corrupt
or truncated objects, and successful-load-only integration. Retail instruction
tests authenticate the native routines and Nintendont dispatch. These are source
and host checks, not a claim that physical Wii motor behavior has been verified;
the two rumble cases in the .44 hardware checklist remain necessary.
