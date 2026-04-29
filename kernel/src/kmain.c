#include "../../include/bootinfo.h"

// Entry point uses MS ABI so the bootloader (compiled with -target x86_64-unknown-windows)
// passes BootInfo* in RCX rather than RDI (SysV ABI).
__attribute__((ms_abi)) void KernelMain(BootInfo *info)
{
    uint32_t *fb     = (uint32_t *)info->FramebufferBase;
    uint32_t  stride = info->FramebufferPitch / 4;

    // Fill the framebuffer with red (0x00FF0000 = red in BGR, the typical OVMF format)
    for (uint32_t y = 0; y < info->FramebufferHeight; y++)
        for (uint32_t x = 0; x < info->FramebufferWidth; x++)
            fb[y * stride + x] = 0x00FF0000u;
    
    while (1) __asm__ volatile("hlt");
}
