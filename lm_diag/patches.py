"""GLMJ01 revision-0 hooks and the proven MEM1 diagnostic reservation.

This deliberately has no relationship to scripts/patches.py, which remains
the inherited Super Mario Sunshine patch table.  The launcher build consumes
this file only when LM_DIAGNOSTIC is enabled.
"""

from enum import Enum


class PatchType(Enum):
    B = 1
    BL = 2
    W32 = 3


# Every modifying write carries its clean-GLMJ01 word.  The launcher kernel
# preflights the complete set before copying any code or applying any branch,
# so a different revision fails closed rather than partly injecting.
patches = [
    # Preserve the retail appearance-slot and player-construction path while
    # a menu warp supplies a scene-owned shadow of one luige placement row.
    {
        "lmj": 0x800E2E08,
        "sym": "lmWarpPrepareAppearance",
        "type": PatchType.BL,
        "expected": 0x48000C15,
    },
    # Replace OSGetArenaLo's two-instruction getter.  A plain branch preserves
    # the caller's LR, so getArenaLo() returns directly to the retail caller.
    {
        "lmj": 0x801D5B5C,
        "sym": "getArenaLo",
        "type": PatchType.B,
        "expected": 0x806DFF38,
    },
    # Wrap both GXCopyDisp calls inside LMChangeFrameBuffer.  They are the two
    # branches of the retail presenter and both pass the completed XFB in r3.
    # The wrapper calls the original copy, waits for it, then draws directly
    # into the 640x480 YUYV buffer with JUTDirectPrint plus a raw heartbeat.
    {
        "lmj": 0x8000776C,
        "sym": "diagnosticCopyDisp",
        "type": PatchType.BL,
        "expected": 0x481E8CF1,
    },
    {
        "lmj": 0x80007828,
        "sym": "diagnosticCopyDisp",
        "type": PatchType.BL,
        "expected": 0x481E8C35,
    },
    # Move the transaction to the main loop's true post-presenter boundary.
    # The preceding wrappers retain hard-lock milestones around the first
    # frame-begin and restored-scene consumers reached after the load.
    {
        "lmj": 0x8000B534,
        "sym": "diagnosticFrameBegin",
        "type": PatchType.BL,
        "expected": 0x4BFFC1A5,
    },
    {
        "lmj": 0x8000B544,
        "sym": "diagnosticMainSceneStep",
        "type": PatchType.BL,
        "expected": 0x4BFFFD05,
    },
    # The outer scene-step marker isolated a hard lock inside fn_8000B248.
    # Bracket its first/last matrix upload, scene draw callback, and final
    # orthographic reset so a no-exception FIFO stall still leaves a boundary.
    {
        "lmj": 0x8000B268,
        "sym": "diagnosticFirstPosMatrix",
        "type": PatchType.BL,
        "expected": 0x481E99C9,
    },
    {
        "lmj": 0x8000B34C,
        "sym": "diagnosticLastNrmMatrix",
        "type": PatchType.BL,
        "expected": 0x481E9921,
    },
    {
        "lmj": 0x8000B35C,
        "sym": "diagnosticSceneDraw",
        "type": PatchType.BL,
        "expected": 0x4E800021,
    },
    # Keep LM's transient-model pool on its retail update path, but validate
    # each active slot first. Cross-epoch cosmetic slots are retired before
    # their model descriptor can be dereferenced by the door-effect update.
    {
        "lmj": 0x800111A8,
        "sym": "diagnosticAnimatedModelPoolUpdate",
        "type": PatchType.BL,
        "expected": 0x480155A9,
    },
    # Validate the selected model/animation at the exact retail callsite. If
    # its relocated data is temporarily absent, skip only this controller
    # update and allow the resource to recover on a later frame.
    {
        "lmj": 0x8002684C,
        "sym": "diagnosticAnimatedModelControllerUpdate",
        "type": PatchType.BL,
        "expected": 0x4BFF8239,
    },
    {
        "lmj": 0x8000B360,
        "sym": "diagnosticOrthoReset",
        "type": PatchType.BL,
        "expected": 0x4BFFC59D,
    },
    {
        "lmj": 0x8000B5EC,
        "sym": "diagnosticPreMainUpdate",
        "type": PatchType.BL,
        "expected": 0x4BFFF6B9,
    },
    {
        "lmj": 0x8000B608,
        "sym": "diagnosticPostMainUpdate",
        "type": PatchType.BL,
        "expected": 0x4BFFC9FD,
    },
    # Close the only non-trivial JAudio gap between the post-update fade
    # controller and the presenter. The following 0x80186868 call is a blr.
    {
        "lmj": 0x8000B618,
        "sym": "diagnosticAudioTailB618",
        "type": PatchType.BL,
        "expected": 0x4817B19D,
    },
    {
        "lmj": 0x8000B62C,
        "sym": "diagnosticChangeFrameBuffer",
        "type": PatchType.BL,
        "expected": 0x4BFFC1BD,
    },
    {
        "lmj": 0x8000B640,
        "sym": "diagnosticConditionalTail",
        "type": PatchType.BL,
        "expected": 0x4BFFC975,
    },
    {
        "lmj": 0x8000B65C,
        "sym": "diagnosticLoopTailSync",
        "type": PatchType.BL,
        "expected": 0x4BFFFD1D,
    },
    {
        "lmj": 0x8000B660,
        "sym": "diagnosticLoopTailClock",
        "type": PatchType.BL,
        "expected": 0x4BFFA7A5,
    },
    {
        "lmj": 0x8000B714,
        "sym": "diagnosticGameLoop",
        "type": PatchType.BL,
        "expected": 0x4BFFFDD5,
    },
    {
        "lmj": 0x8000B728,
        "sym": "diagnosticOuterCleanup",
        "type": PatchType.BL,
        "expected": 0x4BFFF551,
    },
    {
        "lmj": 0x8000B744,
        "sym": "diagnosticOuterRestart",
        "type": PatchType.BL,
        "expected": 0x4BFFA92D,
    },
    # Retail JUTGamePad::init requests analog wire mode 0 (4-bit triggers).
    # Older Phob 2 firmware replies in mode 3 regardless of that request: the
    # mode-0 decoder then mistakes L's low nibble for R and R for analog A/B.
    # Keep both JUT's mode value and PADSetAnalogMode's argument at mode 3.
    # The retail decoder, calibration, clamp and button mapping stay intact.
    {
        "lmj": 0x801D2074,
        "val": 0x38000003,
        "type": PatchType.W32,
        "expected": 0x38000000,
    },
    {
        "lmj": 0x801D207C,
        "val": 0x38600003,
        "type": PatchType.W32,
        "expected": 0x38600000,
    },
    # Intercept the sole PADRead inside JUTGamePad::read. The wrapper preserves
    # its return value while giving the retail button/stick derivation a neutral
    # port-1 sample whenever the payload-native practice menu owns input.
    {
        "lmj": 0x801D20B4,
        "sym": "diagnosticPadRead",
        "type": PatchType.BL,
        "expected": 0x48012849,
    },
    # Supplement the native clock only when retail skipped it for a native menu.
    {
        "lmj": 0x8000B918,
        "sym": "diagnosticTimerGameUpdate",
        "type": PatchType.B,
        "expected": 0x7C0802A6,
    },
    # GaddWarp's Boo clock endpoints, after a successful native event load.
    {
        "lmj": 0x8002B5B4,
        "sym": "diagnosticTimerEventStart",
        "type": PatchType.BL,
        "expected": 0x48039BA5,
    },
]

