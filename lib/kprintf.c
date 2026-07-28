/* baSic_ - lib/kprintf.c
 * Copyright (C) 2026 Dhrubo
 * GPL v2 — see LICENSE
 * minimal kernel printf backed by vga_putchar + klog
 * supports: %s %d %u %x %c %%
 */
#include "kprintf.h"
#include "../kernel/vga.h"
#include "../kernel/klog.h"
#include "../include/types.h"

/* va_list without stdarg.h — x86 cdecl: args pushed right-to-left on stack */
typedef u8 *va_list;
#define va_start(ap, last)  (ap = (va_list)&(last) + sizeof(last))
#define va_arg(ap, T)       (*(T *)((ap += sizeof(T)) - sizeof(T)))
#define va_end(ap)          ((void)0)

/* output buffer — kprintf writes here then flushes to VGA + klog */
static char kbuf[256];
static int  kbuf_pos = 0;

static void kbuf_flush(void)
{
    if (!kbuf_pos) return;
    kbuf[kbuf_pos] = '\0';
    vga_print(kbuf);
    klog_write(kbuf);
    kbuf_pos = 0;
}

static void kbuf_putc(char c)
{
    if (kbuf_pos >= 255) kbuf_flush();
    kbuf[kbuf_pos++] = c;
    /* flush on newline so dmesg shows complete lines */
    if (c == '\n') kbuf_flush();
}

static void kbuf_puts(const char *s)
{
    while (*s) kbuf_putc(*s++);
}

static void kbuf_uint(u32 val, int base)
{
    const char digits[] = "0123456789abcdef";
    char  buf[32];
    int   pos = 31;
    buf[31] = '\0';
    if (val == 0) { kbuf_putc('0'); return; }
    while (val > 0) { buf[--pos] = digits[val % base]; val /= base; }
    kbuf_puts(&buf[pos]);
}

static void kbuf_int(i32 val)
{
    if (val < 0) { kbuf_putc('-'); kbuf_uint((u32)(-(val+1))+1, 10); }
    else kbuf_uint((u32)val, 10);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') { kbuf_putc(*fmt++); continue; }
        fmt++;
        switch (*fmt) {
        case 's': { const char *s = va_arg(ap, const char *); kbuf_puts(s ? s : "(null)"); break; }
        case 'd': kbuf_int(va_arg(ap, i32));           break;
        case 'u': kbuf_uint(va_arg(ap, u32), 10);      break;
        case 'x': kbuf_puts("0x"); kbuf_uint(va_arg(ap, u32), 16); break;
        case 'c': kbuf_putc((char)va_arg(ap, int));    break;
        case '%': kbuf_putc('%');                       break;
        default:  kbuf_putc('%'); kbuf_putc(*fmt);     break;
        }
        fmt++;
    }
    kbuf_flush();
    va_end(ap);
}