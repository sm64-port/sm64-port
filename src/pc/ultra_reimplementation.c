#include <stdio.h>
#include <string.h>
#include "lib/src/libultra_internal.h"
#include "macros.h"
#include "fs_utils.h"

#if defined(TARGET_PSP)
#include <pspiofilemgr.h>
#endif

#ifdef TARGET_WEB
#include <emscripten.h>
#endif

#define FILE_PREFIX ""

extern OSMgrArgs piMgrArgs;

u64 osClockRate = 62500000;

s32 osPiStartDma(UNUSED OSIoMesg *mb, UNUSED s32 priority, UNUSED s32 direction,
                 uintptr_t devAddr, void *vAddr, size_t nbytes,
                 UNUSED OSMesgQueue *mq) {
    memcpy(vAddr, (const void *) devAddr, nbytes);
    return 0;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}

void osSetEventMesg(UNUSED OSEvent e, UNUSED OSMesgQueue *mq, UNUSED OSMesg msg) {
}
s32 osJamMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}
s32 osSendMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
#if defined(VERSION_EU) || defined(VERSION_SH)
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }
    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;
#endif
    return 0;
}
s32 osRecvMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg *msg, UNUSED s32 flag) {
#if defined(VERSION_EU) || defined(VERSION_SH)
    if (mq->validCount == 0) {
        return -1;
    }
    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
#endif
    return 0;
}

uintptr_t osVirtualToPhysical(void *addr) {
    return (uintptr_t) addr;
}

void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode *mode) {
}
void osViSetEvent(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViBlack(UNUSED u8 active) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void *vaddr) {
}

#include <psprtc.h>
OSTime osGetTime(void) {
    long long unsigned int temp;
    sceRtcGetCurrentTick(&temp);
    return (unsigned int) ((temp) & 0xffffffff);
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void *a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void *a, UNUSED size_t b) {
}

u32 osGetCount(void) {
    static u32 counter;
    return counter++;
}

s32 osAiSetFrequency(u32 freq) {
    u32 a1;
    s32 a2;
    u32 D_8033491C;

#ifdef VERSION_EU
    D_8033491C = 0x02E6025C;
#else
    D_8033491C = 0x02E6D354;
#endif

    a1 = D_8033491C / (float) freq + .5f;

    if (a1 < 0x84) {
        return -1;
    }

    a2 = (a1 / 66) & 0xff;
    if (a2 > 16) {
        a2 = 16;
    }

    return D_8033491C / (s32) a1;
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return 1;
}

s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    u8 content[512];
    s32 ret = -1;

#ifdef TARGET_WEB
    if (EM_ASM_INT({
        var s = localStorage.sm64_save_file;
        if (s && s.length === 684) {
            try {
                var binary = atob(s);
                if (binary.length === 512) {
                    for (var i = 0; i < 512; i++) {
                        HEAPU8[$0 + i] = binary.charCodeAt(i);
                    }
                    return 1;
                }
            } catch (e) {
            }
        }
        return 0;
    }, content)) {
        memcpy(buffer, content + address * 8, nbytes);
        ret = 0;
    }
#else
    char path[FS_MAX_PATH];
#if !defined(TARGET_PSP)
    FILE *fp;
#endif

    fs_resolve_path(path, sizeof(path), FILE_PREFIX"sm64_save_file.bin");
    printf("Opening save for read: '%s'\n", path);
#if defined(TARGET_PSP)
    {
        SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
        if (fd < 0) {
            printf("Failed to open save for read.\n");
            return -1;
        }
        if (sceIoRead(fd, content, sizeof(content)) == (int) sizeof(content)) {
            memcpy(buffer, content + address * 8, nbytes);
            ret = 0;
        }
        sceIoClose(fd);
    }
#else
    fp = fopen(path, "rb");
    if (fp == NULL && strcmp(path, FILE_PREFIX"sm64_save_file.bin") != 0) {
        printf("Opening save for read (legacy fallback): '%s'\n", FILE_PREFIX"sm64_save_file.bin");
        fp = fopen(FILE_PREFIX"sm64_save_file.bin", "rb");
    }
    if (fp == NULL) {
        printf("Failed to open save for read.\n");
        return -1;
    }
    if (fread(content, 1, 512, fp) == 512) {
        memcpy(buffer, content + address * 8, nbytes);
        ret = 0;
    }
    fclose(fp);
#endif
#endif
    return ret;
}

s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    u8 content[512] = {0};
    s32 ret;
    if (address != 0 || nbytes != 512) {
        osEepromLongRead(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, nbytes);

#ifdef TARGET_WEB
    EM_ASM({
        var str = "";
        for (var i = 0; i < 512; i++) {
            str += String.fromCharCode(HEAPU8[$0 + i]);
        }
        localStorage.sm64_save_file = btoa(str);
    }, content);
    s32 ret = 0;
#else
    char path[FS_MAX_PATH];
#if !defined(TARGET_PSP)
    FILE *fp;
#endif

    fs_resolve_path(path, sizeof(path), FILE_PREFIX"sm64_save_file.bin");
    fs_prepare_parent_path(FILE_PREFIX"sm64_save_file.bin");
    printf("Opening save for write: '%s'\n", path);
#if defined(TARGET_PSP)
    {
        SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd < 0) {
            printf("Failed to open save for write.\n");
            return -1;
        }
        ret = sceIoWrite(fd, content, sizeof(content)) == (int) sizeof(content) ? 0 : -1;
        sceIoClose(fd);
    }
#else
    fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("Failed to open save for write.\n");
        return -1;
    }
    ret = fwrite(content, 1, 512, fp) == 512 ? 0 : -1;
    fclose(fp);
#endif
#endif
    return ret;
}

s32 gNumVblanks;

s32 osMotorInit(UNUSED OSMesgQueue *mq, UNUSED void *pfs, UNUSED int channel) {
    return 0;
}

s32 osMotorStart(UNUSED void *pfs) {
    return 0;
}

s32 osMotorStop(UNUSED void *pfs) {
    return 0;
}

OSPiHandle *osCartRomInit(void) {
    static OSPiHandle handle;
    return &handle;
}

OSPiHandle *osDriveRomInit(void) {
    static OSPiHandle handle;
    return &handle;
}

s32 osEPiStartDma(UNUSED OSPiHandle *pihandle, OSIoMesg *mb, UNUSED s32 direction) {
    memcpy(mb->dramAddr, (const void *) mb->devAddr, mb->size);
    osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
}
