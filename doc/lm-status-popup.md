# Brief action notices

The permanent memory/version panel and checkerboard heartbeat are replaced by a
small top-left notice: Saving, Saved, Loading, Loaded, Busy or Rejected. It lasts
60 game updates (about two seconds during normal gameplay). Each explicit attempt
restarts the duration, including another attempt with the same result. Passive
stability/streaming gates do not create recurring Busy messages. The menu keeps
its detailed status information; while open it takes priority over the popup.

The rectangle is 112 by 22 physical pixels, inset 24 pixels from the left and
32 from the top for overscan. No renderer assets or heap allocation are needed.
Expiry stops drawing; the next retail framebuffer copy restores the game image,
so there is no large erased panel or stored-background buffer.

For synchronous save/load, the start notice is painted onto the current
presenter's completed framebuffer immediately before the transaction. That
surface is validated and GX-complete, and its loan is cleared before and after
each presenter. The explicit flush covers the notice's rows; retail
JUTDirectPrint also flushes its framebuffer after text, as it did previously.
Outcomes are not painted
through the loan after restore; they appear in the next normal framebuffer copy.
This adds neither a blocking delay nor another game update. A very quick save or
load may make its start message fleeting; no hardware screenshot test is claimed.

State failures map transient availability failures to Busy, and incompatible,
empty, corrupt or unsafe snapshots to Rejected. Exact rejection codes remain in
the menu and journal. Warp starts/phases/completion use Loading/Loaded, with a
brief Busy/Rejected on an explicit failure. There is no automatic busy heartbeat.

All floor/canary sampling, periodic heap validation, detailed critical journals
and crash capture remain enabled. First floor/canary failures additionally enter
the breadcrumb ring as events `0x130`/`0x131`, now that the old BAD text is gone.
The popup's own state lives in the injected mod's memory and is not snapshotted.

Host tests: `scripts/test_lm_status_popup.py`, with the existing diagnostic and
input display tests updated to assert the permanent panel's removal.
