#pragma once
#include <stdint.h>

// ELF magic bytes (e_ident[0..3])
#define ELFMAG0 0x7Fu
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// e_ident index constants
#define EI_CLASS  4
#define EI_DATA   5

// EI_CLASS values
#define ELFCLASS64  2

// EI_DATA values
#define ELFDATA2LSB 1

// e_type values
#define ET_EXEC 2
#define ET_DYN  3

// e_machine values
#define EM_X86_64 62

// p_type values
#define PT_LOAD 1

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;
