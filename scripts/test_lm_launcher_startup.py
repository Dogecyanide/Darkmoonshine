"""Execute root-theme path/creation policy and audit lazy startup wiring."""
import ctypes
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
LOADER=ROOT/"launcher/loader"

def function(source,name):
    masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda match:" "*len(match.group()),source,flags=re.S)
    found=re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',masked)
    if found is None: raise AssertionError("Function not found: "+name)
    begin=masked.index("{",found.start())
    depth=1; end=begin+1
    while depth:
        depth+=(masked[end]=="{")-(masked[end]=="}");end+=1
    return source[begin:end]

class ThemeFilesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("gcc") or "C:/msys64/mingw64/bin/gcc.exe"
        if not Path(compiler).exists(): raise unittest.SkipTest("Host GCC unavailable")
        cls.temp=tempfile.TemporaryDirectory(prefix="lm-launcher-test-")
        output=Path(cls.temp.name)/"theme.dll"
        command=[compiler,"-shared","-O2","-std=c99","-Wall","-Werror",
            "-I",str(ROOT/"scripts/lm_launcher_test_stubs"),"-I",str(LOADER/"include"),
            str(ROOT/"scripts/lm_launcher_theme_harness.c"),"-o",str(output)]
        env=os.environ.copy();env["PATH"]=str(Path(compiler).parent)+os.pathsep+env.get("PATH","")
        subprocess.run(command,check=True,capture_output=True,text=True,env=env)
        cls.lib=ctypes.CDLL(str(output))
        cls.lib.setup.argtypes=[ctypes.c_int]*5
        cls.lib.SusamuneThemeEnsureDirectory.argtypes=[ctypes.c_char_p]
        cls.lib.last_path.restype=ctypes.c_char_p
        cls.lib.find_file.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_uint]
    @classmethod
    def tearDownClass(cls):
        from _ctypes import FreeLibrary
        FreeLibrary(cls.lib._handle);cls.temp.cleanup()
    def setUp(self):self.lib.setup(4,0,0,0,16)
    def test_sd_and_usb_root_creation_only(self):
        for device in (b"sd",b"usb"):
            self.setUp()
            self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(device),0)
            self.assertEqual(self.lib.last_path(),device+b":/Darkmoonshine_Theme")
            self.assertEqual([self.lib.metric(i) for i in (0,1)],[1,1])
    def test_existing_folder_never_written(self):
        self.lib.setup(0,16,7,0,16)
        self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(b"sd"),0)
        self.assertEqual(self.lib.metric(1),0)
    def test_existing_file_not_replaced(self):
        self.lib.setup(0,0,0,0,0)
        self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(b"sd"),8)
        self.assertEqual(self.lib.metric(1),0)
    def test_missing_path_can_create_but_io_failure_cannot(self):
        for initial in (1,2,3,7):
            self.lib.setup(initial,0,0,0,16)
            self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(b"sd"),initial)
            self.assertEqual(self.lib.metric(1),0)
        self.lib.setup(5,0,7,0,16)
        self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(b"sd"),7)
        self.assertEqual(self.lib.metric(1),1)
    def test_creation_race_requires_directory_not_regular_file(self):
        for attribute,expected in ((16,0),(0,8)):
            self.lib.setup(4,0,8,0,attribute)
            self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(b"sd"),expected)
            self.assertEqual(self.lib.metric(0),2)
    def test_invalid_devices_never_touch_storage(self):
        for device in (None,b"",b"SD",b"sd:/other",b"../",b"usb:"):
            self.assertEqual(self.lib.SusamuneThemeEnsureDirectory(device),6)
        self.assertEqual(self.lib.metric(0),0);self.assertEqual(self.lib.metric(1),0)
    def test_assets_share_root_on_both_devices(self):
        for device in (b"sd",b"usb"):
            for leaf in (b"background.png",b"bgm.mp3"):
                self.lib.setup(0,0,0,0,0)
                self.assertEqual(self.lib.find_file(device,leaf,128),0)
                self.assertEqual(self.lib.last_path(),device+b":/Darkmoonshine_Theme/"+leaf)
                self.assertEqual(self.lib.metric(1),0)
    def test_unknown_traversal_and_truncated_paths_never_reach_fs(self):
        for leaf in (None,b"../background.png",b"config.ini",b"",b"/bgm.mp3"):
            self.assertEqual(self.lib.find_file(b"sd",leaf,128),6)
        for size in (0,1,5,20):
            self.assertEqual(self.lib.find_file(b"sd",b"background.png",size),6)
        self.assertEqual(self.lib.metric(0),0)

class StartupWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main=(LOADER/"source/main.c").read_text()
        cls.global_=(LOADER/"source/global.c").read_text()
        cls.menu=(LOADER/"source/SusamuneMenu.c").read_text()
    def test_theme_preload_single_attempt_own_device_and_closed(self):
        code=function(self.main,"PreloadLauncherTheme")
        self.assertIn("UseSD ? DEV_SD : DEV_USB",code)
        self.assertIn("dev == DEV_USB && isWiiVC",code)
        self.assertIn("MountDeviceWithTimeout(dev, 0)",code)
        self.assertLess(code.index("SusamuneThemeLoad"),code.index("UnmountDevice(dev)"))
        self.assertNotIn("Music",code);self.assertNotIn("EnsureDirectory",code)
    def test_theme_created_on_stable_mount_before_autoboot(self):
        main=function(self.main,"main")
        self.assertEqual(main.count("SusamuneThemeEnsureDirectory(GetRootDevice())"),1)
        self.assertLess(main.index("KernelLoaded = 1"),main.index("SusamuneThemeEnsureDirectory"))
        self.assertLess(main.index("SusamuneThemeEnsureDirectory"),main.index("if(gIni.autoboot)"))
    def test_unused_usb_not_mounted_for_menu_or_autoboot(self):
        main=function(self.main,"main")
        self.assertNotIn("MountDeviceOnce(DEV_SD)",main)
        self.assertNotIn("MountDeviceOnce(DEV_USB)",main)
        self.assertIn("EnsureDeviceMounted(dev)",function(self.menu,"ValidateSelection"))
        self.assertIn("EnsureDeviceMounted(pos - 1)",function(self.menu,"BrowseDevices"))
        helper=function(self.menu,"EnsureDeviceMounted")
        self.assertLess(helper.index("DeviceMounted(dev)"),helper.index("MountDevice(dev)"))
        self.assertIn("dev == DEV_USB && isWiiVC",helper)
    def test_audio_only_after_autoboot_choice_and_preserves_shutdown(self):
        main=function(self.main,"main")
        menu=main[main.index("if(!(ncfg->Config & NIN_CFG_AUTO_BOOT))"):]
        self.assertEqual(main.count("SusamuneMusicInit()"),1)
        self.assertIn("SusamuneMusicInit()",menu)
        self.assertLess(menu.index("SusamuneMusicInit()"),menu.index("SusamuneMusicLoad"))
        self.assertLess(menu.index("SusamuneMusicLoad"),menu.index("SusamuneMusicStart"))
        self.assertNotIn("SusamuneMusicInit",function(self.global_,"Initialise"))
        self.assertIn("SusamuneMusicShutdown()",main)
        self.assertIn("SusamuneMusicShutdown()",self.global_)
        self.assertNotIn("RevealBackground(false)",main)
    def test_theme_loader_is_readonly_and_both_assets_use_shared_helper(self):
        theme=(LOADER/"source/SusamuneTheme.c").read_text()
        music=(LOADER/"source/SusamuneMusic.c").read_text()
        for source in (theme,music):
            self.assertIn("SusamuneThemeFindFile",source)
            self.assertNotIn("/theme",source)
            self.assertNotIn("f_mkdir",source)
        helper=(LOADER/"source/SusamuneThemeFiles.c").read_text()
        for forbidden in ("f_unlink","f_rename","f_write","FA_CREATE_ALWAYS"):
            self.assertNotIn(forbidden,helper)
    def test_mount_existing_device_and_allocation_failure_are_safe(self):
        mount=function(self.global_,"MountDeviceWithTimeout")
        self.assertLess(mount.index("if (devices[pdrv])"),mount.index("disk_initialize"))
        self.assertLess(mount.index("if (devices[pdrv] == NULL)"),mount.index("f_mount"))
        self.assertIn("while (time(NULL) - timeout < timeoutSeconds)",mount)
    def test_launcher_keeps_lm_mod_path_kernel_transport_and_b_cancel(self):
        self.assertIn("SusamuneLoadMod(ncfg->GameID)",self.main)
        self.assertIn("SusamuneAutoBoot(GetRootDevice())",self.main)
        self.assertIn("cancel = FPAD_Cancel(1)",self.main)
        self.assertIn("NIN_CFG_CFG_ON_USB",self.menu)
        self.assertIn("SusamuneCheckGameID(gameID)",self.menu)

if __name__=="__main__":unittest.main()
