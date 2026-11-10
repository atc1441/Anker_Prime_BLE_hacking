/* GD32F303VET6: 512 KB flash, 64 KB SRAM.
   Frame and cache sizes stay the ones the converted packs were planned for. */

MEMORY
{
    FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 0x7FC00
    SETTINGS (r)   : ORIGIN = 0x0807FC00, LENGTH = 0x400
    RAM      (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000
}

STACK_SIZE = 0x1000;
CORE_SIZE  = 0x3800;

FRAME_BASE = ORIGIN(RAM) + STACK_SIZE + CORE_SIZE;
RAM_END    = ORIGIN(RAM) + LENGTH(RAM);

FRAME_MIN = 8368;
FRAME_MAX = 15208;
CACHE_MIN = 18688;
CACHE_MAX = 22784;
ARENA_MIN = 3848;
SETTINGS_PAGE = ORIGIN(SETTINGS);

ENTRY(Reset_Handler)

TIC_OFFSET = 6912;
TIC_SIZE   = 1456;
STRIP_MAX  = 6400;
RC_SIZE    = 18688;
FLAT_SLOT  = 1024;
USB_RAM    = 4608;

SECTIONS
{
    .stack (NOLOAD) :
    {
        __stack_bottom = .;
        . += STACK_SIZE;
        __stack_top = .;
    } > RAM

    .text :
    {
        KEEP(*(.vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        __data_start = .;
        *(SORT_BY_ALIGNMENT(.data*))
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > FLASH
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
    __core_end = .;

    .dynram FRAME_BASE (NOLOAD) :
    {
        __frame_start = .;
        __dyn_start = .;
        *(.dynram .dynram.*)
        . = ALIGN(4);
        __dyn_free = .;
    } > RAM
    __ram_end = RAM_END;

    __image_end = LOADADDR(.data) + SIZEOF(.data);
    __settings_page = SETTINGS_PAGE;
    __frame_min = FRAME_MIN;
    __frame_max = FRAME_MAX;
    __core_size = __core_end - __data_start;
    dmc_tic = FRAME_BASE + TIC_OFFSET;
    usb_ram = FRAME_BASE + FRAME_MIN;

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}

ASSERT(__stack_bottom == ORIGIN(RAM), "the stack must start at 0x20000000")
ASSERT(__stack_top == ORIGIN(RAM) + STACK_SIZE && STACK_SIZE >= 0x800, "stack")
ASSERT(FRAME_BASE == ORIGIN(RAM) + STACK_SIZE + CORE_SIZE, "frame region")
ASSERT(__data_start == __stack_top, "core follows the stack")
ASSERT(__core_end <= FRAME_BASE, "fixed core over CORE_SIZE")
ASSERT(FRAME_BASE % 16 == 0, "FRAME_BASE alignment")
ASSERT(__dyn_free == FRAME_BASE, "no static data in the frame region")
ASSERT(TIC_OFFSET >= STRIP_MAX, "tic scratch overlaps strips")
ASSERT(TIC_OFFSET + TIC_SIZE <= FRAME_MIN, "tic scratch past the frame")
ASSERT(dmc_tic % 4 == 0, "tic scratch alignment")
ASSERT(CACHE_MIN == RC_SIZE && CACHE_MAX >= RC_SIZE + 4 * FLAT_SLOT, "cache budget")
ASSERT(FRAME_BASE + FRAME_MAX + CACHE_MAX + ARENA_MIN <= RAM_END, "largest plan")
ASSERT(FRAME_BASE + FRAME_MIN + CACHE_MIN + ARENA_MIN <= RAM_END, "smallest plan")
ASSERT(RAM_END == ORIGIN(RAM) + 0x10000, "SRAM end")
ASSERT(usb_ram + USB_RAM <= RAM_END, "usb ram")
ASSERT(__image_end <= ORIGIN(SETTINGS), "image runs into the last page")
ASSERT(SETTINGS_PAGE == 0x0807FC00, "settings page")
