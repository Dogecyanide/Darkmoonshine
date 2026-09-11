"""Create an isolated Dolphin profile for an owned GLMJ01 development ISO.

The GCI-folder card is for normal boot. Dolphin 5.0 DTM playback overrides it
with RAW-card selection; see write_lm_dtm.py before replaying an input movie.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


def prepare(profile, save=None):
    profile = Path(profile).resolve()
    marker = profile / "lm-test-profile.json"
    if profile.exists() and any(profile.iterdir()):
        raise ValueError("test profile must be new/empty; existing Dolphin profiles are never modified")
    save_bytes = None
    if save:
        save_bytes = Path(save).read_bytes()
        if len(save_bytes) < 64 or save_bytes[:6] != b"GLMJ01":
            raise ValueError("save must be a Japanese Luigi's Mansion GCI")
        blocks = struct.unpack_from(">H", save_bytes, 0x38)[0]
        if len(save_bytes) != 64 + blocks * 8192:
            raise ValueError("GCI block count does not match file size")
    config = profile / "Config"
    config.mkdir(parents=True, exist_ok=True)
    card = profile / "GC" / "JAP" / "Card A"
    card.mkdir(parents=True, exist_ok=True)
    (config / "Dolphin.ini").write_text(
        "[General]\nISOPaths = 0\nRecursiveISOPaths = False\n"
        "[Core]\nHLE_BS2 = True\nCPUCore = 1\nCPUThread = False\n"
        "DSPHLE = True\nMMU = False\nEnableCheats = False\n"
        "SelectedLanguage = 0\nOverrideGCLang = True\n"
        "SlotA = 8\nSlotB = 255\nSIDevice0 = 6\n"
        "SIDevice1 = 0\nSIDevice2 = 0\nSIDevice3 = 0\n"
        "GFXBackend = OGL\nEmulationSpeed = 1.0\n"
        "[Interface]\nConfirmStop = False\nUsePanicHandlers = True\n"
        "PauseOnFocusLost = False\nOnScreenDisplayMessages = True\n"
        "[Display]\nFullscreen = False\nRenderToMain = True\n"
        "RenderWindowWidth = 960\nRenderWindowHeight = 720\n"
        "ProgressiveScan = False\nPAL60 = True\n"
        "[DSP]\nBackend = No audio output\n", encoding="utf-8")
    (config / "GFX.ini").write_text(
        "[Hardware]\nVSync = False\n[Settings]\nShowFPS = True\n"
        "UseXFB = True\nUseRealXFB = True\nEFBScale = 2\n"
        "[Enhancements]\nMaxAnisotropy = 0\n",
        encoding="utf-8")
    (config / "GCPadNew.ini").write_text(
        "[GCPad1]\nDevice = DInput/0/Keyboard Mouse\n"
        "Buttons/A = X\nButtons/B = Z\nButtons/X = C\nButtons/Y = V\n"
        "Buttons/Z = Q\nButtons/Start = Return\n"
        "Main Stick/Up = W\nMain Stick/Down = S\nMain Stick/Left = A\nMain Stick/Right = D\n"
        "C-Stick/Up = I\nC-Stick/Down = K\nC-Stick/Left = J\nC-Stick/Right = L\n"
        "Triggers/L = E\nTriggers/R = R\n"
        "D-Pad/Up = Up\nD-Pad/Down = Down\nD-Pad/Left = Left\nD-Pad/Right = Right\n",
        encoding="utf-8")
    (config / "Logger.ini").write_text(
        "[Options]\nVerbosity = 3\nWriteToFile = True\nWriteToConsole = False\n"
        "WriteToWindow = False\n[Logs]\nBOOT = True\nOSREPORT = True\n"
        "MASTER = True\n", encoding="utf-8")
    if save_bytes:
        (card / "01-GLMJ-LMHiddenMansion.gci").write_bytes(save_bytes)
    marker.write_text(json.dumps({
        "purpose": "isolated Luigi's Mansion development tests",
        "memory_card_backend": "gci-folder",
        "dolphin_5_movie_requires_raw_card": True,
        "save_sha256": hashlib.sha256(save_bytes).hexdigest() if save_bytes else None,
        "keys": {"A": "X", "B": "Z", "stick": "WASD", "menu": "Down",
                 "save": "Left", "load": "Right", "L/R": "E/R"},
    }, indent=2) + "\n", encoding="utf-8")
    return profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--save")
    args = parser.parse_args()
    print(prepare(args.profile, args.save))


if __name__ == "__main__":
    main()
