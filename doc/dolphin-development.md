# Luigi's Mansion Dolphin development

The Wii release remains a clean-ISO Nintendont launcher. Dolphin testing uses a
separate payload compiled with `LM_EMULATOR=ON`: snapshots and compressed slot
storage use Dolphin's fake VMEM aperture. The ARM SD-file service is unavailable
in this build. Dolphin results do not prove Wii timing, cache, or SD correctness.

Configure once with your own full-size Japanese revision-0 disc image:

```powershell
cmake --preset diagnostic_emulator -DLM_ISO="X:/path/Luigi's Mansion (Japan).iso"
cmake --build --preset emulator
```

On a host where Ninja is not on PATH, additionally set
`-DCMAKE_MAKE_PROGRAM="<repository>/venv/Scripts/ninja.exe"` during configure.

The target writes `build-lm-emu/moonshine_lmj_dolphin.iso` and a matching `.bps`.
The ISO is local test material and must not be distributed. The patch contains
injected code and changed words; all retail content comes from BPS source reads.
The build authenticates the `GLMJ01` revision-0 header, the retail main.dol SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`, every hook's original instruction, and
all three BPS checksums. Source and output paths must differ. The emulator CMake
configuration deliberately does not expose the Nintendont packaging target.

Use a new isolated Dolphin user directory to protect normal profiles and cards:

```powershell
venv/Scripts/python.exe scripts/prepare_lm_dolphin.py --profile build-lm-emu/dolphin-user --save "path/to/Hidden Mansion.gci"
Dolphin.exe -u "<repository>/build-lm-emu/dolphin-user" -e "<repository>/build-lm-emu/moonshine_lmj_dolphin.iso"
```

The preparation script validates and copies a GLMJ01 GCI into card A; it refuses
nonempty profiles. It never formats an existing card. Omit `--save` for a fresh
test profile. Any first-launch privacy choice belongs to the user.

The installed Dolphin 5.0 needs **Real XFB** (`UseXFB=True`, `UseRealXFB=True`) to
show the CPU-drawn memory HUD and practice menu. Fake VMEM requires `MMU=False`.
These settings are generated only in the isolated profile. Keyboard mapping is
A=X, B=Z, main stick=WASD, L/R=E/R, and the normal D-pad arrow keys.

For repeatable held inputs, `scripts/write_lm_dtm.py` writes Dolphin 5.0 DTM
movies from JSON steps such as `{"polls":30,"buttons":["A"]}`. Steps may specify
`stick`, `cstick`, and `triggers` as two-byte arrays. Counts are controller polls,
not video frames: LM changes update rate between screens. The movie fingerprints
the generated ISO and never requests clearing memory-card data. A movie started
from an emulator savestate requires its matching `.dtm.sav` and `--from-state`.
Replay with Dolphin's `-m <movie.dtm>` option. Such inputs exercise the game and
mod naturally; they do not substitute for observing the resulting behavior.
The generator avoids short absolute frame/tick caps because native savestates
restore those counters; the controller-record count ends the movie instead.

### Dolphin 5.0 movies and save cards

Normal boot uses the prepared `SlotA=8` GCI folder at `GC/JAP/Card A`. Movie
playback is different: Dolphin 5.0 overrides slot A with a RAW memory card when
DTM byte 151 has bit 0 set. It cannot request the folder backend. Consequently,
a valid Hidden Mansion GCI can be present in the profile while movie playback
still shows three empty save slots. Stop the movie and boot the ISO normally
to use the prepared GCI folder. This is an emulator test-setup issue, not proof
that the mod erased a save.

The movie writer now requires one explicit card choice:

- `--raw-card "path/to/MemoryCardA.JAP.raw"`: read-only preflight of an existing
  Japanese RAW card containing a GLMJ01 save. Configure Dolphin's RAW card A to
  that same path separately; DTM stores no card filename and the helper does
  not change configuration, import saves, or create cards. Stop emulation
  before checking the card. Keep this card inside the isolated test profile.
- `--allow-empty-card`: deliberately bypass the save check, for example for
  title-screen tests. This does **not** ask Dolphin to clear a save or create a
  fresh movie card; normal playback may still create its default RAW file if
  one is missing. Do not expect the GCI-folder completion file to be available.

These choices also apply with `--from-state`: a matching `.dtm.sav` is still
required, and merely starting from a native state is not treated as proof of
the intended card backend. The RAW preflight checks file size, Japanese
encoding, metadata checksums, the active GLMJ01 directory entry, and its block
chain. It does not validate LM's internal save checksums or confirm 100%/Hidden
Mansion progression. It conservatively rejects damaged metadata instead of
repairing either copy. Card contents and profile settings remain untouched.

Both choices retain `bSaveConfig=1`, `memcards=1`, and `bClearSave=0`. Setting
`bSaveConfig=0` is not a folder-card solution: Dolphin 5.0's `GetSettings()`
recognizes only RAW devices for its movie card bitmask, then enables saved
config again. The source of this behavior is the official 5.0
[EXI initialization](https://github.com/dolphin-emu/dolphin/blob/5.0/Source/Core/Core/HW/EXI.cpp),
[movie configuration](https://github.com/dolphin-emu/dolphin/blob/5.0/Source/Core/Core/Movie.cpp),
and [memory-card backends](https://github.com/dolphin-emu/dolphin/blob/5.0/Source/Core/Core/HW/EXI_DeviceMemoryCard.cpp).
The structural preflight follows the official
[card layout](https://github.com/dolphin-emu/dolphin/blob/5.0/Source/Core/Core/HW/GCMemcard.h)
and [metadata selection/checksums](https://github.com/dolphin-emu/dolphin/blob/5.0/Source/Core/Core/HW/GCMemcard.cpp).

Run the helper regression tests with
`venv/Scripts/python.exe -m unittest scripts/test_lm_dtm.py scripts/test_lm_emulator.py`.

On Dolphin, the crash report remains available at `0x70FF3800` and phase trace at
`0x70FF4000` for debugger inspection. No ARM worker writes `/lm_dumps` or SD crash
files. On Wii those files still use the normal Nintendont transport.

For offline diagnosis, capture a new native Dolphin save slot without replacing
a user's checkpoint. `scripts/extract_lm_dolphin_state.py --state <GLMJ01.sNN>
--output <new-directory>` decodes Dolphin 5.0 Win64's LZO blocks (optional `lzokay`
Python package) and validates the memory-bank markers before extracting
`mem1.bin` at base `0x80000000` and `fake-vmem.bin` at base `0x70000000`.
It requires a committed LM snapshot in fake VMEM and refuses a nonempty output
directory. These files contain retail runtime memory and must not be shipped
with the release. Never load an old native Dolphin checkpoint over a newly
built mod: it restores the old injected code as well as the game state.
