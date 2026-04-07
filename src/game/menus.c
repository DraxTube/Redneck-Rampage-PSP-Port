/*
 * menus.c - Menu system and HUD implementation
 * Redneck Rampage PSP Port
 *
 * Renders menus and HUD using direct framebuffer drawing.
 * Uses a built-in bitmap font for text rendering.
 */

#include "menus.h"
#include "psp_display.h"
#include "psp_timer.h"
#include <pspctrl.h>

/* ============================================================
 * Simple bitmap font (4x6 pixel glyphs, ASCII 32-127)
 * ============================================================ */
static const uint8_t font_4x6[96][6] = {
    {0,0,0,0,0,0},         /* space */
    {4,4,4,0,4,0},         /* ! */
    {10,10,0,0,0,0},       /* " */
    {10,15,10,15,10,0},    /* # */
    {6,11,6,13,6,0},       /* $ */
    {9,2,4,9,0,0},         /* % */
    {4,10,4,11,13,0},      /* & */
    {4,4,0,0,0,0},         /* ' */
    {2,4,4,4,2,0},         /* ( */
    {4,2,2,2,4,0},         /* ) */
    {0,10,4,10,0,0},       /* * */
    {0,4,14,4,0,0},        /* + */
    {0,0,0,4,4,8},         /* , */
    {0,0,14,0,0,0},        /* - */
    {0,0,0,0,4,0},         /* . */
    {1,2,4,8,0,0},         /* / */
    {6,9,9,9,6,0},         /* 0 */
    {4,12,4,4,14,0},       /* 1 */
    {6,9,2,4,15,0},        /* 2 */
    {6,9,2,9,6,0},         /* 3 */
    {2,6,10,15,2,0},       /* 4 */
    {15,8,14,1,14,0},      /* 5 */
    {6,8,14,9,6,0},        /* 6 */
    {15,1,2,4,4,0},        /* 7 */
    {6,9,6,9,6,0},         /* 8 */
    {6,9,7,1,6,0},         /* 9 */
    {0,4,0,4,0,0},         /* : */
    {0,4,0,4,4,8},         /* ; */
    {2,4,8,4,2,0},         /* < */
    {0,15,0,15,0,0},       /* = */
    {8,4,2,4,8,0},         /* > */
    {6,9,2,0,4,0},         /* ? */
    {6,9,11,8,6,0},        /* @ */
    {6,9,15,9,9,0},        /* A */
    {14,9,14,9,14,0},      /* B */
    {6,9,8,9,6,0},         /* C */
    {14,9,9,9,14,0},       /* D */
    {15,8,14,8,15,0},      /* E */
    {15,8,14,8,8,0},       /* F */
    {6,8,11,9,6,0},        /* G */
    {9,9,15,9,9,0},        /* H */
    {14,4,4,4,14,0},       /* I */
    {7,2,2,10,4,0},        /* J */
    {9,10,12,10,9,0},      /* K */
    {8,8,8,8,15,0},        /* L */
    {9,15,15,9,9,0},       /* M */
    {9,13,11,9,9,0},       /* N */
    {6,9,9,9,6,0},         /* O */
    {14,9,14,8,8,0},       /* P */
    {6,9,9,11,7,0},        /* Q */
    {14,9,14,10,9,0},      /* R */
    {7,8,6,1,14,0},        /* S */
    {14,4,4,4,4,0},        /* T */
    {9,9,9,9,6,0},         /* U */
    {9,9,9,6,6,0},         /* V */
    {9,9,15,15,9,0},       /* W */
    {9,9,6,9,9,0},         /* X */
    {9,9,7,1,6,0},         /* Y */
    {15,2,4,8,15,0},       /* Z */
    {6,4,4,4,6,0},         /* [ */
    {8,4,2,1,0,0},         /* \ */
    {6,2,2,2,6,0},         /* ] */
    {4,10,0,0,0,0},        /* ^ */
    {0,0,0,0,15,0},        /* _ */
    {4,2,0,0,0,0},         /* ` */
    {0,6,10,10,7,0},       /* a */
    {8,14,9,9,14,0},       /* b */
    {0,6,8,8,6,0},         /* c */
    {1,7,9,9,7,0},         /* d */
    {0,6,11,12,6,0},       /* e */
    {2,4,14,4,4,0},        /* f */
    {0,7,9,7,1,6},         /* g */
    {8,14,9,9,9,0},        /* h */
    {4,0,4,4,4,0},         /* i */
    {2,0,2,2,10,4},        /* j */
    {8,10,12,10,9,0},      /* k */
    {4,4,4,4,2,0},         /* l */
    {0,9,15,9,9,0},        /* m */
    {0,14,9,9,9,0},        /* n */
    {0,6,9,9,6,0},         /* o */
    {0,14,9,14,8,8},       /* p */
    {0,7,9,7,1,1},         /* q */
    {0,6,8,8,8,0},         /* r */
    {0,7,12,3,14,0},       /* s */
    {4,14,4,4,2,0},        /* t */
    {0,9,9,9,7,0},         /* u */
    {0,9,9,6,6,0},         /* v */
    {0,9,9,15,6,0},        /* w */
    {0,9,6,6,9,0},         /* x */
    {0,9,7,1,14,0},        /* y */
    {0,15,2,4,15,0},       /* z */
    {2,4,8,4,2,0},         /* { */
    {4,4,4,4,4,0},         /* | */
    {8,4,2,4,8,0},         /* } */
    {0,5,10,0,0,0},        /* ~ */
    {15,15,15,15,15,0},    /* DEL */
};

