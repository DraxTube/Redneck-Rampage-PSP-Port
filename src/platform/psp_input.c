/*
 * psp_input.c - PSP controller input implementation
 * Redneck Rampage PSP Port
 */

#include "psp_input.h"
#include <pspctrl.h>

#define ANALOG_DEADZONE  32
#define ANALOG_CENTER    128

void psp_input_init(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void psp_input_poll(psp_input_t *input) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);

    input->prev_buttons = input->buttons;
    input->buttons = pad.Buttons;

    /* Analog stick */
    int32_t ax = (int32_t)pad.Lx - ANALOG_CENTER;
    int32_t ay = (int32_t)pad.Ly - ANALOG_CENTER;

    /* Apply deadzone */
    if (klabs(ax) < ANALOG_DEADZONE) ax = 0;
    if (klabs(ay) < ANALOG_DEADZONE) ay = 0;

    /* Analog stick → look (turn left/right, look up/down) */
    input->look_lr = ax;
    input->look_ud = ay;

    /* D-Pad → movement */
    input->move_fb = 0;
    input->move_lr = 0;

    if (pad.Buttons & PSP_CTRL_UP)      input->move_fb =  127;
    if (pad.Buttons & PSP_CTRL_DOWN)    input->move_fb = -127;
    if (pad.Buttons & PSP_CTRL_LEFT)    input->move_lr = -127;
    if (pad.Buttons & PSP_CTRL_RIGHT)   input->move_lr =  127;

    /* Face buttons */
    input->fire        = (pad.Buttons & PSP_CTRL_CROSS)    ? 1 : 0;
    input->use         = (pad.Buttons & PSP_CTRL_SQUARE)   ? 1 : 0;
    input->jump        = (pad.Buttons & PSP_CTRL_TRIANGLE) ? 1 : 0;
    input->crouch      = (pad.Buttons & PSP_CTRL_CIRCLE)   ? 1 : 0;

    /* Triggers */
    input->prev_weapon = psp_input_pressed(input, PSP_CTRL_LTRIGGER);
    input->next_weapon = psp_input_pressed(input, PSP_CTRL_RTRIGGER);
    input->run         = (pad.Buttons & PSP_CTRL_LTRIGGER) &&
                         (pad.Buttons & PSP_CTRL_RTRIGGER);

    /* Start/Select */
    input->menu        = psp_input_pressed(input, PSP_CTRL_START);
    input->map         = psp_input_pressed(input, PSP_CTRL_SELECT);
}

int psp_input_pressed(psp_input_t *input, uint32_t btn) {
    return (input->buttons & btn) && !(input->prev_buttons & btn);
}

int psp_input_held(psp_input_t *input, uint32_t btn) {
    return (input->buttons & btn) ? 1 : 0;
}
