#if defined(TARGET_PSP)

#include "psp_me.h"

#include <me-core-mapper/me-core.h>
#include <pspkernel.h>
#include <psputils.h>
#include <stdint.h>

#include "audio/audio_psp.h"

#define PSP_ME_READY_TIMEOUT_US 250000
#define PSP_ME_POLL_US 100
#define PSP_ME_PROGRESS_ENTERED 0x10
#define PSP_ME_PROGRESS_BOOT_RELEASED 0x20
#define PSP_ME_PROGRESS_READY 0x100

enum PspMeState {
    PSP_ME_STATE_STOPPED,
    PSP_ME_STATE_BOOTING,
    PSP_ME_STATE_IDLE,
    PSP_ME_STATE_GENERATE_AUDIO,
    PSP_ME_STATE_STOP,
    PSP_ME_STATE_HALTED,
    PSP_ME_STATE_FAULT,
};

enum PspMeSharedIndex {
    PSP_ME_SHARED_STATE,
    PSP_ME_SHARED_PROGRESS,
    PSP_ME_SHARED_AUDIO_OUTPUT,
    PSP_ME_SHARED_AUDIO_FRAMES,
    PSP_ME_SHARED_COUNT,
};

static volatile uint32_t sPspMeSharedStorage[PSP_ME_SHARED_COUNT]
    __attribute__((aligned(64), section(".uncached")));

#define sPspMeShared                                                                  \
    ((volatile uint32_t *)(UNCACHED_USER_MASK | (uintptr_t)sPspMeSharedStorage))
#define sPspMeState sPspMeShared[PSP_ME_SHARED_STATE]
#define sPspMeProgress sPspMeShared[PSP_ME_SHARED_PROGRESS]
#define sPspMeAudioOutput sPspMeShared[PSP_ME_SHARED_AUDIO_OUTPUT]
#define sPspMeAudioFrames sPspMeShared[PSP_ME_SHARED_AUDIO_FRAMES]

static bool sPspMeBootStarted;
static int sPspMeBootResult = -1;
static bool sPspMeInitialized;

__attribute__((noinline, aligned(4))) void meLibOnException(void) {
    sPspMeState = PSP_ME_STATE_FAULT;
    meLibSync();
    meLibHalt();
}

__attribute__((noinline, aligned(4))) void meLibOnExternalInterrupt(void) {
    sPspMeState = PSP_ME_STATE_FAULT;
    meLibSync();
    meLibHalt();
}

__attribute__((noinline, aligned(4))) void meLibOnProcess(void) {
    sPspMeProgress = PSP_ME_PROGRESS_ENTERED;
    meLibSync();

    do {
        meLibDelayPipeline();
    } while (sPspMeState == PSP_ME_STATE_BOOTING);

    sPspMeProgress = PSP_ME_PROGRESS_BOOT_RELEASED;
    meLibSync();
    sPspMeProgress = PSP_ME_PROGRESS_READY;
    meLibSync();

    while (sPspMeState != PSP_ME_STATE_STOP) {
        if (sPspMeState == PSP_ME_STATE_GENERATE_AUDIO) {
            int16_t *output = (int16_t *)(uintptr_t)sPspMeAudioOutput;
            uint32_t frames = sPspMeAudioFrames;

            /* The SM64 audio engine still builds and executes its ABI work as
             * one unit. Keep its mutable globals coherent until the mixer is
             * split into the command-list model used by the OOT port. */
            meCoreDcacheWritebackInvalidateAll();
            audio_psp_generate_cpu(output, frames);
            meCoreDcacheWritebackInvalidateAll();
            meLibSync();
            sPspMeState = PSP_ME_STATE_IDLE;
            meLibSync();
        } else {
            meLibDelayPipeline();
        }
    }

    sPspMeState = PSP_ME_STATE_HALTED;
    meLibSync();
    meLibHalt();
}

int psp_me_boot(void) {
    if (sPspMeBootStarted) {
        return sPspMeBootResult;
    }

    sPspMeState = PSP_ME_STATE_BOOTING;
    sPspMeProgress = 0;
    sPspMeAudioOutput = 0;
    sPspMeAudioFrames = 0;
    meLibSync();

    sPspMeBootStarted = true;
    sPspMeBootResult = meLibDefaultInit();
    return sPspMeBootResult;
}

bool psp_me_init(void) {
    uint32_t start;

    if (sPspMeInitialized) {
        return true;
    }
    if (psp_me_boot() < 0) {
        return false;
    }

    meLibSync();
    sPspMeState = PSP_ME_STATE_IDLE;
    meLibSync();

    start = sceKernelGetSystemTimeLow();
    while (sPspMeProgress != PSP_ME_PROGRESS_READY) {
        if ((sceKernelGetSystemTimeLow() - start) >= PSP_ME_READY_TIMEOUT_US) {
            sPspMeState = PSP_ME_STATE_STOP;
            meLibSync();
            return false;
        }
        sceKernelDelayThread(PSP_ME_POLL_US);
    }

    sPspMeInitialized = true;
    return true;
}

bool psp_me_is_active(void) {
    return sPspMeInitialized;
}

bool psp_me_generate_audio(int16_t *samples, uint32_t frames) {
    if (!sPspMeInitialized || (sPspMeState != PSP_ME_STATE_IDLE) ||
        (samples == NULL) || (frames == 0)) {
        return false;
    }

    /* Publish every Allegrex-owned audio input before releasing the ME. */
    sceKernelDcacheWritebackInvalidateAll();
    sPspMeAudioOutput = (uint32_t)(uintptr_t)samples;
    sPspMeAudioFrames = frames;
    meLibSync();
    sPspMeState = PSP_ME_STATE_GENERATE_AUDIO;
    meLibSync();

    while (sPspMeState == PSP_ME_STATE_GENERATE_AUDIO) {
        meLibSync();
        sceKernelDelayThread(PSP_ME_POLL_US);
    }

    if (sPspMeState != PSP_ME_STATE_IDLE) {
        sPspMeInitialized = false;
        return false;
    }

    /* Discard stale Allegrex cache lines after the ME published audio state
     * and PCM. No Allegrex audio work runs in this producer while it waits. */
    sceKernelDcacheWritebackInvalidateAll();
    return true;
}

void psp_me_shutdown(void) {
    uint32_t start;

    if (!sPspMeBootStarted || (sPspMeBootResult < 0)) {
        return;
    }
    if (sPspMeState == PSP_ME_STATE_HALTED) {
        sPspMeInitialized = false;
        return;
    }

    sPspMeState = PSP_ME_STATE_STOP;
    meLibSync();
    start = sceKernelGetSystemTimeLow();
    while ((sPspMeState != PSP_ME_STATE_HALTED) &&
           ((sceKernelGetSystemTimeLow() - start) < PSP_ME_READY_TIMEOUT_US)) {
        sceKernelDelayThread(PSP_ME_POLL_US);
    }
    sPspMeInitialized = false;
}

#endif