/* Draw a single character at (x,y) with color, 2x scale */
static void draw_char(int px, int py, char c, uint32_t color) {
    if (c < 32 || c > 127) c = '?';
    int idx = c - 32;
    const uint8_t *glyph = font_4x6[idx];

    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 4; col++) {
            if (glyph[row] & (8 >> col)) {
                /* 2x scale */
                int sx = px + col * 2;
                int sy = py + row * 2;
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int fx = sx + dx;
                        int fy = sy + dy;
                        if (fx >= 0 && fx < XDIM && fy >= 0 && fy < YDIM) {
                            framebuffer[fy * XDIM + fx] = color;
                        }
                    }
                }
            }
        }
    }
}

/* Draw a string at (x,y) */
static void draw_string(int x, int y, const char *str, uint32_t color) {
    while (*str) {
        draw_char(x, y, *str, color);
        x += 10; /* char width (4*2 + 2 spacing) */
        str++;
    }
}

/* Draw centered string */
static void draw_string_center(int y, const char *str, uint32_t color) {
    int len = 0;
    const char *s = str;
    while (*s++) len++;
    int x = (XDIM - len * 10) / 2;
    draw_string(x, y, str, color);
}

/* Draw a filled rectangle */
static void draw_rect(int x1, int y1, int x2, int y2, uint32_t color) {
    x1 = kmax(x1, 0); y1 = kmax(y1, 0);
    x2 = kmin(x2, XDIM); y2 = kmin(y2, YDIM);
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            framebuffer[y * XDIM + x] = color;
        }
    }
}

/* Draw a horizontal bar */
static void draw_bar(int x, int y, int w, int h, int fill, int max,
                     uint32_t fg, uint32_t bg) {
    draw_rect(x, y, x + w, y + h, bg);
    int fw = (fill * w) / max;
    if (fw > w) fw = w;
    if (fw > 0) {
        draw_rect(x, y, x + fw, y + h, fg);
    }
}

/* Draw number right-aligned */
static void draw_number(int x, int y, int num, uint32_t color) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", num);
    draw_string(x, y, buf, color);
}

/* ============================================================
 * Menu system
 * ============================================================ */
static menu_t menus[MENU_COUNT];
static menu_id_t current_menu = MENU_MAIN;

