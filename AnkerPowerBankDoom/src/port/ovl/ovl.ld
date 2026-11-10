/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Artur Bieniek */

/* Link script of one overlay. Used as
     ld.lld -T port/ovl/ovl.ld --just-symbols=main.elf --gc-sections --emit-relocs objs
   so the resident symbols, __frame_start (FRAME_BASE) and __frame_min (FRAME_MIN) come from
   main.elf. The blob is the .ovl section: header, entry table (OVL_TABLE), code, rodata and
   data. The bss word of the header spans from the end of the blob to the end of .ovl_bss,
   alignment gap included, and the loader clears all of it. pack_overlays.py refuses
   overlays with .data or .bss (between __ovl_data_start and __ovl_data_end, or in .ovl_bss)
   and needs the relocations. */

ENTRY(__ovl_start)

SECTIONS
{
    .ovl __frame_start : AT(__frame_start)
    {
        __ovl_start = .;
        LONG(0x314C564F)                      /* "OVL1" */
        LONG(__frame_start)
        LONG(__ovl_copy_end - __ovl_start)
        LONG(__ovl_bss_end - __ovl_copy_end)
        KEEP(*(.ovl_entries))
        __ovl_entries_end = .;
        *(.text .text.*)
        *(.rodata .rodata.*)
        __ovl_data_start = .;
        *(.data .data.*)
        __ovl_data_end = .;
        . = ALIGN(4);
        __ovl_copy_end = .;
    }

    .ovl_bss (NOLOAD) :
    {
        __ovl_bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __ovl_bss_end = .;
    }

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}

ASSERT(__ovl_entries_end > __ovl_start + 16, "overlay has no entry table (OVL_TABLE)")
ASSERT(__ovl_bss_end - __ovl_start <= __frame_min, "overlay is larger than FRAME_MIN")
