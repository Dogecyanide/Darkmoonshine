# Moonshine Luigi's Mansion 0.3.44 — focused runner tests

Japanese LM, completed Hidden Mansion file. Install the whole matching .44
app. Make fresh .44 states and SD archives; earlier-build states are not
compatible. Keep your settings and archive_key0/1.bin. No need to repeat the
already-passed deletion and timer-positioning tests for this build.

D-pad Left saves, Right loads, Down opens the menu. SD Import fills the memory
slot; close the menu and press Right to restore gameplay. Never remove the SD
while an operation is pending.

1. **Fresh named archive and cold boot.** Save in Parlor, export as `parlor44`,
   and check for REBOOT READY. Fully power the Wii off/on, enter the same file,
   import that original archive and load it. Walk around and use the Anteroom
   door. Keep this original archive unchanged for the following tests.

2. **Exact crashing route.** Import/load the original Parlor archive. Menu-warp
   to Storage, load Parlor, menu-warp to Boneyard, load Parlor, then walk through
   the Anteroom door normally. Walk back into Parlor and load once more. Report
   exactly which load or door action fails, even if several earlier ones pass.

3. **Repeated reuse without resaving.** With that same original Parlor state,
   repeat Storage warp → Parlor load → Boneyard warp → Parlor load → normal
   Anteroom door twice. Do not replace the snapshot between repetitions. Look
   for delayed crashes, missing actors or doors, not just a correct first frame.

4. **Reverse direction.** Make a separate memory state in Boneyard. Menu-warp
   to Parlor, load back to Boneyard, then move around and use an available normal
   door. Load that Boneyard state again. Report the actual door/rooms used.

5. **Load while rumbling.** Save while the controller is quiet. Trigger a
   normal rumble event, then load while it is still rumbling. The old rumble
   should stop at the load. Trigger another natural rumble: it should still
   work and end normally. Repeat this twice if possible; name the event used.

6. **Snapshot taken during rumble.** If practical, save during an ongoing
   rumble, wait for it to end and load that state. It must not leave the motor
   stuck on. A new natural rumble should still work afterward. A momentary
   new rumble caused by the restored gameplay itself is different from a motor
   that never stops; tell us which you observe.

7. **Door-transition refusal.** With a valid state already saved, request a
   load while actively going through a door. It should safely refuse/defer
   rather than corrupt the transition. Once fully idle, load the same state
   and then use a door normally. Report any stuck rumble or controls as well.

On any failure, stop that route and preserve the entire `lm_dumps` folder plus
`luigis_mansion_crash_a/b.bin` and `.txt`, ideally before further attempts
overwrite evidence. Send the original named archive and its name sidecars if
the failure involves SD import; do not publish archive keys. The eight rotating
attempt journals are diagnostic logs, not playable savestates. Note whether
the game refused, froze immediately, displayed a loaded frame, or crashed later.

The prior .43 Storage→Parlor fix, cold-boot baseline, deletion and faster
positioning passed user Wii tests. This checklist targets the remaining
multi-warp/door crash and rumble mismatch; it does not certify every mansion
or boss boundary. SD operations require Wii, not Dolphin.