void menus_init(void) {
    /* Main menu */
    menus[MENU_MAIN].id = MENU_MAIN;
    menus[MENU_MAIN].title = "REDNECK RAMPAGE";
    menus[MENU_MAIN].num_items = 3;
    menus[MENU_MAIN].selected = 0;
    menus[MENU_MAIN].items[0] = "NEW GAME";
    menus[MENU_MAIN].items[1] = "OPTIONS";
    menus[MENU_MAIN].items[2] = "QUIT";

    /* Episode select */
    menus[MENU_NEW_GAME].id = MENU_NEW_GAME;
    menus[MENU_NEW_GAME].title = "SELECT EPISODE";
    menus[MENU_NEW_GAME].num_items = 2;
    menus[MENU_NEW_GAME].selected = 0;
    menus[MENU_NEW_GAME].items[0] = "EP 1: HICKSTON";
    menus[MENU_NEW_GAME].items[1] = "EP 2: SUCKIN GRITS";

    /* Options */
    menus[MENU_OPTIONS].id = MENU_OPTIONS;
    menus[MENU_OPTIONS].title = "OPTIONS";
    menus[MENU_OPTIONS].num_items = 2;
    menus[MENU_OPTIONS].selected = 0;
    menus[MENU_OPTIONS].items[0] = "BACK";
    menus[MENU_OPTIONS].items[1] = "CONTROLS INFO";

    /* Pause menu */
    menus[MENU_PAUSE].id = MENU_PAUSE;
    menus[MENU_PAUSE].title = "PAUSED";
    menus[MENU_PAUSE].num_items = 3;
    menus[MENU_PAUSE].selected = 0;
    menus[MENU_PAUSE].items[0] = "RESUME";
    menus[MENU_PAUSE].items[1] = "QUIT TO MENU";
    menus[MENU_PAUSE].items[2] = "QUIT GAME";

    /* Game over */
    menus[MENU_GAMEOVER].id = MENU_GAMEOVER;
    menus[MENU_GAMEOVER].title = "YOU DIED!";
    menus[MENU_GAMEOVER].num_items = 2;
    menus[MENU_GAMEOVER].selected = 0;
    menus[MENU_GAMEOVER].items[0] = "TRY AGAIN";
    menus[MENU_GAMEOVER].items[1] = "QUIT TO MENU";

    /* Victory */
    menus[MENU_VICTORY].id = MENU_VICTORY;
    menus[MENU_VICTORY].title = "LEVEL COMPLETE!";
    menus[MENU_VICTORY].num_items = 2;
    menus[MENU_VICTORY].selected = 0;
    menus[MENU_VICTORY].items[0] = "NEXT LEVEL";
    menus[MENU_VICTORY].items[1] = "QUIT TO MENU";
}

