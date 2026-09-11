# Moonshine input display in Luigi's Mansion

`lm_input_display.cpp` ports the controller diagram from Moonshine's
`src/input_display.cpp`, including its 182 x 120 design, palette, 32-point
circular buttons, octagonal stick gates, moving stick knobs, and analog
trigger bars. Buttons are outlines when released and filled when held.
Digital L/R clicks extend their bars beyond the analog-travel length.
The original design credits sup39's controller display, as represented by
BitPatty's Apache-2.0 gct-generator; the Moonshine renderer and this port are
native drawing implementations, not replayed Gecko patches.

The live PAD sample comes from `LMTools::samplePad`, before menu input masking.
A disconnected pad renders neutral rather than holding the last sample.
The diagram is displayed at Moonshine's default physical position (16,314)
on the 640 x 480 XFB, and is hidden while the practice menu is open.
Other telemetry moves to its right while it is enabled. This port retains
the existing Input display On/Off menu setting; Moonshine's visual-style
editor and optional numeric readouts are not included.

## Renderer and cache ownership

LM does not share Sunshine's J2D menu renderer. `diagnosticCopyDisp` already
waits for `GXDrawDone`, validates the 640 x 480 framebuffer, and invalidates
the cached XFB before calling the overlay renderers. The diagram draws into
that completed Y0-Cb-Y1-Cr buffer without touching live GX state. It flushes
its row band before the presenter can hand the completed image to VI.

`lm_xfb_draw.h` is the heapless shared CPU renderer. Convex fills are
scan-converted, outlines use line segments, and every span clips to the
validated surface. Pair-shared chroma is blended once using pixel coverage,
so drawing one pixel does not overwrite its neighbour's luma or blend the
same colour into both pixels twice. Fully covered opaque pairs use four
byte stores without destination reads or blend arithmetic; only an odd
left/right boundary needs neighbour-preserving chroma blending.
Shapes allocate no heap or temporary
framebuffer. Polygon coordinates are bounded to avoid signed arithmetic
overflow on malformed input.

`LMDraw::fillBox` and `LMDraw::flush` use JUT's 320 x 240 logical coordinates
for the practice menu. `fillPoly` and `strokePoly` use physical XFB pixels
to retain Moonshine's precise diagram proportions.

## Verification

`scripts/test_lm_input_display.py` compiles the actual runtime diagram with
its cache call replaced by a no-op. Eight native tests exercise released and
pressed buttons, moving sticks, analog-versus-click trigger travel,
disconnected controllers, YUYV pair blending, clipped shapes and sentinel
bytes surrounding the entire framebuffer. Three hundred randomized opaque
fills compare both edge parities against the original pair-blending formula.
Its preview is rendered from
the same YUYV output into `build-lm-diag/input-display-preview.png`.

The native preview verifies the drawing path, not console rendering speed.
Dolphin/Wii testing remains necessary for final video and pad integration.
