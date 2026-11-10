"""Soft-float Thumb build of the GD32 Doom image.

    python src/gd32/build.py <out-dir>

The image is DOOM.BIN in that directory. Build it twice into two
directories and compare the hashes; the build is reproducible. Nothing
here programs a chip.

Set ARM_GNU_TOOLCHAIN to the toolchain root if arm-none-eabi-gcc is not
on PATH.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "src")
GEN = os.path.join(SRC, "gen")


def tool(name):
    """arm-none-eabi-<name>, from ARM_GNU_TOOLCHAIN or from PATH."""
    exe = "arm-none-eabi-" + name
    root = os.environ.get("ARM_GNU_TOOLCHAIN")
    if root:
        for folder in (os.path.join(root, "bin"), root):
            path = os.path.join(folder, exe)
            for candidate in (path, path + ".exe"):
                if os.path.isfile(candidate):
                    return candidate
    found = shutil.which(exe)
    if found:
        return found
    raise SystemExit(
        f"{exe} not found. Put the toolchain on PATH or set ARM_GNU_TOOLCHAIN."
    )


CC = tool("gcc")
OBJCOPY = tool("objcopy")
SIZE = tool("size")
NM = tool("nm")

ARCH = [
    "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft",
    "-ffreestanding", "-fno-builtin", "-fno-common", "-fwrapv", "-fsigned-char",
    "-ffunction-sections", "-fdata-sections", "-std=gnu11",
]
DEFS = [
    "-DDMC_FW", "-DDMC_HASHORDER", "-DDMC_SINE_L1", "-DOVL_RESIDENT", "-DRC_SETS=256",
]
INC = [
    "-I" + os.path.join(SRC, "gd32"),
    "-I" + os.path.join(SRC, "input"),
    "-I" + os.path.join(SRC, "board"),
    "-I" + os.path.join(SRC, "platform"),
    "-I" + os.path.join(SRC, "platform", "apps"),
    "-I" + os.path.join(SRC, "port", "game"),
    "-I" + os.path.join(SRC, "port", "data"),
    "-I" + os.path.join(SRC, "port", "render"),
    "-I" + os.path.join(SRC, "port", "obj"),
    "-I" + os.path.join(SRC, "port", "math"),
    "-I" + os.path.join(SRC, "port", "ovl"),
    "-I" + os.path.join(SRC, "port", "usb"),
    "-I" + os.path.join(SRC, "engine"),
    "-I" + os.path.join(SRC, "engine", "stub", "include"),
    "-I" + os.path.join(SRC, "tests", "lockstep"),
    "-I" + os.path.join(SRC, "controller", "rc-n1"),
    "-I" + GEN,
]


def sources():
    eng_skip = {"am_map.c", "z_zone.c", "m_argv.c", "w_wad.c", "w_file.c"}
    game_skip = {"dmc_hash.c", "dmc_usb.c", "dmc_report.c"}
    out = []
    for name in sorted(os.listdir(os.path.join(SRC, "engine"))):
        if name.endswith(".c") and name not in eng_skip:
            out.append(os.path.join(SRC, "engine", name))
    for folder in ("port/data", "port/render", "port/obj", "port/game"):
        for name in sorted(os.listdir(os.path.join(SRC, folder))):
            if not name.endswith(".c"):
                continue
            if folder == "port/game" and name in game_skip:
                continue
            out.append(os.path.join(SRC, folder, name))
    out.append(os.path.join(SRC, "port", "math", "trig.c"))
    for name in ("info.c", "colormap.c", "trig_tables.c", "view.c"):
        out.append(os.path.join(GEN, name))
    for name in ("text.c", "font8x8.c", "settings.c", "rt.c", "usbd.c", "ovl_resident.c"):
        out.append(os.path.join(SRC, "platform", "py32", name))
    # The board layer. ui.c here replaces platform/apps/ui.c: the glass
    # shows only the middle 240 of the 320 columns the engine draws, so
    # text has to start at a margin instead of at column 0.
    for name in (
        "startup.c", "plat.c", "systick.c", "buttons.c", "buzzer.c", "lcd.c",
        "pump.c", "nor.c", "spi.c", "dbg.c", "fault.c", "gauge.c", "ble.c",
        "power.c", "adc.c", "timers.c", "ui.c", "splash.c",
    ):
        out.append(os.path.join(SRC, "gd32", name))
    out.append(os.path.join(SRC, "input", "gestures.c"))
    return out


def run(cmd):
    print(" ".join(cmd))
    subprocess.check_call(cmd)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build")
    obj = os.path.join(out, "obj")
    os.makedirs(obj, exist_ok=True)
    objs = []
    for src in sources():
        base = os.path.relpath(src, ROOT).replace(":", "").replace("\\", "_").replace("/", "_")
        dest = os.path.join(obj, base + ".o")
        slashed = src.replace("\\", "/")
        warn = ["-w"] if "/engine/" in slashed or src.endswith("info.c") else ["-Wall"]
        run([CC, "-O2"] + ARCH + DEFS + INC + warn + ["-c", src, "-o", dest])
        objs.append(dest)
    elf = os.path.join(out, "doom.elf")
    run([
        CC, "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft", "-nostdlib",
        "-Wl,--gc-sections", "-Wl,--defsym,__dmc_variant=3", "-Wl,--wrap=D_PostEvent",
        "-Wl,-T," + os.path.join(SRC, "gd32", "gd32.ld"),
        "-Wl,-Map," + os.path.join(out, "doom.map"),
    ] + objs + ["-lgcc", "-o", elf])
    bin_path = os.path.join(out, "DOOM.BIN")
    run([OBJCOPY, "-O", "binary", "-j", ".text", "-j", ".data", elf, bin_path])
    run([SIZE, elf])
    run([NM, "-u", elf])
    print("image", bin_path, os.path.getsize(bin_path))


if __name__ == "__main__":
    main()
