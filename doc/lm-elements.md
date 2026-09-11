# Native Poltergust tank selection

This is a clean-ISO replacement for selecting an elemental ghost in GaddWarp,
not a port of its physical ghost or medal spawning. It fills/empties the live
tank only; medal ownership is unchanged. No event flags, money, ghost
records, pickups or memory-card save are written.

Clean GLMJ01 DOL SHA1: `722005ea9c1eab54b114f814734d8f327e5614ee`.

The registry getter `8006916C` indexes `803306D0 + actor * 1C`. Entries `72`,
`73`, `74` hex name `elfire`, `elice`, `elwater`. Pickup handling at
`800AE018..800AE08C` maps them to tank types 2, 4, 3 respectively. Type 1 is
empty. The same path converts the u32 capacity at `8049B7AC` (retail 100) to
float, writes it to Player+`1188`, then writes the type to Player+`1184`.

Native depletion resets the same pair to fuel 0/type 1 at `800AF548/554`.
HUD production reads the pair at `800B6D64`, `800B6DA0`, `800B6EB4` and
publishes the type to the HUD structure. The weapon state at Player+`1180`
is separate: native input selects 0 for idle, 1 for vacuum, 2..4 for spraying
and other action values. The menu refuses while that state is nonzero; it
does not force-cancel weapon actions, clear sound handles, or reset emitters.

The action rechecks state readiness, no active native warp/event, bounded
GAME player storage, exact Player vtable `8034EE50`, positive health and a
valid existing tank type. It validates the capacity, then writes only the pickup
pair. A new game update performs normal
HUD/action reconciliation. The choice is menu-local, not a permanent lock;
future native pickups and depletion work normally.

The previous 43/44/45 gate was an incorrect ownership proxy: native
`800D0F40` maps the first-element actors `elffst/elifst/elwfst` to these event
completion flags. A runner's completed save could collect fire normally while
our menu rejected it. The practice tank preset now has no story-flag dependency;
it does not claim to check medal ownership or grant a medal. Selecting a tank
before collecting its medal is intentionally supported for practice, without
changing acquisition/progression records. The UI calls this **Tank preset**.

Native planning tests cover all four choices without story-flag dependencies,
non-idle actions and invalid capacity/choice without partial writes. Retail
tests authenticate the DOL, registry strings, mapping, capacity, stores and
HUD/depletion consumers. Hardware element effects still require testing.
