OUTPUT_FORMAT(binary)
SECTIONS
{       . = 0x1000;     
        .start : { *(.start) }
        .text : { *(.text*) }
        .rodata : { *(.rodata) }
        .data : { *(.data) }
}