# Extra clean-DOL signatures that are authenticated but left untouched.  These
# bind the payload to the complete retail getter, the main-loop presenter call,
# LM's final JUTDirectPrint framebuffer handoff, the port-1 PADRead path, and
# the complete four-instruction JUTException callback setter.
checks = [
    {"addr": 0x801D5B60, "expected": 0x4E800020},
    {"addr": 0x80007870, "expected": 0x481CCFC1},
    # Bind the dynamic draw wrapper to sCurScene and Scene::mDrawFn at +0x1C.
    {"addr": 0x8000B350, "expected": 0x806D8038},
    {"addr": 0x8000B354, "expected": 0x8183001C},
    {"addr": 0x8000B358, "expected": 0x7D8803A6},
    {"addr": 0x801D20B0, "expected": 0x387D0018},
    # Bind the wire-mode immediates to JUT's matching global store and the
    # existing PADSetAnalogMode/PADInit call order; neither call is replaced.
    {"addr": 0x801D2078, "expected": 0x900D158C},
    {"addr": 0x801D2080, "expected": 0x48013461},
    {"addr": 0x801D2084, "expected": 0x48012559},
    # PADRead entry called by the practice-menu wrapper.
    {"addr": 0x801E48FC, "expected": 0x7C0802A6},
    {"addr": 0x801E4900, "expected": 0x3C808049},
    {"addr": 0x801E4904, "expected": 0x90010004},
    {"addr": 0x801E4908, "expected": 0x38045910},
    {"addr": 0x801D4124, "expected": 0x800D1594},
    {"addr": 0x801D4128, "expected": 0x906D1594},
    {"addr": 0x801D412C, "expected": 0x7C030378},
    {"addr": 0x801D4130, "expected": 0x4E800020},
    # Scheduler nesting is held across each interrupt-disabled transaction.
    {"addr": 0x801DAE98, "expected": 0x7C0802A6},
    {"addr": 0x801DAE9C, "expected": 0x90010004},
    {"addr": 0x801DAEA0, "expected": 0x9421FFF0},
    {"addr": 0x801DAED8, "expected": 0x7C0802A6},
    {"addr": 0x801DAEDC, "expected": 0x90010004},
    {"addr": 0x801DAEE0, "expected": 0x9421FFF0},
    # JAIBasic::basic assignment and the LM JAAMain scene-change body used to
    # drain prior audio and recreate its required bootstrap sequence.
    {"addr": 0x8018B810, "expected": 0x93ED12F0},
    {"addr": 0x8018D4E4, "expected": 0x7C0802A6},
    {"addr": 0x8018D510, "expected": 0x806301E8},
    {"addr": 0x8018D54C, "expected": 0x800DF94C},
    {"addr": 0x8018D5F0, "expected": 0x901E0064},
    {"addr": 0x8018D5F8, "expected": 0x389D0800},
    {"addr": 0x8018D61C, "expected": 0x38BE0064},
    {"addr": 0x8018D62C, "expected": 0x4BFFF1F1},
    {"addr": 0x8018D630, "expected": 0x801E0050},
    {"addr": 0x804A03A8, "expected": 0x803E3CF8},
]