void menus_update(game_t *g) {
    psp_input_t *inp = &g->input;
    menu_t *m;

    switch (g->state) {
        case GAMESTATE_TITLE:
            if (inp->fire || inp->use || inp->menu) {
                g->state = GAMESTATE_MENU;
                current_menu = MENU_MAIN;
            }
            return;

        case GAMESTATE_MENU:
            m = &menus[current_menu];
            break;

        case GAMESTATE_PAUSED:
            m = &menus[MENU_PAUSE];
            break;

        case GAMESTATE_GAMEOVER:
            m = &menus[MENU_GAMEOVER];
            break;

        case GAMESTATE_VICTORY:
            m = &menus[MENU_VICTORY];
            break;

        default:
            /* In game - check for pause */
            if (inp->menu) {
                g->state = GAMESTATE_PAUSED;
            }
            return;
    }

    /* Menu navigation */
    if (psp_input_pressed(inp, PSP_CTRL_UP)) {
        m->selected--;
        if (m->selected < 0) m->selected = m->num_items - 1;
    }
    if (psp_input_pressed(inp, PSP_CTRL_DOWN)) {
        m->selected++;
        if (m->selected >= m->num_items) m->selected = 0;
    }

    /* Selection */
    if (inp->fire || psp_input_pressed(inp, PSP_CTRL_CROSS)) {
        if (g->state == GAMESTATE_MENU) {
            switch (current_menu) {
                case MENU_MAIN:
                    switch (m->selected) {
                        case 0: /* New game */
                            current_menu = MENU_NEW_GAME;
                            menus[MENU_NEW_GAME].selected = 0;
                            break;
                        case 1: /* Options */
                            current_menu = MENU_OPTIONS;
                            break;
                        case 2: /* Quit */
                            g->state = GAMESTATE_QUIT;
                            break;
                    }
                    break;

                case MENU_NEW_GAME:
                    g->episode = m->selected + 1;
                    g->level = 1;
                    g->state = GAMESTATE_LOADING;
                    break;

                case MENU_OPTIONS:
                    if (m->selected == 0) {
                        current_menu = MENU_MAIN;
                    }
                    break;

                default:
                    break;
            }
        } else if (g->state == GAMESTATE_PAUSED) {
            switch (m->selected) {
                case 0: /* Resume */
                    g->state = GAMESTATE_PLAYING;
                    break;
                case 1: /* Quit to menu */
                    g->state = GAMESTATE_MENU;
                    current_menu = MENU_MAIN;
                    break;
                case 2: /* Quit game */
                    g->state = GAMESTATE_QUIT;
                    break;
            }
        } else if (g->state == GAMESTATE_GAMEOVER) {
            switch (m->selected) {
                case 0: /* Try again */
                    g->state = GAMESTATE_LOADING;
                    break;
                case 1: /* Quit to menu */
                    g->state = GAMESTATE_MENU;
                    current_menu = MENU_MAIN;
                    break;
            }
        } else if (g->state == GAMESTATE_VICTORY) {
            switch (m->selected) {
                case 0: /* Next level */
                    g->level++;
                    if (g->level > MAX_LEVELS) {
                        g->state = GAMESTATE_MENU;
                        current_menu = MENU_MAIN;
                    } else {
                        g->state = GAMESTATE_LOADING;
                    }
                    break;
                case 1:
                    g->state = GAMESTATE_MENU;
                    current_menu = MENU_MAIN;
                    break;
            }
        }
    }

    /* Back button */
    if (psp_input_pressed(inp, PSP_CTRL_CIRCLE)) {
        if (g->state == GAMESTATE_MENU) {
            if (current_menu != MENU_MAIN) {
                current_menu = MENU_MAIN;
            }
        } else if (g->state == GAMESTATE_PAUSED) {
            g->state = GAMESTATE_PLAYING;
        }
    }
}

void menus_draw(game_t *g) {
    menu_t *m;

    switch (g->state) {
        case GAMESTATE_MENU:   m = &menus[current_menu]; break;
        case GAMESTATE_PAUSED: m = &menus[MENU_PAUSE]; break;
        case GAMESTATE_GAMEOVER: m = &menus[MENU_GAMEOVER]; break;
        case GAMESTATE_VICTORY: m = &menus[MENU_VICTORY]; break;
        default: return;
    }

    /* Semi-transparent background overlay */
    for (int i = 0; i < XDIM * YDIM; i++) {
        uint32_t c = framebuffer[i];
        uint8_t r = ((c >> 0) & 0xFF) >> 1;
        uint8_t g_c = ((c >> 8) & 0xFF) >> 1;
        uint8_t b = ((c >> 16) & 0xFF) >> 1;
        framebuffer[i] = 0xFF000000 | (b << 16) | (g_c << 8) | r;
    }

    /* Menu box */
    int box_w = 200, box_h = 30 + m->num_items * 20;
    int box_x = (XDIM - box_w) / 2;
    int box_y = (YDIM - box_h) / 2;
    draw_rect(box_x, box_y, box_x + box_w, box_y + box_h, 0xC0000000);

    /* Title */
    draw_string_center(box_y + 8, m->title, 0xFF00FFFF);

    /* Items */
    for (int i = 0; i < m->num_items; i++) {
        int iy = box_y + 28 + i * 18;
        uint32_t color = (i == m->selected) ? 0xFF00FF00 : 0xFFCCCCCC;
        if (i == m->selected) {
            draw_string_center(iy, "> ", 0xFF00FF00);
        }
        draw_string_center(iy, m->items[i], color);
    }
}

