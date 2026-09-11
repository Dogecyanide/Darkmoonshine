# Player/effect query cache snapshot boundary

This audit uses Japanese retail `main.dol`, SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`. Native instruction and optional
private-capture evidence is checked by `scripts/test_lm_player_query_cache.py`.

Capture exactly **`803CC718..803CC818`**, 0x100 bytes. This adds coherent
gameplay cache state, not a demonstrated explanation for the reported crash.

| Bytes | Native purpose |
| --- | --- |
| `803CC718..803CC724` | Three-float vector scratch |
| `803CC724..803CC730` | Second three-float vector scratch |
| `803CC730..803CC790` | Primary player/effect query record |
| `803CC790..803CC7F0` | Secondary query record |
| `803CC7F0..803CC810` | Query distance, result vector, flags, native member-function descriptor |
| `803CC810..803CC818` | Self pointers to the two records |
| `803CC818..803CC824` | **Excluded runtime destructor-registration record** |

The vector producer at `8013C20C..8013C254` copies two three-word vectors
from a gameplay object. These are value scratch, not pointer owners.

The plain-cache static initializer `80143E30` clears the record flags at
`CC730+5B` and `CC730+BB`, then writes `CC730+E0 = CC730` and
`CC730+E4 = CC790`. Its final store is at `80143E4C`; it registers no
destructor. The member-function descriptor at `+D4..+DC` comes from native
read-only data at `80143C60..80143C8C` and is dispatched by the native
member-call helper `801F58D0`, not by an OS or audio callback queue.

## Rebuilt pointers versus retained state

The five uncovered GAME pointers found in the older capture were at
`CC730`, `CC74C`, `CC750`, `CC768`, and `CC76C`. Unlike the dialogue picture
owners, they are **borrowed pointers rebuilt during normal gameplay update**:

`8000B9EC -> 80143AD8 -> 8012A5A8`

`8012A5A8` obtains the player descriptor via `800ABD1C` and refreshes cache
fields `+0`, `+1C`, `+20`, `+38`, and `+3C` from descriptor fields
`+0`, `+98`, `+9C`, `+34`, and `+38` respectively. It also updates numeric
direction, magnitude, position, and gameplay flags. The secondary record is
updated by `8012A748`; that function can return early when its source is
inactive, retaining other record fields. The wrapper does not re-run the
static initializer or reset every scratch/flag/member-descriptor field.

Cache consumers include `8014298C` (for example, the `+E0 -> +1C` position
chain at `80142A30..80142A3C`) and the numerous callers of accessors
`80143E10` / `80143E20`. The cache is gameplay-side value/pointer state;
there is no allocation ownership, thread, mutex, DVD command, or destructor
link inside the recommended span. Keeping all its bytes with the restored
heap avoids a partially future-epoch cache before or around the next refresh.
This audit does **not** prove a stale-pointer dereference before that refresh
in any particular reported crash.

Both private 0.3.30 captures have different values for all five borrowed
pointers while the two self links remain identical. That demonstrates epoch
dependence but does not, by itself, demonstrate corrupt pointed-to contents.

## Hard exclusion boundary

The next initializer, `80146A30`, uses `r5 = 803CC818`, `r3 = r5+0C`, and
`r4 = 80146A84`, then calls the runtime registration helper at `80146A70`.
`801F51C8` writes the linked-list predecessor, destructor address, and object
address into `[r5, r5+0C)` and publishes the record to `r13+1840`.
Consequently `803CC818..803CC824` must stay live. Do not extend the snapshot
range through it to the next object or model-effect manager. This audit
makes no capture recommendation for `803CC824..803CC998`.
