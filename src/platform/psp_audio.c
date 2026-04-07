/*
 * psp_audio.c - PSP audio implementation
 * Redneck Rampage PSP Port
 *
 * Software mixer feeding PCM to sceAudio channels.
 * Supports 16 simultaneous sound effects mixed down to stereo.
 */

#include "psp_audio.h"
#include <pspkernel.h>
#include <pspaudio.h>
#include <string.h>

/* Internal channel state */
typedef struct {
    sound_t *snd;
    int32_t  pos;          /* Current sample position */
    int32_t  volume;       /* 0-255 */
    int32_t  pan;          /* 0=left, 128=center, 255=right */
    int      active;
} audio_channel_t;

static audio_channel_t channels[AUDIO_MAX_CHANNELS];
static int master_volume = 200;
static int psp_audio_handle = -1;
static SceUID audio_thread_id = -1;
static volatile int audio_running = 0;

/* Mix buffer */
static int16_t mix_buffer[AUDIO_BUFFER_SIZE * 2]; /* Stereo */

/* Audio callback thread */
static int audio_thread_func(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (audio_running) {
        /* Clear mix buffer */
        memset(mix_buffer, 0, sizeof(mix_buffer));

        /* Mix all active channels */
        for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
            audio_channel_t *c = &channels[ch];
            if (!c->active || !c->snd || !c->snd->data) continue;

            int32_t vol = (c->volume * master_volume) >> 8;
            int32_t pan_l = (255 - c->pan) * vol >> 8;
            int32_t pan_r = c->pan * vol >> 8;
            /* Center pan boost */
            if (c->pan >= 96 && c->pan <= 160) {
                pan_l = vol * 3 / 4;
                pan_r = vol * 3 / 4;
            }

            for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
                if (c->pos >= c->snd->length) {
                    c->active = 0;
                    break;
                }

                int32_t sample = (int32_t)c->snd->data[c->pos];
                c->pos++;

                /* Mix into stereo buffer with panning */
                int32_t left  = mix_buffer[i * 2 + 0] + ((sample * pan_l) >> 8);
                int32_t right = mix_buffer[i * 2 + 1] + ((sample * pan_r) >> 8);

                /* Clamp */
                if (left > 32767) left = 32767;
                if (left < -32768) left = -32768;
                if (right > 32767) right = 32767;
                if (right < -32768) right = -32768;

                mix_buffer[i * 2 + 0] = (int16_t)left;
                mix_buffer[i * 2 + 1] = (int16_t)right;
            }
        }

        /* Output to PSP audio */
        if (psp_audio_handle >= 0) {
            sceAudioOutputBlocking(psp_audio_handle, PSP_AUDIO_VOLUME_MAX, mix_buffer);
        }
    }

    return 0;
}

void psp_audio_init(void) {
    memset(channels, 0, sizeof(channels));

    /* Reserve a hardware audio channel */
    psp_audio_handle = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
                                          AUDIO_BUFFER_SIZE,
                                          PSP_AUDIO_FORMAT_STEREO);

    if (psp_audio_handle < 0) {
        return;
    }

    /* Start audio thread */
    audio_running = 1;
    audio_thread_id = sceKernelCreateThread("audio_thread",
                                             audio_thread_func,
                                             0x12, 0x10000, 0, NULL);
    if (audio_thread_id >= 0) {
        sceKernelStartThread(audio_thread_id, 0, NULL);
    }
}

void psp_audio_shutdown(void) {
    audio_running = 0;

    if (audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL);
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
    }

    if (psp_audio_handle >= 0) {
        sceAudioChRelease(psp_audio_handle);
        psp_audio_handle = -1;
    }
}

/*
 * Parse Creative VOC or raw PCM data.
 * VOC format: "Creative Voice File\x1A" header, then data blocks.
 * For simplicity, we also handle raw unsigned 8-bit PCM.
 */
int psp_audio_load_sound(const uint8_t *data, int32_t size, sound_t *snd) {
    if (!data || size < 16 || !snd) return -1;

    memset(snd, 0, sizeof(sound_t));

    /* Check for VOC signature */
    if (size >= 26 && memcmp(data, "Creative Voice File\x1A", 20) == 0) {
        /* VOC format */
        int header_size = read_le16u(data + 20);
        const uint8_t *p = data + header_size;
        int32_t remaining = size - header_size;

        /* Find first sound data block (type 1) */
        while (remaining > 4) {
            uint8_t block_type = *p;
            if (block_type == 0) break; /* Terminator */

            int32_t block_size = p[1] | (p[2] << 8) | (p[3] << 16);
            p += 4;
            remaining -= 4;

            if (block_type == 1 && block_size >= 2) {
                /* Sound data block */
                uint8_t freq_div = p[0];
                uint8_t codec = p[1]; /* 0 = unsigned 8-bit PCM */

                int sample_rate = 1000000 / (256 - freq_div);
                int num_samples = block_size - 2;

                if (codec == 0 && num_samples > 0) {
                    snd->data = (int16_t *)xmalloc(num_samples * sizeof(int16_t));
                    if (!snd->data) return -1;

                    /* Convert unsigned 8-bit to signed 16-bit */
                    for (int i = 0; i < num_samples; i++) {
                        snd->data[i] = ((int16_t)p[2 + i] - 128) << 8;
                    }

                    snd->length = num_samples;
                    snd->rate = sample_rate;
                    snd->bits = 16;
                    snd->channels = 1;
                    return 0;
                }
            }

            p += block_size;
            remaining -= block_size;
        }
        return -1;
    }

    /* Raw unsigned 8-bit PCM fallback */
    snd->data = (int16_t *)xmalloc(size * sizeof(int16_t));
    if (!snd->data) return -1;

    for (int i = 0; i < size; i++) {
        snd->data[i] = ((int16_t)data[i] - 128) << 8;
    }

    snd->length = size;
    snd->rate = 11025;
    snd->bits = 16;
    snd->channels = 1;
    return 0;
}

int psp_audio_play(sound_t *snd, int volume, int pan) {
    if (!snd || !snd->data) return -1;

    /* Find free channel */
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        if (!channels[ch].active) {
            channels[ch].snd = snd;
            channels[ch].pos = 0;
            channels[ch].volume = kclamp(volume, 0, 255);
            channels[ch].pan = kclamp(pan, 0, 255);
            channels[ch].active = 1;
            return ch;
        }
    }

    /* All channels busy - replace the one closest to finishing */
    int best = 0;
    int32_t best_remaining = 0x7FFFFFFF;
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        int32_t rem = channels[ch].snd ? (channels[ch].snd->length - channels[ch].pos) : 0;
        if (rem < best_remaining) {
            best_remaining = rem;
            best = ch;
        }
    }

    channels[best].snd = snd;
    channels[best].pos = 0;
    channels[best].volume = kclamp(volume, 0, 255);
    channels[best].pan = kclamp(pan, 0, 255);
    channels[best].active = 1;
    return best;
}

void psp_audio_stop(int channel) {
    if (channel >= 0 && channel < AUDIO_MAX_CHANNELS) {
        channels[channel].active = 0;
    }
}

void psp_audio_stop_all(void) {
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        channels[ch].active = 0;
    }
}

void psp_audio_set_volume(int vol) {
    master_volume = kclamp(vol, 0, 255);
}

void psp_audio_update(void) {
    /* Audio mixing happens in audio thread, nothing to do here */
}
