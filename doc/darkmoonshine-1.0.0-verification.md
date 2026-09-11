# DarkMoonshine V1.0.0 release verification

Release: **V1.0.0 Frozen in Time**, prepared 2026-09-11.

## Acceptance and scope

Dogecyanide reported all ten RC4 Wii checklist items worked. This includes
repeated/busy-room states, cross-floor and post-warp returns, the fragile
Parlor/Storage/Boneyard/Anteroom route, safe transition rejection, failed-import
preservation, named archive switching and repeated soft/full-reboot use.
The final promotion changes branding, documentation and release packaging,
not the RC4 gameplay implementation. No assistant-run final-binary Wii or
Dolphin gameplay acceptance is claimed.

## Local verification

- Final host suite: **1,054 tests, no failures, errors or skips**. Six release
  workflow tests supplement the prior 1,048. After the suite, the same six
  workflow tests also passed explicit SD-source and author verification.
- Wii launcher/full payload and emulator payload compile and link.
  Wii resident size remains **309,357 bytes**; emulator **288,429 bytes**.
  Padded manifest sizes are 309,360 / 288,432, both below the 320 KiB ceiling.
  The Wii payload still has 18,323 bytes free.
- Both manifests retain 27 writes, 33 original-word checks and 0x82000 arena
  reservation. Snapshot format 28, storage protocol 6 and preferences
  version 2 remain unchanged. Final branding changes authenticated identity;
  fresh V1.0.0 archives are required despite the same schema.
- Exact Wii ZIP verification, also used by the release workflow, checks ten
  entries, source bytes, final name/version/authors, both codec licenses,
  theme assets and the authenticated GLMJ01 payload extent/CRC.
- The Dolphin development ZIP contains six verified files, including only
  the BPS, documentation and codec notices. The BPS builder verified clean
  GLMJ01 DOL SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee` and target
  ISO CRC32 **5135D5FB**. No retail ISO is in the release ZIPs.
- Private archives, keys, dumps, settings, build trees, the extracted
  toolchain and personal SD handoff scripts are excluded from the PR.
  Supplied theme and Sunshine timer artwork are intentionally included with
  their existing provenance; no claim of wholly original artwork is made.

## Locally packaged artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii ZIP | 4921610 | `ef6b09ed16ceb022c51fa59ee3e7e1566dfb6eb4653b790ba23a55d67e1f6ff4` |
| Dolphin ZIP | 115302 | `78190d7a76daf279f418af7da186aa931d95f112f84fd066161e65925d48feea` |
| Wii mod_lmj.bin | 310116 | `f313e349f11f1a225e1d4d7768e6a8346429677d813383e0c41db6b299d39196` |
| Wii boot.dol | 1568192 | `7813e9c795b0f4d403535db31af96592c5b3334cf000a524ddb9145e716aaeda` |
| Dolphin BPS | 288708 | `8dc1cafa6af6412bd379200e0bc4f166f0794035e46b5e0949d6a4aa41cad48e` |

ZIP names use `DarkMoonshine-1.0.0-Frozen-in-Time`; the emulator ZIP adds
`-Dolphin`. These are local package hashes; ZIP timestamps or a separate
build environment may change archive bytes. The release workflow now builds
the complete mod, not the former bootstrap-only app, explicitly packages
release notes/checklist, and verifies contents before uploading.

## Remaining limits

One RAM slot remains; the measured second complete state does not fit the
safe separate region. Secret Altar/event and other owner/transition guards
remain active; no universal boss-boundary compatibility is asserted. Dojo,
rush modes, infinite health and verified trick-success judgements are not
newly added by release promotion. The host codec speedup is not a measured
end-to-end Wii speed multiplier. See the release notes and benchmark details.
