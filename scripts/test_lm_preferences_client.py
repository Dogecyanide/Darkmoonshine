"""Exercise actual PPC preference client publication and acknowledgement rules."""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name=="nt" and (ROOT/"toolchain/clang.exe").exists(),"native compiler unavailable")
class PreferencesClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix="lm-prefs-client-")
        cls.libs=[]
        for emulator in (0,1):
            obj=Path(cls.temp.name)/f"prefs{emulator}.obj"
            dll=Path(cls.temp.name)/f"prefs{emulator}.dll"
            result=subprocess.run([str(ROOT/"toolchain/clang.exe"),"--target=x86_64-pc-windows-msvc","-O2",
                "-fno-stack-protector","-ffreestanding","-fno-exceptions","-fno-rtti",f"-DIS_EMULATOR={emulator}",
                "-c",str(ROOT/"scripts/lm_preferences_client_harness.cpp"),"-I",str(ROOT/"include"),
                "-I",str(ROOT/"lm_diag/include"),"-o",str(obj)],capture_output=True,text=True)
            if result.returncode: raise RuntimeError(result.stderr)
            result=subprocess.run([str(ROOT/"toolchain/lld-link.exe"),"/dll","/noentry","/nodefaultlib",f"/out:{dll}",str(obj)],capture_output=True,text=True)
            if result.returncode: raise RuntimeError(result.stderr)
            lib=ctypes.CDLL(str(dll))
            for name,count in (("boot_config",4),("set_live",2),("get_live",1),("set_open",1),
                               ("mailbox_word",1),("tamper",2),("ack",2),("event_word",2)):
                getattr(lib,name).argtypes=[ctypes.c_uint]*count
                getattr(lib,name).restype=ctypes.c_uint
            lib.status_text.restype=ctypes.c_char_p
            cls.libs.append(lib)
        cls.lib=cls.libs[0]

    @classmethod
    def tearDownClass(cls):
        for lib in cls.libs:ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(lib._handle))
        cls.libs=[]
        cls.temp.cleanup()

    def setUp(self): self.lib.reset_client()
    def boot(self,status=0,low=0xffffffff,high=0xffff,seq=0):
        self.lib.boot_config(status,low,high,seq)
        self.lib.step()
    def text(self):return self.lib.status_text().decode()
    def events(self):return [tuple(self.lib.event_word(i,j) for j in range(3)) for i in range(self.lib.event_count())]
    def words(self):return tuple(self.lib.mailbox_word(i) for i in range(64))
    def modified(self,index=0,value=987):
        self.lib.set_live(index,value)
        self.lib.save_config()

    def test_valid_boot_applies_only_present_fields_once_without_writing(self):
        self.boot(low=1,high=1<<13)
        self.assertEqual(self.lib.get_live(0),1000)
        self.assertEqual(self.lib.get_live(1),101)
        self.assertEqual(self.lib.get_live(45),1045)
        self.assertEqual(self.lib.applied(),3)
        self.lib.step()
        self.assertEqual(self.lib.applied(),3)
        self.assertFalse(any(e[0]==2 for e in self.events()))
        self.assertIn("LOADED",self.text())

    def test_missing_file_publishes_defaults_but_never_claims_saved_before_ack(self):
        self.boot(status=1,low=0,high=0)
        self.assertEqual(self.lib.mailbox_word(2),1)
        self.assertEqual(self.lib.mailbox_word(16),100)
        self.assertEqual(self.lib.mailbox_word(61),145)
        self.assertEqual(self.lib.mailbox_word(62),146)
        self.assertEqual(self.lib.mailbox_word(63),147)
        self.assertIn("SAVING",self.text())
        self.lib.step()
        self.assertIn("SAVING",self.text())
        self.lib.ack(1,0);self.lib.step()
        self.assertIn("SAVED",self.text())

    def test_publication_flushes_payload_before_control_and_never_arm_line(self):
        self.boot();self.lib.clear_events();self.modified()
        events=self.events()
        writes=[e for e in events if e[0]==2]
        self.assertEqual(writes,[(2,64,192),(2,0,32)])
        payload=events.index(writes[0]);control=events.index(writes[1])
        self.assertIn((3,0,0),events[payload+1:control])
        self.assertEqual(events[-1],(3,0,0))

    def test_pending_changes_coalesce_without_touching_published_payload(self):
        self.boot();self.modified(value=777)
        before=self.words();self.lib.clear_events()
        self.modified(value=888);self.modified(value=999)
        self.assertEqual(self.words(),before)
        self.assertFalse(any(e[0]==2 for e in self.events()))
        self.lib.ack(1,0);self.lib.step()
        self.assertEqual(self.lib.mailbox_word(2),2)
        self.assertEqual(self.lib.mailbox_word(16),999)
        self.lib.ack(2,0);self.lib.step()
        self.assertIn("SAVED",self.text())

    def test_reverting_while_pending_is_saved_after_current_ack(self):
        self.boot();self.modified(value=777);self.modified(value=1000)
        self.lib.ack(1,0);self.lib.step()
        self.assertEqual(self.lib.mailbox_word(2),2)
        self.assertEqual(self.lib.mailbox_word(16),1000)

    def test_same_value_or_discard_does_not_write_again(self):
        self.boot();self.lib.clear_events();self.lib.save_config()
        self.assertFalse(any(e[0]==2 for e in self.events()))
        self.lib.set_open(1);self.lib.step();self.lib.set_live(2,1)
        self.lib.set_live(2,1002);self.lib.set_open(0);self.lib.step()
        self.assertEqual(self.lib.mailbox_word(2),0)

    def test_menu_close_persists_actual_change(self):
        self.boot();self.lib.set_open(1);self.lib.step()
        self.lib.set_live(45,321);self.lib.set_open(0);self.lib.step()
        self.assertEqual(self.lib.mailbox_word(61),321)
        self.assertEqual(self.lib.mailbox_word(2),1)

    def test_new_tail_fields_are_loaded_and_saved_independently(self):
        for index in (46,47):
            with self.subTest(index=index):
                self.lib.reset_client()
                self.boot(low=0,high=1<<(index-32))
                self.assertEqual(self.lib.get_live(index),1000+index)
                other=47 if index==46 else 46
                self.assertEqual(self.lib.get_live(other),100+other)
                self.modified(index=index,value=1)
                self.assertEqual(self.lib.mailbox_word(16+index),1)
                self.assertEqual(self.lib.mailbox_word(16+other),100+other)
                self.assertEqual(self.lib.mailbox_word(5),0xffff)
                self.assertEqual(self.lib.mailbox_word(1),2)
                self.assertEqual(self.lib.mailbox_word(2),1)
                self.lib.ack(1,0);self.lib.step()
                self.assertIn("SAVED",self.text())

    def test_migrated_absent_tail_retains_current_defaults_without_auto_write(self):
        self.lib.set_live(46,0);self.lib.set_live(47,0)
        self.boot(high=0x3fff)
        self.assertEqual(self.lib.get_live(45),1045)
        self.assertEqual(self.lib.get_live(46),0)
        self.assertEqual(self.lib.get_live(47),0)
        self.assertEqual(self.lib.mailbox_word(2),0)

    def test_early_new_tail_edit_survives_delayed_boot(self):
        self.lib.set_open(1);self.lib.step()
        self.lib.set_live(46,1);self.lib.set_live(47,1)
        self.lib.boot_config(0,0xffffffff,0xffff,0)
        self.lib.set_open(0);self.lib.step()
        self.assertEqual(self.lib.get_live(46),1)
        self.assertEqual(self.lib.get_live(47),1)
        self.assertEqual(self.lib.mailbox_word(62),1)
        self.assertEqual(self.lib.mailbox_word(63),1)

    def test_invalid_file_is_not_auto_overwritten_but_edit_is_explicit(self):
        self.boot(status=2,low=0,high=0)
        self.assertEqual(self.lib.applied(),0)
        self.assertEqual(self.lib.mailbox_word(2),0)
        self.lib.save_config();self.assertEqual(self.lib.mailbox_word(2),0)
        self.modified();self.assertEqual(self.lib.mailbox_word(2),1)

    def test_corrupt_boot_crc_is_never_applied(self):
        self.lib.boot_config(0,0xffffffff,0x3fff,0)
        self.lib.tamper(3,0);self.lib.step()
        self.assertEqual(self.lib.applied(),0)
        self.assertEqual(self.lib.get_live(0),100)
        self.assertIn("CHECK FAILED",self.text())

    def test_wrong_ack_never_completes_or_reuses_buffer(self):
        self.boot();self.modified();before=self.words()
        self.lib.ack(0,0);self.lib.step()
        self.assertIn("SAVING",self.text())
        self.lib.ack(2,0);self.lib.step()
        self.assertIn("SAVING",self.text())
        self.assertEqual(self.lib.mailbox_word(2),before[2])

    def test_failed_write_requires_request_and_retries_with_new_sequence(self):
        self.boot();self.modified();self.lib.ack(1,3);self.lib.step()
        self.assertIn("FAILED",self.text())
        self.lib.step();self.assertEqual(self.lib.mailbox_word(2),1)
        self.lib.save_config();self.assertEqual(self.lib.mailbox_word(2),2)
        self.lib.ack(2,0);self.lib.step();self.assertIn("SAVED",self.text())

    def test_corrupted_inflight_payload_is_not_a_successful_save(self):
        self.boot();self.modified();self.lib.tamper(16,4321)
        self.lib.ack(1,0);self.lib.step()
        self.assertIn("CHECK FAILED",self.text())

    def test_soft_reset_waits_for_old_worker_ack(self):
        self.lib.boot_config(0,0xffffffff,0x3fff,7);self.lib.ack(6,0)
        self.lib.step();self.assertEqual(self.lib.applied(),0)
        self.lib.save_config();self.assertEqual(self.lib.mailbox_word(2),7)
        self.lib.ack(7,0);self.lib.step()
        self.assertEqual(self.lib.get_live(0),1000)
        self.assertEqual(self.lib.mailbox_word(2),7)

    def test_sequence_wrap_skips_zero(self):
        self.boot(seq=0xffffffff);self.modified()
        self.assertEqual(self.lib.mailbox_word(2),1)

    def test_dolphin_never_accesses_mailbox_or_claims_persistence(self):
        lib=self.libs[1];lib.reset_client();lib.step();lib.save_config()
        self.assertEqual(lib.event_count(),0)
        self.assertEqual(lib.applied(),0)
        self.assertIn(b"WII ONLY",lib.status_text())

    def test_boot_waits_until_menu_closed_and_preserves_early_user_edits(self):
        self.lib.set_open(1);self.lib.step()
        self.lib.set_live(3,88);self.lib.boot_config(0,0xffffffff,0x3fff,0)
        self.lib.step();self.assertEqual(self.lib.applied(),0)
        self.lib.set_open(0);self.lib.step()
        self.assertEqual(self.lib.get_live(3),88)
        self.assertEqual(self.lib.get_live(4),1004)
        self.assertEqual(self.lib.mailbox_word(19),88)


if __name__=="__main__":unittest.main()