# GLMJ01's linked arena floor and the retail debug-stack gap were recovered
# directly from the clean DOL (SHA-1 722005ea9c1eab54b114f814734d8f327e5614ee).
arena_lo = {"lmj": 0x804B8400}
base_addr = dict(arena_lo)

# Exact tuple produced by Nintendont's Patch.c DOL parser.  DOLSize uses the
# 0xE4-byte dolhdr struct, not main.dol's 0x100-byte padded on-disc header.
dol_size = {"lmj": 0x00394924}
dol_min = {"lmj": 0x00003100}
dol_max = {"lmj": 0x004A6400}

mod_region_size = 0x80000
debug_stack_size = 0x2000
arena_reserve = mod_region_size + debug_stack_size

# Keep the transport limits identical to Moonshine's established 512 KiB
# layout.  The diagnostic itself is tiny and does not create an attachment
# heap; these values are capacity boundaries, not allocations.
mod_scratch_size = 0x40
mod_mem1_working_cap_size = 0x50000
mod_attachment_heap_offset = 0x50000
mod_attachment_heap_size = 0x20000
mod_blob_max_size = mod_mem1_working_cap_size
mod_file_max_size = 0x5F000
mod_write_count = sum(1 + patch.get("nop_count", 0) for patch in patches)

assert mod_attachment_heap_offset == mod_mem1_working_cap_size
assert (
    mod_attachment_heap_offset + mod_attachment_heap_size
    <= mod_region_size - mod_scratch_size
)
assert (mod_attachment_heap_offset | mod_attachment_heap_size) & 31 == 0

game_id = {"lmj": 0x474C4D4A}  # "GLMJ"
region = {"lmj": "JP"}
disc_name = {"lmj": "GLMJ01"}
