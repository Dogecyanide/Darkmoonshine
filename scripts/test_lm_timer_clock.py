"""Native-frame conversion and authenticated Japanese clock lifecycle anchors."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_lm_hud_state import AuthenticatedRetailTests

ROOT = Path(__file__).resolve().parents[1]


class TimerClockTests(unittest.TestCase):
    def test_portable_conversion_and_native_active_gate(self):
        compiler = ROOT / "toolchain/clang++.exe"
        if not compiler.exists():
            self.skipTest("Bundled Windows clang unavailable")
        with tempfile.TemporaryDirectory(prefix="lm-clock-") as directory:
            exe = Path(directory) / "clock.exe"
            build = subprocess.run([
                str(compiler), "--target=x86_64-pc-windows-msvc", "-fuse-ld=lld",
                "-nostdlib", "-fno-stack-protector", "-Wl,/entry:main,/subsystem:console",
                "-I", str(ROOT / "include"), str(ROOT / "scripts/test_lm_timer_clock.cpp"),
                "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_adapter_supplements_only_proven_native_menus_without_borrowing_flag(self):
        source = (ROOT / "lm_diag/src/lm_timer_clock.cpp").read_text()
        self.assertIn("volatile const u32", source)
        self.assertIn("LMPractice::isOpen()", source)
        self.assertIn("bool sAvailable, sRunning, sRunInNativeMenus;", source)
        self.assertIn("0x804993C0u", source)
        self.assertIn("0x804A1288u", source)
        self.assertIn("0x804A128Cu", source)
        self.assertNotIn("197u", source)
        self.assertNotIn("OSGetTime", source)
        event = source.split('extern "C" void diagnosticTimerEventStart', 1)[1]
        self.assertLess(event.index("0x80065158u"), event.index("LmTimerClockEventAction"))
        self.assertIn("word(0x803C7CACu) != 0u", event)
        self.assertIn("if (action)", event)
        update=source.split('extern "C" u32 diagnosticTimerGameUpdate()',1)[1].split('extern "C" void diagnosticTimerEventStart',1)[0]
        self.assertLess(update.index("beforeHigh="),update.index("diagnosticTimerRetailGameUpdate()"))
        self.assertLess(update.index("diagnosticTimerRetailGameUpdate()"),update.index("LmTimerClockSupplement"))
        self.assertLess(update.index("LmTimerClockSupplement"),update.index("0x80061A48u"))
        self.assertIn("return result;",update)
        self.assertNotIn("volatile u32 *",update)
        self.assertNotIn("804A0BBA",source)

    def test_original_clock_unhooked_and_whole_update_uses_entry_wrapper(self):
        from test_lm_diag import lm_diag
        self.assertNotIn(0x8000B9B8, [entry["lmj"] for entry in lm_diag.patches])
        hook=[entry for entry in lm_diag.patches if entry["lmj"]==0x8000B918]
        self.assertEqual(len(hook),1)
        self.assertEqual(hook[0]["sym"],"diagnosticTimerGameUpdate")
        self.assertEqual(hook[0]["type"].name,"B")
        # Visibility and editor settings have no write access to clock state.
        display = (ROOT / "lm_diag/src/lm_timer.cpp").read_text()
        self.assertNotIn("0x804993C0", display)
        self.assertNotIn("0x804A1288", display)

    def test_native_menu_option_is_fifth_row_and_persistent_value46(self):
        source=(ROOT/"lm_diag/src/lm_timer.cpp").read_text()
        self.assertIn('"Edit streak...", "Run in native menus"',source)
        self.assertIn('"D-pad down menu always runs."',source)
        self.assertIn("v[46]=LMTimerClock::runInNativeMenus();",source)
        self.assertIn("present(lo,hi,46) && v[46]<=1u",source)
        self.assertIn("sSelected = (sSelected + 4u) % 5u",source)
        self.assertIn("sSelected = (sSelected + 1u) % 5u",source)
        self.assertIn("else if (sSelected == 4)",source)
        self.assertIn("i < 5; ++i",source)
        self.assertLess(61+4*18+7,148)
        self.assertLess(16+len("Run in native menus")*6,254)

    def test_compiled_ppc_trampoline_replays_only_original_entry_then_rejoins(self):
        compiler=ROOT/"toolchain/clang.exe"
        if not compiler.exists():self.skipTest("Bundled PPC compiler unavailable")
        from elftools.elf.elffile import ELFFile
        with tempfile.TemporaryDirectory(prefix="lm-clock-ppc-") as directory:
            obj=Path(directory)/"clock.o"
            result=subprocess.run([str(compiler),"--target=powerpc-unknown-eabi","-m32","-std=c++17","-O2",
                "-fno-exceptions","-fno-rtti","-DSUSAMUNE_VERSION_LMJ","-I",str(ROOT/"include"),
                "-I",str(ROOT/"lm_diag/include"),"-c",str(ROOT/"lm_diag/src/lm_timer_clock.cpp"),"-o",str(obj)],
                capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            with obj.open("rb") as stream:
                elf=ELFFile(stream)
                symbol=elf.get_section_by_name(".symtab").get_symbol_by_name("diagnosticTimerRetailGameUpdate")[0]
                section=elf.get_section(symbol["st_shndx"])
                code=section.data()[symbol["st_value"]:symbol["st_value"]+symbol["st_size"]]
                # mflr r0; lis r12,8000; ori r12,r12,B91C; mtctr r12; bctr.
                self.assertEqual(code,bytes.fromhex("7C0802A6 3D808000 618CB91C 7D8903A6 4E800420"))

    def test_native_timer_already_in_snapshot_ranges(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertIn("kGameSdata1Start = 0x80498B20u;", source)
        self.assertIn("kGameSdata1End = 0x804A03A8u;", source)
        self.assertIn("kGameSbss1Start = 0x804A0CB0u;", source)
        self.assertIn("kGameSbss1End = 0x804A1D10u;", source)
        self.assertTrue(0x80498B20 <= 0x804993C0 < 0x804A03A8)
        self.assertTrue(0x804A0CB0 <= 0x804A1288 < 0x804A129C <= 0x804A1D10)


class NativeTimerClockTests(AuthenticatedRetailTests):
    def test_native_ui_ownership_is_not_the_doublebuffer_or_transition_flag(self):
        self.assertEqual(0x804A0AE0+0x164,0x804A0C44)
        self.words({0x8000C678:0x806D0118,0x8000C67C:0x8003001C,
                    0x8000C680:0x540004E7,0x8000C688:0x3BE00001,0x8000C690:0x93ED0164,
                    0x8000B95C:0x806D0118,0x8000B960:0x8003001C,0x8000B964:0x54000529,
                    0x8000C2D8:0x3BA00002,0x8000C30C:0x93AD0164,
                    0x8000C804:0x8003001C,0x8000C808:0x540006F7,
                    0x8000C864:0x38600003,0x8000C86C:0x906D0164,
                    0x8000C660:0x808D0164,0x8000C6C0:0x2C040001,
                    0x8000C378:0x800D0164,0x8000C38C:0x2C000002,
                    0x8000C8C4:0x800D0164,0x8000C8D8:0x2C000003})
        self.call(0x8000B974,0x8000C238)
        self.call(0x8000C3CC,0x800297B8)
        self.call(0x8000C6DC,0x80038D9C)
        self.call(0x8000C8A0,0x800525D0)

    def test_retail_update_skips_clock_only_in_native_ui_paths(self):
        self.words({0x8000B918:0x7C0802A6,0x8000B91C:0x90010004,
                    0x8000B98C:0x800D0164,0x8000B990:0x2C000004,
                    0x8000B994:0x41820024,0x8000B99C:0x2C000000,
                    0x8000B9A0:0x41820018,0x8000B9A8:0x2C0000FF,
                    0x8000B9AC:0x4182000C,0x8000B9B0:0x38600002,
                    0x8000B9B4:0x480000A0,0x8000B940:0x40820018,
                    0x8000B950:0x38600001,0x8000B954:0x48000100,
                    0x8000BA54:0x8001000C,0x8000BA5C:0x7C0803A6,0x8000BA60:0x4E800020})
        self.call(0x8000B938,0x8000C650)
        self.call(0x8000B978,0x8000C368)
        self.call(0x8000B97C,0x8000C8B8)
        self.call(0x8000B9B8,0x80061A48)

    def test_optional_overlays_gate_native_single_buffer_without_changing_mode(self):
        # The second XFB becomes the native screenshot/menu's background texture.
        self.words({0x80007374: 0x981E0002, 0x80007800: 0x88030002,
                    0x80007804: 0x28000001, 0x80007808: 0x40820054,
                    0x800080EC: 0x38600000, 0x80007FD4: 0x38600001,
                    0x80007D50: 0x38AD00D8, 0x80007D54: 0x9C650002})
        self.call(0x80007828, 0x801F045C)
        self.call(0x8000785C, 0x80007714)
        self.call(0x800080F0, 0x80007D48)
        self.call(0x80007FD8, 0x80007D48)
        self.assertEqual(0x804A0AE0 + 0xD8 + 2, 0x804A0BBA)

    def test_event_loader_publishes_id_only_with_matching_script(self):
        self.call(0x8002B5B4, 0x80065158)
        self.call(0x80065200, 0x8002BB7C)
        self.words({0x80065178: 0x7C7A1B78, 0x8006517C: 0x93DD100C,
                    0x800651F0: 0x7C7F1B79, 0x800651F4: 0x4082000C,
                    0x80065204: 0xB07D1000, 0x8006520C: 0x93FC0000,
                    0x8002BB7C: 0x806D0410, 0x8002BB80: 0xA8630038})

    def test_tick_count_and_active_flag(self):
        self.words({0x80061A60: 0x800D88E0, 0x80061A68: 0x41820024,
                    0x80061A6C: 0x80AD07AC, 0x80061A70: 0x38000001,
                    0x80061A74: 0x808D07A8, 0x80061A80: 0x900D07AC,
                    0x80061A88: 0x900D07A8, 0x800646A4: 0x38000000,
                    0x800646A8: 0x900D88E0, 0x800646B0: 0x38000001,
                    0x800646B4: 0x900D88E0})
        self.assertEqual(0x804A0AE0 - 0x7720, 0x804993C0)
        self.assertEqual(0x804A0AE0 + 0x7A8, 0x804A1288)
        self.assertEqual(0x804A0AE0 + 0x7AC, 0x804A128C)
        self.call(0x8000B9B8, 0x80061A48)

    def test_native_reset_and_world_clock_units(self):
        self.words({0x80061A2C: 0x38000000, 0x80061A30: 0x900D07AC,
                    0x80061A34: 0x900D07A8, 0x80061A38: 0x900D07B0,
                    0x80061A3C: 0x900D07B4, 0x80061A40: 0x900D07B8,
                    0x80061A90: 0x3880000A, 0x80061AB8: 0x38C0001E,
                    0x80061AF8: 0x38C0003C, 0x80061B10: 0x38C0003C,
                    0x80061B20: 0x2C000006})
        self.call(0x8000B880, 0x80061A2C)
        self.call(0x8000C084, 0x80061A2C)


if __name__ == "__main__":
    unittest.main()
