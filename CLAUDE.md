# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make          # build BOOTX64.EFI + kernel.elf + bootable disk image, then run in QEMU
make image    # build everything but don't launch QEMU
make clean    # remove all build artifacts (also unmounts if mounted)
```

QEMU serial output is written to `qemu.log`. There are no tests.

## Architecture

This is a two-stage bare-metal x86-64 system: a UEFI bootloader that loads an ELF kernel.

### Bootloader (`src/`, target: `build/BOOTX64.EFI`)

Compiled with `clang -target x86_64-unknown-windows` (MS ABI throughout). Entry point is `UefiEntry` in `src/efi.c`.

The bootloader presents an interactive text menu before launching the kernel. The menu system is built on `EFI_MENU_PAGE` structs (`include/menu.h`), each with `ProcessInput` and `Update` function pointers. `src/menu.c` holds `DefaultPage`. Each screen is a separate file: `main_menu.c`, `screeninfo_menu.c`, `gopinfo_menu.c`, `kernelstart_menu.c`.

Global EFI handles (`BS`, `ST`, `RS`, `GOP`, `cIn`, `cOut`, `cErr`) are initialized once in `src/efilib.c:InitGlobalVars()` and used as globals everywhere. `Printf` in the bootloader takes `CHAR16*` format strings (UTF-16 literals: `u"..."`).

### Kernel handoff (`src/kernelstart_menu.c`)

When the user selects "Launch Kernel", `KernelStart()`:
1. Opens the FAT32 boot volume via `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` and reads `kernel.elf`
2. Validates ELF64 magic and parses `PT_LOAD` segments, calling `AllocatePages(AllocateAddress, ...)` for each — **addresses must be page-aligned**, which is enforced by `kernel/linker.ld`
3. Captures framebuffer info and ACPI RSDP from the config table
4. Calls `GetMemoryMap` then `ExitBootServices` (with one retry on stale map key)
5. Calls `KernelMain(BootInfo*)` using MS ABI (pointer in RCX)

### Kernel (`kernel/src/`, target: `build/kernel.elf`)

Compiled with plain `clang` (SysV ABI), but `KernelMain` is declared `__attribute__((ms_abi))` to match how the bootloader calls it. Linked at physical base `0x400000` by `kernel/linker.ld`. No libc, no virtual memory, no paging — runs directly at physical addresses.

`BootInfo` (`include/bootinfo.h`) carries: framebuffer base/dimensions/pitch/format, memory map pointer/size/descriptor-size/version, and ACPI RSDP address.

Current kernel (`kernel/src/kmain.c`): clears the framebuffer to black, then prints "Kernel booted!" and the screen resolution using the 8×8 bitmap font renderer.

### Font renderer (`kernel/src/font.c`, `include/font.h`)

`font_draw_char` and `font_draw_str` blit 8×8 glyphs from a hardcoded IBM CP437 `font8x8[256][8]` table. Each byte is one row; **bit 0 (LSB) is the leftmost pixel**. Framebuffer colors are `0x00RRGGBB` in BGRx format (matching OVMF's `PixelBlueGreenRedReserved8BitPerColor`).

### Linker script (`kernel/linker.ld`)

Sections are page-aligned with `. = ALIGN(4096)` between `.text`, `.rodata`, `.data`, and `.bss`. This is required because `AllocatePages(AllocateAddress, ...)` demands page-aligned `p_paddr` values, and lld emits separate `PT_LOAD` segments per permission group. It also prepares for per-section page protections once paging is enabled.

## Key constraints

- Bootloader strings and `Printf` format strings must be UTF-16 (`u"..."`)
- Kernel has no stdlib — use `<stdint.h>` (clang builtin, works with `-ffreestanding`) but nothing else from libc
- New `.c` files in `kernel/src/` are compiled automatically (wildcard). New `.c` files in `src/` likewise.
- Editing `kernel/linker.ld` does **not** auto-trigger a relink unless a kernel source file also changes — touch one if needed
