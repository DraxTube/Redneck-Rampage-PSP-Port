/*
 * psp_audio.h - PSP audio API
 * Redneck Rampage PSP Port
 */

#ifndef PSP_AUDIO_H
#define PSP_AUDIO_H

#include "compat.h"

#define AUDIO_MAX_CHANNELS  16
#define AUDIO_SAMPLE_RATE   22050
#define AUDIO_BUFFER_SIZE   1024

/* Sound effect handle */
typedef struct {
    int16_t  *data;        /* PCM sample data (16-bit signed) */
    int32_t   length;      /* Number of samples */
    int32_t   rate;        /* Sample rate */
    uint8_t   bits;        /* Bits per sample (8 or 16) */
    uint8_t   channels;    /* Mono=1 Stereo=2 */
} sound_t;

/* Initialize audio system */
void psp_audio_init(void);

/* Shutdown audio */
void psp_audio_shutdown(void);

/* Load a sound effect from raw data (VOC or RAW PCM) */
int psp_audio_load_sound(const uint8_t *data, int32_t size, sound_t *snd);

/* Play a sound effect, returns channel number or -1 */
int psp_audio_play(sound_t *snd, int volume, int pan);

/* Stop a channel */
void psp_audio_stop(int channel);

/* Stop all channels */
void psp_audio_stop_all(void);

/* Set master volume (0-255) */
void psp_audio_set_volume(int vol);

/* Update audio (call each frame, handles mixing) */
void psp_audio_update(void);

#endif /* PSP_AUDIO_H */
