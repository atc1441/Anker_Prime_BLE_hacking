/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Artur Bieniek */
/* Link script of the ARM size build: sizes only, the image never runs. */

ENTRY(dmc_size_entry)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08007000, LENGTH = 1M
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 1M
}

SECTIONS
{
    .text : { KEEP(*(.text.dmc_size_entry)) *(.text*) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.ARM.attributes) *(.comment) }
}

/* Frame region of the firmware link (platform/link/regions.ld:
   FRAME_BASE = 0x20000000 + 2048 + 5216), the largest plan, the tic
   scratch in the frame region (FRAME_BASE + 6912) and the end of SRAM,
   where the level arena ends. */
__frame_start = ORIGIN(RAM) + 0x1C60;
__frame_max = 15208;
PROVIDE(dmc_tic = __frame_start + 6912);
PROVIDE(__ram_end = 0x20008000);
