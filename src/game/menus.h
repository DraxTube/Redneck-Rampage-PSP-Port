/*
 * menus.h - Menu system definitions
 * Redneck Rampage PSP Port
 */

#ifndef MENUS_H
#define MENUS_H

#include "game.h"

/* Menu IDs */
typedef enum {
    MENU_MAIN,
    MENU_NEW_GAME,
    MENU_OPTIONS,
    MENU_PAUSE,
    MENU_GAMEOVER,
    MENU_VICTORY,
    MENU_COUNT
} menu_id_t;

#define MENU_MAX_ITEMS 8

typedef struct {
    menu_id_t id;
    int        num_items;
    int        selected;
    const char *title;
    const char *items[MENU_MAX_ITEMS];
} menu_t;

/* Initialize menu system */
void menus_init(void);

/* Process menu input */
void menus_update(game_t *g);

/* Draw current menu to framebuffer */
void menus_draw(game_t *g);

/* Draw the HUD (in-game overlay) */
void hud_draw(game_t *g);

/* Draw title screen */
void title_draw(game_t *g);

/* Draw loading screen */
void loading_draw(const char *text);

#endif /* MENUS_H */
