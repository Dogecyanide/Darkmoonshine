# Native clock behind the Sunshine timer display

The overlay reads the Japanese game's frame counter, not its accelerated
in-world clock. It is a game-time counter (30 game updates per second), not a
real-time or lag-inclusive speedrun timer. Conversion rounds to the nearest
centisecond; the six-digit Sunshine presentation saturates at 99:59.99.
The unmodified native updater itself rolls its world clock and this counter
over at 64,800 game updates (36 gameplay minutes). This adapter preserves
that behavior; it is not an unlimited full-run stopwatch.

## Authenticated GLMJ01 evidence

The source DOL SHA-1 is `722005ea9c1eab54b114f814734d8f327e5614ee`.
`r13` is `0x804A0AE0`.

| State / routine | Address | Meaning |
| --- | --- | --- |
| Active word | `804993C0` | Native `TIMESTOP` writes zero, `TIMEACTIVE` writes one |
| Frame count | `804A1288..804A1290` | Unsigned 64-bit high/low pair |
| Clock update | `80061A48` | Adds one to the pair only when the active word is nonzero |
| Update call | `8000B9B8` | `bl 80061A48`, original word `48056091` |
| Reset | `80061A2C` | Zeros the frame pair and three derived world-clock fields |
| Reset calls | `8000B880`, `8000C084` | New-game setup and map-2 load request respectively |
| `TIMESTOP` | `800646A4..800646AC` | Event command 81 |
| `TIMEACTIVE` | `800646B0..800646B8` | Event command 82 |

`TIMERSTOP`, event command 120, is a separate elevator/wipe delay and must
not be treated as the stopwatch's stop event.

The existing savestate ranges already contain both the active word (game
sdata) and the frame pair (game SBSS). The adapter does not maintain a second
elapsed-time accumulator, retain game pointers, or change snapshot format.
A successful in-memory or SD restore therefore supplies the timer value too.

The original clock call at `8000B9B8` remains unhooked. The practice menu owns
controller input, not the game simulation, so opening the D-pad-down menu must
not stop an active clock. RC1 incorrectly suppressed updates there; RC2 removed
that suppression. The native-menu option below never borrows or overwrites the
active flag, and existing scripted stops continue to take priority.

## Native-menu option

`Displays / Sunshine Timer / Run in native menus` defaults to **OFF**, preserving
RC2 and retail behavior. It persists as preferences value 46. OFF stops while
using START pause, the Y map or Z Game Boy Horror; ON includes their game updates
in the same native timer. The D-pad-down mod menu is excluded from this option:
it keeps an active timer advancing regardless of the setting, including if it
is opened over one of those native menus. A Boo/script `TIMESTOP` remains stopped.

The native UI discriminator is the word `804A0C44` (`r13+164`), not the shared
single/double-XFB flag. Authenticated Japanese paths establish its ownership:

| Owner | Entry proof | Native update branch |
| --- | --- | --- |
| 1: START pause | `8000C680` tests pad mask `1000`; `8000C690` stores 1 | `8000C6C0` compares 1; pause object initialized at `8000C6DC` |
| 2: Y map | `8000B964` tests `0800`; `8000C30C` stores 2 | `8000C38C` compares 2; map initialized at `8000C3CC` |
| 3: Z Game Boy Horror | `8000C808` tests `0010`; `8000C86C` stores 3 | `8000C8D8` compares 3; interface initialized at `8000C8A0` |

`8000B98C..8000B9B4` reaches the retail clock only for control owners 0, 4 and
FF; native UI branches return before it. Consequently, hooking the clock call
alone cannot enable timing in native menus.

The entry word at `8000B918`, originally `7C0802A6` (`mflr r0`), instead branches
to `diagnosticTimerGameUpdate`. A five-instruction naked trampoline replays that
one instruction and joins the untouched retail body at `8000B91C`, preserving
its return value and LR/stack ABI. After the retail update, the wrapper calls
the original `80061A48` exactly once only when all these conditions hold:

- the completed update is still owned by native UI 1, 2 or 3;
- scene, loop scene and owner scene are all 2, with no scene transition;
- the native active flag is exactly one;
- native-menu timing is enabled, or the D-pad-down mod menu is open;
- both words of the native counter are unchanged by the retail update.

