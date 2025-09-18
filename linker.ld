ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : ALIGN(4) {
        *(.text.boot)
        *(.text*)
    }

    .rodata : ALIGN(8) {
        *(.rodata*)
    }

    .data : ALIGN(8) {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    __stack_top = . + 0x00040000; /* 256 KiB stack reserved */
}
