ENTRY(_start)

SECTIONS {
    . = 0x00400000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    _data_start = .;

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }

    _bss_end = .;
}