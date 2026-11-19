OUTPUT_FORMAT(binary)
ENTRY(_start)
SECTIONS {
  . = 0x100;
  .text : { *(.text.start) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  __bss_end = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
