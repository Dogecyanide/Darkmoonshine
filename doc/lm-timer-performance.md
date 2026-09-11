# Sunshine timer rendering performance — RC2

The RC1 timer resampled the tiled Sunshine artwork, interpolated RGBA, tinted it,
converted it to YCbCr and blended it into the XFB on every presentation. It also
walked the transparent corners of every rotated bounding rectangle and flushed
all 614,400 framebuffer bytes. The clock read itself is not this rendering cost.

## Changes

- The renderer clips each scanline in its original fixed-point stepping domain.
  That preserves edge-pixel rounding at every supported angle and scale.
- Two-channel packed interpolation replaces four separate channel iterations.
  Untinted 100%-brightness pixels skip redundant colour multiplication.
- A fixed, heapless 64 KiB raster cache stores converted YCbCr/alpha pairs and
  compact row spans for the ten native panes. The keys add 680 bytes; the complete
  cache is 66,216 bytes inside the already reserved mod blob, not GAME/SYS/MEM2.
- Unchanged panes reuse their converted pixels. Changing a digit, position,
  scale, tint, brightness, opacity or surface dimensions rebuilds that pane.
  Each presentation still blends against its own fresh framebuffer. No XFB
  pointer, background pixels, game heap pointer or GPU resource is cached.
- Transparent pairs do not write the framebuffer. Dirty-row flushing includes
  independently moved streak/background padding and the Creation target arrow.

Cache reservations are 4 KiB for each digit, 2 KiB for each mark, 8 KiB for TIME,
and 28 KiB for the streak. All native-size artwork fits, including every digit
and both pixel-pair alignments. Larger custom panes can exceed their fixed slot:
the incomplete stream is never published, and that pane uses the exact optimized
direct renderer instead. Repeating the same oversized key does not rebuild a
failed cache. Large custom sizes can therefore still be expensive; this is not
a promise of equal cost at 200% size.

## Measured locally

`scripts/test_lm_timer_render.py` compiles both the frozen RC1 implementation and
the production renderer as native test libraries. A representative run over 180
presentations, with centiseconds advancing at 30 Hz, measured:

| Path | Host milliseconds/presentation |
| --- | ---: |
| RC1 reference | 0.771 |
| Optimized direct | 0.509 |
| Cached, running timer | 0.085 |
| Cold cache rebuilt every presentation | 0.526 |

These are host measurements, **not Wii frame-time results**. The deterministic
work counters show the running-cache benefit independently of host clock noise:
texel decodes fall from 12,181,680 to 1,342,940; colour conversions from 2,004,934
to 216,078. Blended pixel pairs remain identical at 1,042,348. The default timer
flush drops from 614,400 bytes to 122,880 bytes (96 physical rows).

Tests compare complete output byte-for-byte against the RC1 reference, including
cold and warm caches, every digit, clock rewind, custom colour/opacity/brightness,
offscreen clipping and 25–200% geometry. A nonuniform changing background proves
cached panes do not replay old XFB content. Sixty-five decorated layouts check
that every changed byte falls within the reported flush rows. Guard bytes catch
out-of-frame writes. The independent Creation tests remain unchanged and pass.

## Scope and hardware validation

Ouroboros ultimately replaced its software timer with native GX textured quads;
Moonshine edits Sunshine's existing J2D panes. This update keeps LM's current CPU
compositor and does not claim those GPU implementations have been ported.

Native LM pause/screenshot paths can reuse the actively scanned XFB. Reducing CPU
work reduces the time spent touching it, but does not by itself eliminate that
presentation race or establish that all reported flicker is fixed. Presenter-mode
handling and Wii tests are separate from these renderer parity/performance tests.
Compare the timer on/off at its default size in the same room and camera position;
also test native pause/Z screens and large edited timers. Do not describe the
approximately ninefold host result as a ninefold Wii speedup.