void title_draw(game_t *g) {
    (void)g;
    /* Clear to dark red */
    for (int i = 0; i < XDIM * YDIM; i++) {
        framebuffer[i] = 0xFF080020;
    }

    /* Draw decorative border */
    draw_rect(8, 8, XDIM - 8, 12, 0xFF0044CC);
    draw_rect(8, YDIM - 12, XDIM - 8, YDIM - 8, 0xFF0044CC);
    draw_rect(8, 8, 12, YDIM - 8, 0xFF0044CC);
    draw_rect(XDIM - 12, 8, XDIM - 8, YDIM - 8, 0xFF0044CC);

    /* Title text */
    draw_string_center(40, "REDNECK RAMPAGE", 0xFF0000FF);
    draw_string_center(60, "PSP EDITION", 0xFF00CCFF);

    /* Subtitle */
    draw_string_center(100, "BUILD ENGINE PORT", 0xFFCCCCCC);

    /* Prompt */
    uint32_t blink = (psp_timer_get_ms() / 500) & 1;
    if (blink) {
        draw_string_center(150, "PRESS X TO START", 0xFFFFFFFF);
    }

    /* Credits */
    draw_string_center(180, "REQUIRES REDNECK.GRP", 0xFF888888);
}

void loading_draw(const char *text) {
    for (int i = 0; i < XDIM * YDIM; i++) {
        framebuffer[i] = 0xFF000000;
    }
    draw_string_center(90, "LOADING...", 0xFFFFFF00);
    if (text) {
        draw_string_center(110, text, 0xFFCCCCCC);
    }
}

/* ============================================================
 * HUD (in-game overlay)
 * ============================================================ */
void hud_draw(game_t *g) {
    /* Bottom HUD bar */
    draw_rect(0, YDIM - 20, XDIM, YDIM, 0xC0000000);

    /* Health */
    draw_string(4, YDIM - 18, "HP", 0xFF0000FF);
    draw_bar(22, YDIM - 16, 60, 8, g->health, PLAYER_MAX_HEALTH,
             0xFF0000FF, 0xFF000040);
    draw_number(86, YDIM - 18, g->health, 0xFFFFFFFF);

    /* Armor */
    draw_string(110, YDIM - 18, "AR", 0xFF00FFFF);
    draw_bar(128, YDIM - 16, 40, 8, g->armor, PLAYER_MAX_ARMOR,
             0xFF00CCCC, 0xFF004040);

    /* Current weapon & ammo */
    if (g->cur_weapon >= 0 && g->cur_weapon < WPN_COUNT) {
        const char *wpn_short[] = {
            "CRW", "REV", "SHT", "RIF", "DYN",
            "XBW", "KEG", "TIT", "BWL", "MNS"
        };
        draw_string(180, YDIM - 18, wpn_short[g->cur_weapon], 0xFFFFFF00);

        if (g->cur_weapon != WPN_CROWBAR) {
            draw_number(220, YDIM - 18, g->ammo[g->cur_weapon].current, 0xFFFFFFFF);
        }
    }

    /* Keys */
    uint32_t key_colors[4] = { 0xFF0000FF, 0xFFFF0000, 0xFF00FFFF, 0xFF00FF00 };
    for (int k = 0; k < 4; k++) {
        if (g->keys[k]) {
            draw_rect(260 + k * 12, YDIM - 16, 268 + k * 12, YDIM - 8, key_colors[k]);
        }
    }

    /* FPS counter */
    char fps_str[16];
    snprintf(fps_str, sizeof(fps_str), "%.0f", psp_timer_get_fps());
    draw_string(XDIM - 30, 2, fps_str, 0xFF00FF00);

    /* Kill count */
    char kills_str[32];
    snprintf(kills_str, sizeof(kills_str), "K:%d", g->kills);
    draw_string(XDIM - 50, YDIM - 18, kills_str, 0xFFCCCCCC);
}
