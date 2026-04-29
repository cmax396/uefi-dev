#pragma once
#include <stdint.h>

typedef struct {
    uint64_t FramebufferBase;
    uint32_t FramebufferWidth;
    uint32_t FramebufferHeight;
    uint32_t FramebufferPitch;   // bytes per scan line
    uint32_t FramebufferFormat;  // EFI_GRAPHICS_PIXEL_FORMAT value

    uint64_t MemoryMapBase;      // pointer to EFI_MEMORY_DESCRIPTOR array
    uint64_t MemoryMapSize;      // total size in bytes
    uint64_t MemoryMapDescSize;  // size of one descriptor
    uint32_t MemoryMapDescVer;
    uint32_t _reserved;

    uint64_t AcpiRsdp;           // physical address of ACPI RSDP, 0 if not found
} BootInfo;
