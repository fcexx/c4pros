c4pros - это стандартная библиотека на C, предназначенная для работы 16-битных бинарных приложений на x16-PRos. Состоит из GAS и NASM.

---

- Сборка:
```bash
# 1. Собираем NASM-часть
nasm -f elf32 c4pros.asm -o c4pros.asmo

# 2. Собираем GAS-часть
i686-elf-gcc -c -Os -ffreestanding -m16 c4pros.c -o c4pros.co

# 3. Линкуем библиотеку и Вашу программу в ELF
i686-elf-ld -T <linker file> -o program.elf c4pros.asmo c4pros.co <другие .o файлы>

# 4. Лепим 16-битный бинарник
i686-elf-objcopy -O binary program.elf prog.BIN
```

Запуск: `prog.BIN`

- Пример для linker file:
```ld
OUTPUT_FORMAT("elf32-i386") // Первичный формат: ELF32

// Пусть точка входа в Вашу программу: __start:
ENTRY(__start)

// Секция в программе в NASM-части: .text.start

SECTIONS
{
  // Обязательный адрес для входа в программу: 0x8000 по x16-PRos стандарту
  . = 0x8000;

  // Сперва старт по умолчанию в nasm программы
  .text.start : {
    *(.text.start)
  }

  // C-код — вход по адресу 0x8100; entry_c
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
Пример для точки входа программы для NASM-части:
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

    ; прыжок в си
    jmp __entry_start

    ret
```

Пример для точки входа на Си:
```c
#include "c4pros.h"

void entry_c() {
    // Ваша программа
    c4pros_set_pixel(0xf, 10, 10);
    // ...

    return;
}
```