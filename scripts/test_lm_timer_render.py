"""Native-memory tests of the retail-art affine XFB timer renderer."""
import ctypes
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
SIZE=640*480*2
BASE=bytes((40,128,40,128))*(SIZE//4)


class Style(ctypes.Structure):
    _fields_=[("x",ctypes.c_int),("y",ctypes.c_int),("scale",ctypes.c_int),
              ("opacity",ctypes.c_int),("brightness",ctypes.c_int),
              ("colours",ctypes.c_uint*9),("showLabel",ctypes.c_int),("showStreak",ctypes.c_int)]


class Streak(ctypes.Structure):
    _fields_=[("x",ctypes.c_int),("y",ctypes.c_int),("scale",ctypes.c_int),
              ("opacity",ctypes.c_int),("brightness",ctypes.c_int),("rgb",ctypes.c_uint)]


@unittest.skipUnless(os.name=="nt" and (ROOT/"toolchain/clang.exe").exists(),"native compiler unavailable")
class TimerRendererTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix="lm-timer-test-")
        obj=Path(cls.temp.name)/"timer.obj"
        dll=Path(cls.temp.name)/"timer.dll"
        subprocess.run([str(ROOT/"toolchain/clang.exe"),"--target=x86_64-pc-windows-msvc","-O2",
            "-fno-stack-protector","-fno-exceptions","-fno-rtti","-c",str(ROOT/"scripts/lm_timer_test_bridge.cpp"),
            "-I",str(ROOT/"include"),"-I",str(ROOT/"lm_diag/include"),"-o",str(obj)],check=True,capture_output=True,text=True)
        subprocess.run([str(ROOT/"toolchain/lld-link.exe"),"/dll","/noentry","/nodefaultlib",f"/out:{dll}",str(obj)],
                       check=True,capture_output=True,text=True)
        cls.lib=ctypes.CDLL(str(dll))
        cls.lib.draw_timer.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.POINTER(Style),ctypes.POINTER(Streak)]
        cls.lib.timer_texel.argtypes=[ctypes.c_uint,ctypes.c_int,ctypes.c_int]
        cls.lib.timer_texel.restype=ctypes.c_uint
        cls.lib.timer_bounds.argtypes=[ctypes.POINTER(Style),ctypes.c_int,ctypes.POINTER(ctypes.c_int)]
        cls.lib.draw_timer_cached.argtypes=cls.lib.draw_timer.argtypes
        cls.lib.timer_lerp.argtypes=[ctypes.c_uint,ctypes.c_uint,ctypes.c_int]
        cls.lib.timer_lerp.restype=ctypes.c_uint
        cls.lib.draw_timer_decorated.argtypes=[ctypes.c_void_p,ctypes.POINTER(Style),ctypes.POINTER(Streak),
            ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.POINTER(ctypes.c_int)]
        refobj=Path(cls.temp.name)/"reference.obj"
        refdll=Path(cls.temp.name)/"reference.dll"
        subprocess.run([str(ROOT/"toolchain/clang.exe"),"--target=x86_64-pc-windows-msvc","-O2",
            "-fno-stack-protector","-fno-exceptions","-fno-rtti","-c",str(ROOT/"scripts/lm_timer_reference_bridge.cpp"),
            "-I",str(ROOT/"include"),"-I",str(ROOT/"lm_diag/include"),"-o",str(refobj)],check=True,capture_output=True,text=True)
        subprocess.run([str(ROOT/"toolchain/lld-link.exe"),"/dll","/noentry","/nodefaultlib",f"/out:{refdll}",str(refobj)],
                       check=True,capture_output=True,text=True)
        cls.reference=ctypes.CDLL(str(refdll))
        cls.reference.draw_timer.argtypes=cls.lib.draw_timer.argtypes

    @classmethod
    def tearDownClass(cls):
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(cls.lib._handle))
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(cls.reference._handle))
        del cls.lib
        del cls.reference
        cls.temp.cleanup()

    def defaults(self):
        return Style(13,419,100,255,100,(ctypes.c_uint*9)(*[0xffffff]*9),1,1),Streak(0,0,100,255,100,0xff28ff)

    def render(self,cs=75456,style=None,streak=None,draw=None,base=BASE):
        default,back=self.defaults()
        style=style or default
        streak=streak or back
        buf=ctypes.create_string_buffer(b"\xAD"*32+base+b"\xAC"*32,SIZE+64)
        (draw or self.lib.draw_timer)(ctypes.addressof(buf)+32,cs,ctypes.byref(style),ctypes.byref(streak))
        self.assertEqual(buf.raw[:32],b"\xAD"*32)
        self.assertEqual(buf.raw[-32:],b"\xAC"*32)
        return buf.raw[32:-32]

    def test_packed_bilinear_rounding_is_exact(self):
        rng=random.Random(0xB111)
        for i in range(1200):
            a,b,f=rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256)
            expected=sum(((((a>>n)&255)*(256-f)+((b>>n)&255)*f+128)>>8)<<n for n in (0,8,16,24))
            self.assertEqual(self.lib.timer_lerp(a,b,f),expected)

    def test_cold_and_warm_cache_byte_match_original_renderer(self):
        rng=random.Random(0xCACE)
        cases=[(13,419,100,100),(320,95,100,100),(-25,35,75,150),
               (610,470,200,200),(3,190,50,25),(220,140,137,83)]
        for x,y,scale,extra in cases:
            style,streak=self.defaults()
            style.x,style.y,style.scale=x,y,scale
            streak.scale=extra
            self.lib.timer_reset_cache()
            for cs in (0,3,9,10,11,99,100,5999,6000,75456,599999):
                expected=self.render(cs,style,streak,self.reference.draw_timer)
                self.assertEqual(self.render(cs,style,streak),expected)
                self.assertEqual(self.render(cs,style,streak,self.lib.draw_timer_cached),expected)
                self.assertEqual(self.render(cs,style,streak,self.lib.draw_timer_cached),expected)
            for _ in range(4):
                for i in range(9): style.colours[i]=rng.getrandbits(24)
                style.opacity=rng.randrange(1,256);style.brightness=rng.randrange(25,201)
                streak.opacity=rng.randrange(1,256);streak.brightness=rng.randrange(25,201)
                streak.rgb=rng.getrandbits(24)
                expected=self.render(38234,style,streak,self.reference.draw_timer)
                self.assertEqual(self.render(38234,style,streak,self.lib.draw_timer_cached),expected)
                self.assertEqual(self.render(38234,style,streak,self.lib.draw_timer_cached),expected)

    def test_cache_blends_fresh_frame_not_saved_background(self):
        style,streak=self.defaults()
        self.lib.timer_reset_cache()
        self.render(style=style,streak=streak,draw=self.lib.draw_timer_cached)
        rng=random.Random(311)
        changing=bytes(rng.randrange(256) for _ in range(SIZE))
        cached=self.render(style=style,streak=streak,draw=self.lib.draw_timer_cached,base=changing)
        self.assertEqual(cached,self.render(style=style,streak=streak,draw=self.reference.draw_timer,base=changing))
        self.assertNotEqual(cached,self.render(style=style,streak=streak,draw=self.lib.draw_timer_cached))

    def test_cache_all_native_panes_fit_and_eliminate_resampling(self):
        style,streak=self.defaults()
        self.lib.timer_reset_cache()
        maximum=[0]*10
        for x in (13,14,320,321):
            style.x=x
            for n in range(10):
                cs=n*60000+(n*6000)%60000+(n%6)*1000+n*100+n*10+n
                self.render(cs,style,streak,self.lib.draw_timer_cached)
                self.assertEqual([self.lib.timer_cache_status(i) for i in range(10)],[1]*10)
                maximum=[max(maximum[i],self.lib.timer_cache_used(i)) for i in range(10)]
        self.assertEqual(maximum,[4024,4024,1812,3944,4024,1812,4024,4024,6268,17220])
        self.lib.timer_reset_work()
        self.render(cs,style,streak,self.lib.draw_timer_cached)
        self.assertEqual([self.lib.timer_work(i) for i in range(3)],[0,0,0])
        self.assertEqual(self.lib.timer_work(4),10)
        self.assertEqual(self.lib.timer_work(5),0)
        self.assertLessEqual(self.lib.timer_cache_size(),65*1024)

    def test_changes_to_all_cache_keys_and_oversize_fallback(self):
        style,streak=self.defaults()
        self.lib.timer_reset_cache()
        mutations=[lambda:setattr(style,"x",319),lambda:setattr(style,"y",105),
            lambda:setattr(style,"scale",200),lambda:setattr(streak,"scale",200),
            lambda:setattr(streak,"x",-100),lambda:setattr(streak,"y",70),
            lambda:setattr(style,"opacity",122),lambda:setattr(style,"brightness",156),
            lambda:setattr(streak,"rgb",0x4040FF),lambda:setattr(streak,"opacity",43),
            lambda:setattr(streak,"brightness",25),lambda:setattr(style,"showLabel",0),
            lambda:setattr(style,"showStreak",0),lambda:setattr(style,"scale",25)]
        for change in mutations:
            change()
            expected=self.render(97531,style,streak,self.reference.draw_timer)
            self.assertEqual(self.render(97531,style,streak,self.lib.draw_timer_cached),expected)
            self.assertEqual(self.render(97531,style,streak,self.lib.draw_timer_cached),expected)

    def test_running_timer_work_reduction_and_report_host_cost(self):
        style,streak=self.defaults();style.x,style.y=320,95
        frame=ctypes.create_string_buffer(BASE,SIZE)
        args=(frame,0,ctypes.byref(style),ctypes.byref(streak))
        counts=[];times=[]
        for draw in (self.reference.draw_timer,self.lib.draw_timer,self.lib.draw_timer_cached):
            self.lib.timer_reset_cache();self.lib.timer_reset_work()
            start=time.perf_counter()
            for i in range(180):draw(args[0],75456+i*10//3,args[2],args[3])
            times.append((time.perf_counter()-start)*1000/180)
            counts.append([self.lib.timer_work(i) for i in range(7)])
        self.assertLess(counts[2][0],counts[1][0]//4)
        self.assertLess(counts[2][2],counts[1][2]//4)
        print("Timer host ms/frame original/direct/cached:",*[round(t,3) for t in times])
        print("Timer 180-frame direct/cached counters (texels,samples,colours,blends,hits,builds,bytes):",counts[1:])
        start=time.perf_counter()
        for i in range(100):
            self.lib.timer_reset_cache()
            self.lib.draw_timer_cached(args[0],75456+i,args[2],args[3])
        print("Timer cold-cache host ms/frame:",round((time.perf_counter()-start)*10,3))

    def test_dirty_row_flush_includes_backgrounds_arrow_and_clipped_art(self):
        rng=random.Random(0xF105)
        style,streak=self.defaults()
        style.x,style.y=320,95
        flush=(ctypes.c_int*2)()
        for case in range(65):
            if case:
                style.x=rng.randrange(-450,700);style.y=rng.randrange(-450,700)
                style.scale=rng.randrange(25,201);streak.scale=rng.randrange(25,201)
                streak.x=rng.randrange(-150,150);streak.y=rng.randrange(-150,150)
            buf=ctypes.create_string_buffer(b"\xA5"*32+BASE+b"\x5A"*32,SIZE+64)
            self.lib.draw_timer_decorated(ctypes.addressof(buf)+32,ctypes.byref(style),ctypes.byref(streak),
                -1 if not case else rng.randrange(-1,17),-1 if not case else rng.randrange(-1,17),
                -1 if not case else rng.randrange(-1,9),flush)
            self.assertEqual(buf.raw[:32],b"\xA5"*32);self.assertEqual(buf.raw[-32:],b"\x5A"*32)
            top,height=flush;self.assertTrue(0<=top<=240 and 0<=height<=240-top)
            data=buf.raw[32:-32]
            self.assertEqual(data[:top*2*1280],BASE[:top*2*1280])
            self.assertEqual(data[(top+height)*2*1280:],BASE[(top+height)*2*1280:])
            if not case:
                self.assertLess(height*2,120)
                print("Timer default flush bytes:",height*2*1280,"of",SIZE)

    def test_timer_only_flushes_dirty_rows_and_keeps_presenter_draw(self):
        source=(ROOT/"lm_diag/src/lm_timer.cpp").read_text()
        self.assertIn("LmTimerCache sRasterCache",source)
        self.assertIn("&style, &streak, &sRasterCache",source)
        self.assertIn("LMDraw::flush(xfb, 640, 480, top, height)",source)
        self.assertNotIn("LMDraw::flush(xfb, 640, 480, 0, 240)",source)
        self.assertIn("LmTimerIncludeRows(&dirty,y,y+6)",source)
        self.assertIn("bounds[1]-sStyle.padding",source)
        self.assertIn("bounds[1]-sStreakStyle.padding",source)
        self.assertIn("void draw(void *, void *xfb) { if (sVisible && LMTimerClock::available()) render(xfb, false); }",source)

    def test_default_native_bounds_and_slant(self):
        style,_=self.defaults()
        bounds=(ctypes.c_int*4)()
        self.assertTrue(self.lib.timer_bounds(ctypes.byref(style),0,bounds))
        self.assertEqual(tuple(bounds),(16,397,62,443))
        self.assertTrue(self.lib.timer_bounds(ctypes.byref(style),7,bounds))
        self.assertEqual(tuple(bounds),(185,358,231,404))
        self.assertLess(bounds[1],397)
        data=self.render()
        self.assertNotEqual(data,BASE)
        self.assertEqual(data[:320*1280],BASE[:320*1280])
        self.assertEqual(data[444*1280:],BASE[444*1280:])

    def test_timer_clamps_to_99_minutes_not_wrap(self):
        self.assertEqual(self.render(599999),self.render(0xffffffff))
        self.assertNotEqual(self.render(0),self.render(1))
        self.assertNotEqual(self.render(5999),self.render(6000))

    def test_opacity_zero_and_invalid_origin_do_not_write(self):
        style,streak=self.defaults()
        for x,alpha in ((13,0),(0x7fffffff,255),(-0x80000000,255)):
            style.x=x;style.opacity=alpha
            self.assertEqual(self.render(style=style,streak=streak),BASE)

    def test_separate_label_and_streak_controls(self):
        style,streak=self.defaults()
        full=self.render(style=style,streak=streak)
        style.showLabel=0
        no_label=self.render(style=style,streak=streak)
        self.assertNotEqual(full,no_label)
        style.showStreak=0
        self.assertNotEqual(no_label,self.render(style=style,streak=streak))

    def test_each_character_tint_only_changes_its_native_pane(self):
        style,streak=self.defaults()
        base=self.render(style=style,streak=streak)
        for i in range(9):
            style.colours[i]=0xff0000
            new=self.render(style=style,streak=streak)
            self.assertNotEqual(base,new)
            bounds=(ctypes.c_int*4)()
            self.lib.timer_bounds(ctypes.byref(style),i,bounds)
            x0,y0,x1,y1=bounds
            for y in range(480):
                start=y*1280
                if not y0<=y<y1:self.assertEqual(base[start:start+1280],new[start:start+1280])
                else:
                    self.assertEqual(base[start:start+(max(0,x0)&~1)*2],new[start:start+(max(0,x0)&~1)*2])
                    at=start+min(640,(x1+1)&~1)*2
                    self.assertEqual(base[at:start+1280],new[at:start+1280])
            style.colours[i]=0xffffff

    def test_offscreen_scaled_and_streak_transforms_are_bounded(self):
        rng=random.Random(0x54494d45)
        for _ in range(25):
            style,streak=self.defaults()
            style.x=rng.randrange(-600,900);style.y=rng.randrange(-600,900)
            style.scale=rng.randrange(-10,400);streak.scale=rng.randrange(-10,400)
            streak.x=rng.randrange(-1000,1000);streak.y=rng.randrange(-1000,1000)
            self.render(style=style,streak=streak)

    def test_preview_native_art(self):
        from PIL import Image
        data=self.render()
        pixels=bytearray(640*480*3)
        def clamp(v):return min(255,max(0,v))
        for i in range(SIZE//4):
            y0,cb,y1,cr=data[i*4:i*4+4]
            for side,y in enumerate((y0,y1)):
                c,d,e=y-16,cb-128,cr-128
                p=(i*2+side)*3
                pixels[p:p+3]=bytes((clamp((298*c+409*e+128)>>8),clamp((298*c-100*d-208*e+128)>>8),clamp((298*c+516*d+128)>>8)))
        image=Image.frombytes("RGB",(640,480),pixels)
        image.save(ROOT/"build-lm-diag/sunshine-timer-preview.png")
        image.crop((0,320,310,460)).resize((930,420)).save(ROOT/"build-lm-diag/sunshine-timer-preview-close.png")


if __name__=="__main__":unittest.main()
