#include "../../include/bootinfo.h"
#include "../../include/font.h"

#define FG 0x00FFFFFF  // white  (BGRx: R=0xFF G=0xFF B=0xFF)
#define BG 0x00000000  // black

static void u32_to_dec(uint32_t num, char *num_buf)
{
    if (num == 0) {
        num_buf[0] = '0';
        num_buf[1] = '\0';
        return;
    }
    char tmp[12];
    int i = 0;
    while (num) {
        tmp[i++] = '0' + (char)(num % 10);
        num /= 10;
    }
    int j = 0;
    while (i > 0) num_buf[j++] = tmp[--i];
    num_buf[j] = '\0';
}

__attribute__((ms_abi)) void KernelMain(BootInfo *info)
{
    uint32_t *fb     = (uint32_t *)info->FramebufferBase;
    uint32_t  stride = info->FramebufferPitch / 4;

    // Clear to black
    for (uint32_t y = 0; y < info->FramebufferHeight; y++)
        for (uint32_t x = 0; x < info->FramebufferWidth; x++)
            fb[y * stride + x] = BG;

    char num[12];

    font_draw_str(fb, stride, 8,  8, "Kernel booted!", FG, BG);

    font_draw_str(fb, stride, 8, 24, "Width:  ", FG, BG);
    u32_to_dec(info->FramebufferWidth, num);
    font_draw_str(fb, stride, 72, 24, num, FG, BG);

    font_draw_str(fb, stride, 8, 32, "Height: ", FG, BG);
    u32_to_dec(info->FramebufferHeight, num);
    font_draw_str(fb, stride, 72, 32, num, FG, BG);

    while (1) __asm__ volatile("hlt");
}
