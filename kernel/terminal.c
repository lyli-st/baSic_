/* baSic_ - kernel/terminal.c
 * Copyright (C) 2026 Dhrubo
 * GPL v2 — see LICENSE
 *
 * terminal input driver
 * wraps keyboard_getchar() and decodes extended scancodes for
 * arrow keys and navigation keys using the PS/2 scancode set 1
 * extended prefix (0xE0)
 */

#include "terminal.h"
#include "keyboard.h"
#include "../lib/kprintf.h"

/* We hook into the keyboard IRQ at a lower level to catch 0xE0 prefix.
 * For now we use a simpler approach: expose extended key state from keyboard.c
 * and translate here.
 */

#define KBD_DATA_PORT   0x60

static volatile u8  last_ext   = 0;
static volatile u8  last_sc    = 0;
static volatile int key_ready  = 0;

static inline u8 inb_p(u16 port)
{
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void term_init(void)
{
    last_ext  = 0;
    last_sc   = 0;
    key_ready = 0;
    kprintf("[OK] terminal: VT100 decoder ready\n");
}

/* map extended scancode to term event */
static term_key_type_t decode_ext(u8 sc)
{
    switch (sc) {
    case 0x48: return TERM_UP;
    case 0x50: return TERM_DOWN;
    case 0x4B: return TERM_LEFT;
    case 0x4D: return TERM_RIGHT;
    case 0x47: return TERM_HOME;
    case 0x4F: return TERM_END;
    case 0x53: return TERM_DEL;
    case 0x49: return TERM_PGUP;
    case 0x51: return TERM_PGDN;
    default:   return TERM_NONE;
    }
}

term_event_t term_poll(void)
{
    term_event_t ev = { TERM_NONE, 0 };

    /* check extended key first — set by IRQ handler on 0xE0 prefix */
    u8 ext = keyboard_get_ext();
    if (ext) {
        ev.type = decode_ext(ext);
        return ev;
    }

    /* regular key — let keyboard.c handle it via its IRQ path
     * but we need to avoid double-reading; use keyboard_getchar() instead */
    /* We fall back to keyboard_getchar for regular keys */
    char c = keyboard_getchar();
    if (c) {
        ev.type = TERM_CHAR;
        ev.ch   = c;
    }
    return ev;
}