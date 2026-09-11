# R-pump display

The Displays toggle remains off by default. When enabled, R-pump uses an
independent right-middle popup, not a row in the persistent bottom overlay.
The popup is 94 by 21 JUT logical units (188 by 42 pixels in the 640 by 480
framebuffer). It shows the current R hold, then its completed count for 90
game updates. Idle startup and expired results draw nothing.

The press sample counts; the release sample does not. These are completed
game updates, not VI retraces. Menus, invalid pad samples, player changes,
saves and loads cancel partial holds and dismiss the popup. An R release
must be observed before a new measurement starts after those discontinuities.
An extreme counter is capped visually at `99999+`; the stored count saturates
at unsigned maximum.

## Input audit

The display reads the raw port-1 PAD sample before the mod menu neutralizes
the retail sample. It uses only the R digital bit (`0x20`) or R analog byte
(`PADStatus +7`, threshold 30). L's digital bit is `0x40`; L analog is `+6`.
The threshold is the existing raw timing-reference threshold, not a claimed
engine-verified vacuum or trick-success condition.

The clean Japanese DOL SHA-1 is
`722005ea9c1eab54b114f814734d8f327e5614ee`. Authenticated instructions:

- `801D20B0/801D20B4` pass the raw status array into the hooked PADRead.
- `801E4C10/801E4C14` advance returned PADStatus pointers by 12 bytes.
- `801E5294..801E52A4` decode the high trigger byte into status `+6` and the
  low trigger byte into `+7` in analog mode 3.
- `801E5084..801E50B0` independently map `+6` to button `0x40`, and `+7` to
  button `0x20`, in the legacy-spec conversion.
- Nintendont's `PADReadGC.c` likewise obtains L from the high trigger byte
  and R from the low byte. Its `global.h` has the same digital masks.

The fields themselves are correct, but the wire mode was incompatible with
older Phob 2 firmware; see [PAD mode audit](lm-pad-analog-mode.md). The patch
now requests mode 3 through the retail initializer, fixing the protocol at
its source instead of swapping L/R only in our display. Input display also
shows raw `Lxxx Rxxx bbbb` below the controller: analog values followed by
the hexadecimal digital button mask. This small diagnostic appears only
when Input display is enabled. A disconnected port is labeled instead.

The earlier permanent bar was unconditionally drawn while its setting was
enabled, even without an R measurement. No counter or bit-mask change is
needed for L/R after matching the wire format; simultaneous L+R remains valid.
The Phob symptom is reproduced by the protocol test, not yet verified on Wii.

## Checks

`test_lm_r_pump.py` authenticates the retail instructions, raw capture order,
default-off appended toggle, independent popup rendering and update-only
sampling. The native `test_lm_timing.cpp` harness uses the project's actual
PADStatus type and checks its size/offsets, every L/R analog byte value,
isolated digital bits, simultaneous L/R, frame counts, popup expiration and
cancellation. These are source/native tests, not a Wii runtime pass.

For hardware: enable the toggle, wait with both triggers released, press L
alone (no new popup), then R briefly and longer (current/final counts). Wait
until the final count disappears; L alone must not reopen it. Open the menu
or load a state during a hold and confirm no partial count survives.
