# Japanese player room identifier

Player `+0xB4` is a packed 32-bit location word, not a room ID. The Japanese
retail room-table consumer at `800DB98C` loads that word and `800DB990`
(`5404063E`, `clrlwi r4,r0,24`) explicitly extracts its low eight bits before
calling `8002DCE4`. The caller obtains the player through its manager's `+808`
field at `800DB980`. The table-query callee masks the same byte again at
`8002DCFC`.

These instructions were read from the user's clean GLMJ01 revision-0 ISO;
the complete DOL SHA1 was verified as
`722005ea9c1eab54b114f814734d8f327e5614ee`.

The retail transition observer at `80092318..80092324` separately tests the
complete packed word for `FFFFFFFF` before consuming it. The helper therefore
returns `-1` for that exact unset sentinel, otherwise the unsigned low byte.
There is no added claim that every word ending in `FF` is the same sentinel.

| Reported packed word | Decoded room |
| --- | --- |
| `2A010427` | 39, Anteroom |
| `0E00040E` | 14, Storage |
| `24010323` | 35, Parlor |

Warp settling now compares the decoded byte with the destination room and
waits while the full-word sentinel is unset. Settling/rejection telemetry keeps
the original packed word for diagnosis; the successful-arrival event and public
room accessor use the decoded ID. Metadata uses the same helper.

Six native/source-contract tests cover the three hardware samples, every byte,
10,000 deterministic upper-field variations, the exact unset sentinel, the
verified instruction's extraction mask, and both consumers. This fixes false
room mismatches and misleading room metadata; it does not change snapshot
capture, restore destinations, collision state or particle handling.