The count comparison prevents duplicate updates on a native tick, reset or
rewind; UI exits that return to normal gameplay use the existing retail clock.
There is no second elapsed-time accumulator and no write to native pause,
controller, XFB mode, or TIMEACTIVE flags. The original clock routine maintains
its derived fields and existing 36-minute wrap consistently.

Tests authenticate the menu branches, entry instruction and original call;
compile the PPC trampoline and check its exact emitted instruction sequence;
exercise all owner/active/setting/mod-menu combinations; reject transitions,
count changes and native stops; and validate the fifth menu row/persistence slot.

## GaddWarp findings and fidelity boundary

Recovered GaddWarp JP 2.2 uses the same `TIMESTOP` and `TIMEACTIVE` commands.
Examples in `build/gaddwarp/extracted/patched/Event`:

- Event01 main menu: stop at entry; active again in the `exit` branch.
- Event08 settings: stop at entry; active again after returning player control.
- Event29 Boo-capture communication: adds stop at entry.
- Event53 Boo introduction: adds active before its message dispatch.
- Events50, 57--60, 65, 66, 72, 75 and others stop/start during scripted boss
  introductions or transitions, with branch-specific timing.

The clean game's scripts already contain many, but not all, of those commands.
The adapter additionally mirrors Event29's stop and Event53's start, which
gives the GaddWarp Boo-capture result hold until the next Boo introduction.
The call at `8002B5B4` (`48039BA5`) is redirected through
`diagnosticTimerEventStart(void *archive)`. This wrapper calls the original
`void 80065158(void *archive)` first. Successful script loading publishes the
event ID at `803C7CA0` and its text cursor at `803C7CAC`; a null cursor means
no action. Only IDs 29 and 53 change the native flag, at their event-entry
boundary. This happens before their first interpreter pass.

Ordinary doors do not reset this clock. A map-2 room warp/reset uses the
native reset path; loading a saved state restores the saved timer value.
There is no universal room-clear stop in the recovered GaddWarp Event77.

The adapter does **not** claim complete GaddWarp endpoint parity: GaddWarp-only
dialogue/dojo/rush scripts have not been imported by this timer adapter. In
particular, room completion must not be advertised as a universal auto-stop.
GaddWarp Event65 stops earlier in the Cemetery skull cutscene than the clean
Bogmire warp script. Its branch-specific camera/wait boundary is not copied
as an inaccurate stop at event entry. Modified lab Event55 and conditional
Event101 are likewise not substituted without their paired custom flow.

GaddWarp also patches the underlying accelerated world-clock arithmetic:
`80061A70` adds 197 instead of one, `80061A90` multiplies by 30 instead of ten,
and `80061AF8`, `80061B10`, `80061B20` use 99 instead of the retail 60/60/6.
Those values are not needed to observe the native active flag and are not
copied: changing them would distort both the source frame count and the
retail mansion clock. The Sunshine presentation derives ordinary seconds
directly from the unmodified native update count instead.

## Creation positioning controls (0.3.43)

D-pad taps retain the precise two-native-pixel adjustment. Holding a direction
starts continuous movement after six subsequent editor updates, then accelerates
from two to six pixels per update at 15 updates and ten pixels at 30 updates.
At LM's normal 30 updates per second, the initial hold delay is 0.2 seconds and
maximum speed is reached after one second. A complete 640-pixel screen crossing
takes 83 updates (about 2.77 seconds), including the initial press.

The two axes have independent hold counters. Releasing or reversing a direction,
disconnecting the controller, opening a confirmation, or starting a new editor
resets its acceleration. Opposing directions cancel rather than moving away from
a screen edge. C-stick colour/value editing and L/R size adjustments retain their
existing repeat rate; holding or changing the D-pad cannot speed them up.

The same positioning controls apply to both the whole timer and its streak.
The visible hint reads `D-pad: tap fine / hold fast  L/R: size`.
The executable Creation harness covers exact thresholds, both screen-crossing
directions, precise taps, release/reversal, axis independence, bounds, confirmation
return, disconnect/reconnect, and unchanged colour/size repeat behavior.
