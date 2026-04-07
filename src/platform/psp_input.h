/*
 * psp_input.h - PSP controller input API
 * Redneck Rampage PSP Port
 */

#ifndef PSP_INPUT_H
#define PSP_INPUT_H

#include "compat.h"

/* Button mapping for game actions */
typedef struct {
    /* Movement */
    int32_t move_fb;        /* Forward/backward (-128 to 127) */
    int32_t move_lr;        /* Strafe left/right (-128 to 127) */
    int32_t look_lr;        /* Turn left/right (-128 to 127) */
    int32_t look_ud;        /* Look up/down (-128 to 127) */

    /* Actions (0 = released, 1 = pressed this frame) */
    int fire;               /* Fire weapon (X / Cross) */
    int use;                /* Use / Open (Square) */
    int jump;               /* Jump (Triangle) */
    int crouch;             /* Crouch (Circle) */
    int prev_weapon;        /* Previous weapon (L trigger) */
    int next_weapon;        /* Next weapon (R trigger) */
    int menu;               /* Pause / Menu (Start) */
    int map;                /* Automap (Select) */
    int run;                /* Run modifier (L+R triggers) */

    /* Raw button state */
    uint32_t buttons;
    uint32_t prev_buttons;
} psp_input_t;

/* Initialize input system */
void psp_input_init(void);

/* Poll input state (call once per frame) */
void psp_input_poll(psp_input_t *input);

/* Check if a button was just pressed (edge detect) */
int psp_input_pressed(psp_input_t *input, uint32_t btn);

/* Check if a button is held down */
int psp_input_held(psp_input_t *input, uint32_t btn);

#endif /* PSP_INPUT_H */
