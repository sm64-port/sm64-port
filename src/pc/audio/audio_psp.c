#if defined(TARGET_PSP)

#include "audio_psp.h"

#include <pspaudio.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psputils.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "audio_api.h"
#include "../psp_me.h"

#define PSP_AUDIO_CHANNELS 2
#define PSP_AUDIO_FREQUENCY 32000
#define PSP_AUDIO_OUTPUT_FRAMES 512
#define PSP_AUDIO_RING_FRAMES 8192
#define PSP_AUDIO_RING_MASK (PSP_AUDIO_RING_FRAMES - 1)
#define PSP_AUDIO_STARTUP_FRAMES 2048
#define PSP_AUDIO_TARGET_FRAMES 3072
#define PSP_AUDIO_OUTPUT_THREAD_PRIORITY 0x1E
#define PSP_AUDIO_PRODUCER_THREAD_PRIORITY 0x20

#ifdef VERSION_EU
#define PSP_AUDIO_SAMPLES_HIGH 656
#define PSP_AUDIO_SAMPLES_LOW 640
#else
#define PSP_AUDIO_SAMPLES_HIGH 544
#define PSP_AUDIO_SAMPLES_LOW 528
#endif

#if (PSP_AUDIO_RING_FRAMES & (PSP_AUDIO_RING_FRAMES - 1)) != 0
#error PSP_AUDIO_RING_FRAMES must be a power of two
#endif

extern void create_next_audio_buffer(int16_t *samples, uint32_t num_samples);
extern void memcpy_vfpu(void *dst, const void *src, size_t size);
extern int gProcessAudio;

static int16_t sAudioRing[PSP_AUDIO_RING_FRAMES * PSP_AUDIO_CHANNELS]
    __attribute__((aligned(64)));
static int16_t sAudioGenerate[PSP_AUDIO_SAMPLES_HIGH * PSP_AUDIO_CHANNELS]
    __attribute__((aligned(64)));
static int16_t sAudioOutput[2][PSP_AUDIO_OUTPUT_FRAMES * PSP_AUDIO_CHANNELS]
    __attribute__((aligned(64)));

static volatile uint32_t sAudioReadPos;
static volatile uint32_t sAudioWritePos;
static volatile bool sAudioRunning;
static volatile bool sAudioStarted;
static SceUID sAudioOutputThreadId = -1;
static SceUID sAudioProducerThreadId = -1;
static bool sAudioChannelReady;

static inline void audio_psp_sync(void) {
    __asm__ volatile("sync" ::: "memory");
}

static uint32_t audio_psp_buffered_frames(void) {
    return (sAudioWritePos - sAudioReadPos) & PSP_AUDIO_RING_MASK;
}

static uint32_t audio_psp_free_frames(void) {
    return (PSP_AUDIO_RING_FRAMES - 1) - audio_psp_buffered_frames();
}

static void audio_psp_queue_frames(const int16_t *samples, uint32_t frames) {
    uint32_t write_pos = sAudioWritePos;
    uint32_t copied = 0;

    if (frames > audio_psp_free_frames()) {
        return;
    }

    while (copied < frames) {
        uint32_t todo = frames - copied;
        uint32_t until_wrap = PSP_AUDIO_RING_FRAMES - write_pos;
        size_t bytes;

        if (todo > until_wrap) {
            todo = until_wrap;
        }
        bytes = todo * PSP_AUDIO_CHANNELS * sizeof(int16_t);
        memcpy_vfpu(&sAudioRing[write_pos * PSP_AUDIO_CHANNELS],
                    samples + (copied * PSP_AUDIO_CHANNELS), bytes);
        copied += todo;
        write_pos = (write_pos + todo) & PSP_AUDIO_RING_MASK;
    }

    audio_psp_sync();
    sAudioWritePos = write_pos;
}

static void audio_psp_dequeue_output(int16_t *output) {
    uint32_t read_pos = sAudioReadPos;
    uint32_t copied = 0;

    while (copied < PSP_AUDIO_OUTPUT_FRAMES) {
        uint32_t todo = PSP_AUDIO_OUTPUT_FRAMES - copied;
        uint32_t until_wrap = PSP_AUDIO_RING_FRAMES - read_pos;
        size_t bytes;

        if (todo > until_wrap) {
            todo = until_wrap;
        }
        bytes = todo * PSP_AUDIO_CHANNELS * sizeof(int16_t);
        memcpy_vfpu(output + (copied * PSP_AUDIO_CHANNELS),
                    &sAudioRing[read_pos * PSP_AUDIO_CHANNELS], bytes);
        copied += todo;
        read_pos = (read_pos + todo) & PSP_AUDIO_RING_MASK;
    }

    audio_psp_sync();
    sAudioReadPos = read_pos;
}

void audio_psp_generate_cpu(int16_t *samples, uint32_t frames) {
    create_next_audio_buffer(samples, frames);
}

