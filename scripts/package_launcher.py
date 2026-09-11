"""Package the built Nintendont loader into the HBC app zip:
moonshine_luigis_mansion/{boot.dol, icon.png, meta.xml, mod_<tag>.bin...}.

One app serves every supported disc revision. The mod is no longer compiled into
the launcher: each mod_<tag>.bin sits next to boot.dol and the loader reads
the one matching the disc it detected (see launcher/loader/source/SusamuneMod.c),
which is why they are packaged here rather than embedded.

meta.xml is rendered from launcher/meta.xml.j2 (jinja2) with the git short hash.

Usage: package_launcher.py --boot-dol boot.dol --out-zip out.zip \
                           [--source di|sd|usb] [--test-log TESTING.md] \
                           [--changelog CHANGELOG.md] \
                           [--mod-bins mod_jp.bin ...]
"""
import argparse
import re
import subprocess
import sys
import zipfile
from pathlib import Path

LAUNCHER_DIR = Path(__file__).resolve().parent.parent / "launcher"
META_TEMPLATE = LAUNCHER_DIR / "meta.xml.j2"
APP_NAME = "moonshine_luigis_mansion"
APP_ICON = LAUNCHER_DIR / "icon.png"
BRANDING_HEADER = LAUNCHER_DIR.parent / "include/susamune/lm_branding.h"
THEME_DIR = LAUNCHER_DIR / "Darkmoonshine_Theme"
# Runtime music remains user-supplied, even when present in a local checkout.
THEME_FILES = ("background.png",)


def lm_branding():
    return dict(re.findall(r'^#define LM_BRANDING_(\w+) "([^"\r\n]*)"$',
                          BRANDING_HEADER.read_text(), re.M))


def git_version():
    """The tag name if HEAD is exactly at a tag (CI release builds), else the
    short commit hash."""
    repo_dir = str(LAUNCHER_DIR.parent)
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--exact-match", "HEAD"],
            cwd=repo_dir, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        pass
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=repo_dir, text=True).strip()
    except Exception:
        return "unknown"


def render_meta(source, regions, version=None):
    import jinja2
    template = jinja2.Template(META_TEMPLATE.read_text())
    is_lm = "lmj" in regions or not regions
    branding = lm_branding() if is_lm else {}
    return template.render(version=version or branding.get("VERSION") or git_version(),
                           source=source, regions=regions, is_lm=is_lm,
                           branding=branding)


def main(argv):
    ap = argparse.ArgumentParser(description="Package the Nintendont launcher HBC app zip.")
    ap.add_argument("--boot-dol", required=True, help="Built loader boot.dol")
    ap.add_argument("--out-zip", required=True, help="Output HBC app zip")
    ap.add_argument("--source", default="di", choices=["di", "sd", "usb"])
    ap.add_argument("--version", help="meta.xml version override")
    ap.add_argument("--test-log", help="tester log to include as TESTING.md")
    ap.add_argument("--changelog", help="release notes to include as CHANGELOG.md")
    ap.add_argument("--mod-bins", nargs="*", default=[],
                    help="mod_<tag>.bin files to drop into the app dir")
    args = ap.parse_args(argv)

    mod_bins = [Path(p) for p in args.mod_bins]
    # "mod_jp.bin" -> "jp", for the meta.xml blurb.
    regions = sorted(p.stem.split("_", 1)[1] for p in mod_bins)
    test_log = Path(args.test_log) if args.test_log else (
        LAUNCHER_DIR.parent / "doc/lm-testing-current.md" if "lmj" in regions else None)

    with zipfile.ZipFile(args.out_zip, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(args.boot_dol, f"{APP_NAME}/boot.dol")
        z.write(APP_ICON, f"{APP_NAME}/icon.png")
        z.writestr(f"{APP_NAME}/meta.xml",
                   render_meta(args.source, regions, args.version))
        if test_log:
            z.write(test_log, f"{APP_NAME}/TESTING.md")
        if args.changelog:
            z.write(args.changelog, f"{APP_NAME}/CHANGELOG.md")
        for bin_path in mod_bins:
            z.write(bin_path, f"{APP_NAME}/{bin_path.name}")
        if "lmj" in regions:
            z.write(LAUNCHER_DIR.parent / "lm_diag/vendor/miniz/LICENSE",
                    f"{APP_NAME}/licenses/miniz.txt")
            z.write(LAUNCHER_DIR.parent / "lm_diag/vendor/lz4/LICENSE",
                    f"{APP_NAME}/licenses/lz4.txt")
            # Theme lives on the SD root, not under Apps. Only package the
            # explicitly supplied assets, never user config, keys or saves.
            for name in THEME_FILES:
                asset = THEME_DIR / name
                if asset.is_file():
                    z.write(asset, f"Darkmoonshine_Theme/{name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
