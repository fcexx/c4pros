c4pros is a non-standard C library designed for 16-bit binary applications on x16-PRos. It includes GAS and NASM parts.

---

- Build:
```bash
# 1. Build the NASM part
nasm -f elf32 c4pros.asm -o c4pros.asmo

# 2. Build the GAS part
i686-elf-gcc -c -Os -ffreestanding -m16 c4pros.c -o c4pros.co

# 3. Link the library and your program into ELF
i686-elf-ld -T <linker file> -o program.elf c4pros.asmo c4pros.co <other .o files>

# 4. Convert to a 16-bit binary
i686-elf-objcopy -O binary program.elf prog.BIN
```

Run: `prog.BIN`

- Example linker file:
```ld
OUTPUT_FORMAT("elf32-i386") // Primary format: ELF32

// Program entry point: __start:
ENTRY(__start)

// Section in the NASM part: .text.start

SECTIONS
{
  // Required program entry address: 0x8000 by x16-PRos standard
  . = 0x8000;

  // Default NASM program start section first
  .text.start : {
    *(.text.start)
  }

  // C code entry at address 0x8100: entry_c
  . = 0x8100;
  .text : {
    *(.text .text.*)
  }

  __entry_start = entry_c;
  
  .rodata : {
    *(.rodata .rodata.*)
  }
  .data : {
    *(.data .data.*)
  }
  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }
  _end = .;
}

```

---
NASM entry point example:
```asm
[BITS 16]

section .text.start
global __start
extern __entry_start

__start:
    push cs
    pop ds
    push cs
    pop es

    ; jump to C entry
    jmp __entry_start

    ret
```

C entry point example:
```c
#include "c4pros.h"

void entry_c() {
    // Your program
    c4pros_set_pixel(0xf, 10, 10);
    // ...

    return;
}
```