static int audio_psp_output_thread(SceSize args, void *argp) {
    unsigned int output_index = 0;

    (void)args;
    (void)argp;

    while (sAudioRunning) {
        int16_t *output = sAudioOutput[output_index];

        if (!sAudioStarted && (audio_psp_buffered_frames() >= PSP_AUDIO_STARTUP_FRAMES)) {
            sAudioStarted = true;
        }

        if (sAudioStarted && (audio_psp_buffered_frames() >= PSP_AUDIO_OUTPUT_FRAMES)) {
            audio_psp_dequeue_output(output);
        } else {
            if (sAudioStarted) {
                sAudioStarted = false;
            }
            memset(output, 0, sizeof(sAudioOutput[0]));
        }

        sceKernelDcacheWritebackRange(output, sizeof(sAudioOutput[0]));
        if (sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, output) < 0) {
            sceKernelDelayThread(PSP_AUDIO_OUTPUT_FRAMES * 1000000U / PSP_AUDIO_FREQUENCY);
        }
        output_index ^= 1;
    }

    sAudioOutputThreadId = -1;
    sceKernelExitDeleteThread(0);
    return 0;
}

static int audio_psp_producer_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (sAudioRunning) {
        uint32_t buffered = audio_psp_buffered_frames();

        if (gProcessAudio && (buffered < PSP_AUDIO_TARGET_FRAMES) &&
            (audio_psp_free_frames() >= PSP_AUDIO_SAMPLES_HIGH)) {
            uint32_t frames = (buffered < PSP_AUDIO_STARTUP_FRAMES)
                                  ? PSP_AUDIO_SAMPLES_HIGH
                                  : PSP_AUDIO_SAMPLES_LOW;

            if (!psp_me_generate_audio(sAudioGenerate, frames)) {
                audio_psp_generate_cpu(sAudioGenerate, frames);
            }
            audio_psp_queue_frames(sAudioGenerate, frames);
            sceKernelRotateThreadReadyQueue(sceKernelGetThreadCurrentPriority());
        } else {
            sceKernelDelayThread(500);
        }
    }

    sAudioProducerThreadId = -1;
    sceKernelExitDeleteThread(0);
    return 0;
}

static bool audio_psp_init(void) {
    if (sAudioChannelReady) {
        return true;
    }

    sceAudioOutput2Release();
    sceAudioSRCChRelease();
    if (sceAudioSRCChReserve(PSP_AUDIO_OUTPUT_FRAMES, PSP_AUDIO_FREQUENCY,
                             PSP_AUDIO_CHANNELS) < 0) {
        return false;
    }

    memset(sAudioRing, 0, sizeof(sAudioRing));
    memset(sAudioGenerate, 0, sizeof(sAudioGenerate));
    memset(sAudioOutput, 0, sizeof(sAudioOutput));
    sAudioReadPos = 0;
    sAudioWritePos = 0;
    sAudioRunning = false;
    sAudioStarted = false;
    sAudioChannelReady = true;
    return true;
}

bool audio_psp_start(void) {
    SceUID thread_id;
    int result;

    if (!sAudioChannelReady) {
        return false;
    }
    if (sAudioRunning) {
        return true;
    }

    (void)psp_me_init();
    sAudioRunning = true;

    thread_id = sceKernelCreateThread("SM64 PSP AudioOut", audio_psp_output_thread,
                                     PSP_AUDIO_OUTPUT_THREAD_PRIORITY, 0x10000,
                                     PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL);
    if (thread_id < 0) {
        sAudioRunning = false;
        return false;
    }
    result = sceKernelStartThread(thread_id, 0, NULL);
    if (result < 0) {
        sceKernelDeleteThread(thread_id);
        sAudioRunning = false;
        return false;
    }
    sAudioOutputThreadId = thread_id;

    thread_id = sceKernelCreateThread("SM64 PSP AudioGen", audio_psp_producer_thread,
                                     PSP_AUDIO_PRODUCER_THREAD_PRIORITY, 0x20000,
                                     PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL);
    if (thread_id < 0) {
        audio_psp_shutdown();
        return false;
    }
    result = sceKernelStartThread(thread_id, 0, NULL);
    if (result < 0) {
        sceKernelDeleteThread(thread_id);
        audio_psp_shutdown();
        return false;
    }
    sAudioProducerThreadId = thread_id;
    return true;
}

void audio_psp_shutdown(void) {
    SceUID output_thread = sAudioOutputThreadId;
    SceUID producer_thread = sAudioProducerThreadId;

    sAudioRunning = false;
    audio_psp_sync();

    /* Releasing SRC wakes a blocking output call so the thread can exit. */
    if (sAudioChannelReady) {
        sceAudioSRCChRelease();
        sAudioChannelReady = false;
    }
    if (producer_thread >= 0) {
        sceKernelWaitThreadEnd(producer_thread, NULL);
    }
    if (output_thread >= 0) {
        sceKernelWaitThreadEnd(output_thread, NULL);
    }

    sAudioOutputThreadId = -1;
    sAudioProducerThreadId = -1;
    psp_me_shutdown();
}

static int audio_psp_buffered(void) {
    return (int)audio_psp_buffered_frames();
}

static int audio_psp_get_desired_buffered(void) {
    return PSP_AUDIO_TARGET_FRAMES;
}

static void audio_psp_play(const uint8_t *buf, size_t len) {
    uint32_t frames = (uint32_t)(len / (sizeof(int16_t) * PSP_AUDIO_CHANNELS));

    if ((buf != NULL) && (frames != 0)) {
        audio_psp_queue_frames((const int16_t *)buf, frames);
    }
}

struct AudioAPI audio_psp = {
    audio_psp_init,
    audio_psp_buffered,
    audio_psp_get_desired_buffered,
    audio_psp_play,
};

#endif
