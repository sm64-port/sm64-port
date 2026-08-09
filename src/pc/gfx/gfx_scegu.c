#define TARGET_SCEGU 1
#if defined(TARGET_SCEGU) || defined(TARGET_PSP)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>
#include <intraFont.h>
#include <string.h>

#include "psp_texture_manager.h"
#include "../psp_home_menu.h"

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define FRAMEBUFFER_SIZE (BUF_WIDTH * SCR_HEIGHT * sizeof(uint16_t))

float identity_matrix[4][4] __attribute__((aligned(16))) = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

/* Shader IDs
id        alp fog edg nse ut0 ut1 num sin0 sin1 mul0 mul1 mix0 mix1 cas
-----------------------------------------------------------------------
69        0   0   0   0   1   0   1   0    1    1    1    1    1    0
512       0   0   0   0   0   0   1   1    1    0    1    0    1    0
909       0   0   0   0   1   0   1   0    1    0    1    1    1    0
1361      0   0   0   0   1   0   2   0    1    0    1    1    1    0
2560      0   0   0   0   1   0   0   1    1    0    1    0    1    0
17059909  1   0   0   0   1   0   1   0    0    1    1    1    1    1
17062400  1   0   0   0   1   0   1   1    0    0    1    0    1    0
17305729  1   0   0   0   0   0   2   0    0    1    1    1    1    1
18092101  1   0   0   0   1   0   1   0    0    1    1    1    1    0
18874437  1   0   0   0   1   0   1   0    1    1    0    1    0    0
18874880  1   0   0   0   0   0   1   1    1    0    0    0    0    1
18875277  1   0   0   0   1   0   1   0    1    0    0    1    0    0
18876928  1   0   0   0   1   0   1   1    1    0    0    0    0    0
27263045  1   0   0   0   1   0   1   0    1    1    0    1    0    0
27265536  1   0   0   0   1   0   0   1    1    0    0    0    0    1
27265647  1   0   0   0   1   1   1   0    1    0    0    1    0    0
52428869  1   1   0   0   1   0   1   0    1    1    0    1    0    0
52429312  1   1   0   0   0   0   1   1    1    0    0    0    0    1
52431360  1   1   0   0   1   0   1   1    1    0    0    0    0    0
84168773  1   0   1   0   1   0   1   0    0    1    1    1    1    1
85983744  1   0   1   0   0   0   1   1    1    0    0    0    0    1
94374400  1   0   1   0   1   0   0   1    1    0    0    0    0    1
127928832 1   1   1   0   1   0   0   1    1    0    0    0    0    1
153092165 1   0   0   1   1   0   1   0    1    1    0    1    0    0
153092608 1   0   0   1   0   0   1   1    1    0    0    0    0    1
153093005 1   0   0   1   1   0   1   0    1    0    0    1    0    0
153094656 1   0   0   1   1   0   1   1    1    0    0    0    0    0

printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", shader_id,
    cc_features.opt_alpha,
    cc_features.opt_fog,
    cc_features.opt_texture_edge,
    cc_features.opt_noise,
    cc_features.used_textures[0],
    cc_features.used_textures[1],
    cc_features.num_inputs,
    cc_features.do_single[0],
    cc_features.do_single[1],
    cc_features.do_multiply[0],
    cc_features.do_multiply[1],
    cc_features.do_mix[0],
    cc_features.do_mix[1],
    cc_features.color_alpha_same
);
*/

/* Shader Working List:
84168773    - Menu Overlays
*/

/* Shader Broken List:
153092165   - Noise
153092608   - Noise
153093005   - Noise
153094656   - Noise
*/

// clang-format off
static uint32_t shader_ids[27] =
{
69       ,
512      ,
909      ,
1361     ,
2560     ,
17059909 ,
17062400 ,
17305729 ,
18092101 ,
18874437 ,
18874880 ,
18875277 ,
18876928 ,
27263045 ,
27265536 ,
27265647 ,
52428869 ,
52429312 ,
52431360 ,
84168773 ,
85983744 ,
94374400 ,
127928832,
153092165,
153092608,
153093005,
153094656
};

static uint32_t shader_remap[27*2] = {
69       ,69       ,
512      ,512      ,
909      ,909      ,
1361     ,1361     ,
2560     ,2560     ,
17059909 ,17059909 ,
17062400 ,17062400 ,
17305729 ,17305729 ,
18092101 ,18092101 ,
18874437 ,18874437 ,
18874880 ,18874880 ,
18875277 ,18875277 ,
18876928 ,18876928 ,
27263045 ,27263045 ,
27265536 ,27265536 ,
27265647 ,27265647 ,
52428869 ,52428869 ,
52429312 ,52429312 ,
52431360 ,52431360 ,
84168773 ,84168773 ,
85983744 ,85983744 ,
94374400 ,94374400 ,
127928832,127928832,
153092165,69,
153092608,69,
153093005,69,
153094656,69
};
// clang-format on

static uint32_t shader_broken[27] = {
    153092165, // Noise
    153092608, // Noise
    153093005, // Noise
    153094656  // Noise
};

unsigned int __attribute__((aligned(64))) list[262144 * 2];

/* sceGuGetMemory stores transient vertices in the display list. Menu text can
 * emit many small sprites, so continue in a fresh list before an allocation
 * can overrun the fixed command buffer. GE state and buffers survive this. */
#define GU_LIST_COMMAND_RESERVE 4096

static void gfx_scegu_reserve_list_memory(size_t data_size) {
    const size_t allocation_size = (data_size + 3) & ~(size_t)3;
    const size_t required_size = allocation_size + 8 + GU_LIST_COMMAND_RESERVE;

    if (((size_t)sceGuCheckList() + required_size) <= sizeof(list)) {
        return;
    }

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuStart(GU_DIRECT, list);
}

static unsigned int staticOffset = 0;
unsigned int scegu_fog_color = 0;

static unsigned int getMemorySize(unsigned int width, unsigned int height, unsigned int psm) {
    switch (psm) {
        case GU_PSM_T4:
            return (width * height) >> 1;

        case GU_PSM_T8:
            return width * height;

        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return 2 * width * height;

        case GU_PSM_8888:
        case GU_PSM_T32:
            return 4 * width * height;

        default:
            return 0;
    }
}

#define TEX_ALIGNMENT (16)
void *getStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memSize = getMemorySize(width, height, psm);
    void *result = (void *) (staticOffset | 0x40000000);
    staticOffset += memSize;

    return result;
}

void *getStaticVramBufferBytes(size_t bytes) {
    unsigned int memSize = bytes;
    void *result = (void *) (staticOffset | 0x40000000);
    staticOffset += memSize;

    return (void *) (((unsigned int) result) + ((unsigned int) sceGeEdramGetAddr()));
}

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "macros.h"

enum MixType {
    SH_MT_NONE,
    SH_MT_TEXTURE,
    SH_MT_COLOR,
    SH_MT_TEXTURE_TEXTURE,
    SH_MT_TEXTURE_COLOR,
    SH_MT_COLOR_COLOR,
};

struct ShaderProgram {
    bool enabled;
    uint32_t shader_id;
    struct CCFeatures cc;
    enum MixType mix;
    bool texture_used[2];
    int texture_ord[2];
    int num_inputs;
};

struct SamplerState {
    int min_filter;
    int mag_filter;
    int wrap_s;
    int wrap_t;
    uint32_t tex;
};

typedef struct Vertex {
    float u, v;
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct VertexColor {
    unsigned short a, b;
    unsigned long color;
    unsigned short x, y, z;
} VertexColor;

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static struct ShaderProgram *cur_shader = NULL;
static struct SamplerState tmu_state[2];
static bool gl_blend = false;
static void *sDrawBuffer;
static void *sDisplayBuffer;
static bool sHomeMenuBgActive;
static bool sHomeMenuBgCaptureRequested;
static bool sHomeMenuBgCaptured;
static uint16_t sHomeMenuBgBuffer[BUF_WIDTH * SCR_HEIGHT] __attribute__((aligned(64)));
static uint16_t sHomeMenuBgBlurScratch[BUF_WIDTH * SCR_HEIGHT] __attribute__((aligned(64)));
static intraFont *sHomeMenuFont;
static bool sHomeMenuFontInitTried;

extern void memcpy_vfpu(void *dst, const void *src, size_t size);

static void *gfx_scegu_vram_cpu_addr(const void *vram_buffer) {
    return (void *)(((uintptr_t)sceGeEdramGetAddr() | 0x40000000U) +
                    ((uintptr_t)vram_buffer & 0x00FFFFFFU));
}

static void gfx_scegu_copy_framebuffer_from_vram(uint16_t *dst, const void *src) {
    memcpy_vfpu(dst, gfx_scegu_vram_cpu_addr(src), FRAMEBUFFER_SIZE);
}

static void gfx_scegu_copy_framebuffer_to_vram(void *dst, const uint16_t *src) {
    void *dst_addr = gfx_scegu_vram_cpu_addr(dst);

    memcpy_vfpu(dst_addr, src, FRAMEBUFFER_SIZE);
    sceKernelDcacheWritebackRange(dst_addr, FRAMEBUFFER_SIZE);
}

static uint16_t gfx_scegu_make_rgb565(unsigned int r, unsigned int g, unsigned int b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void gfx_scegu_accum_rgb565(uint16_t color, unsigned int *r, unsigned int *g,
                                   unsigned int *b) {
    *r += ((color >> 11) & 0x1F) << 3;
    *g += ((color >> 5) & 0x3F) << 2;
    *b += (color & 0x1F) << 3;
}

static int gfx_scegu_clamp_int(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void gfx_scegu_blur_framebuffer_565(uint16_t *pixels) {
    static const int offsets[3] = { -3, 0, 3 };
    int x;
    int y;
    int ox;
    int oy;

    memcpy_vfpu(sHomeMenuBgBlurScratch, pixels, FRAMEBUFFER_SIZE);
    for (y = 0; y < SCR_HEIGHT; y++) {
        for (x = 0; x < SCR_WIDTH; x++) {
            unsigned int r = 0;
            unsigned int g = 0;
            unsigned int b = 0;

            for (oy = 0; oy < 3; oy++) {
                int sample_y = gfx_scegu_clamp_int(y + offsets[oy], 0, SCR_HEIGHT - 1);

                for (ox = 0; ox < 3; ox++) {
                    int sample_x = gfx_scegu_clamp_int(x + offsets[ox], 0, SCR_WIDTH - 1);
                    gfx_scegu_accum_rgb565(pixels[(sample_y * BUF_WIDTH) + sample_x], &r, &g, &b);
                }
            }

            sHomeMenuBgBlurScratch[(y * BUF_WIDTH) + x] =
                gfx_scegu_make_rgb565(r / 9, g / 9, b / 9);
        }
    }

    memcpy_vfpu(pixels, sHomeMenuBgBlurScratch, FRAMEBUFFER_SIZE);
    sceKernelDcacheWritebackRange(pixels, FRAMEBUFFER_SIZE);
}

static void gfx_scegu_apply_home_menu_2d_view(void) {
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2), SCR_WIDTH, SCR_HEIGHT);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuSetMatrix(GU_VIEW, (const ScePspFMatrix4 *)identity_matrix);
    sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)identity_matrix);
}

static unsigned int gfx_scegu_rgba(unsigned int r, unsigned int g, unsigned int b,
                                   unsigned int a) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static void gfx_scegu_draw_rect(int x, int y, int width, int height, unsigned int color) {
    VertexColor *verts;

    if ((width <= 0) || (height <= 0)) {
        return;
    }

    gfx_scegu_reserve_list_memory(sizeof(VertexColor) * 2);
    verts = (VertexColor *)sceGuGetMemory(sizeof(VertexColor) * 2);
    if (verts == NULL) {
        return;
    }

    verts[0].a = 0;
    verts[0].b = 0;
    verts[0].color = color;
    verts[0].x = (unsigned short)x;
    verts[0].y = (unsigned short)y;
    verts[0].z = 0;
    verts[1].a = 0;
    verts[1].b = 0;
    verts[1].color = color;
    verts[1].x = (unsigned short)(x + width);
    verts[1].y = (unsigned short)(y + height);
    verts[1].z = 0;

    gfx_scegu_apply_home_menu_2d_view();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT |
                                   GU_TRANSFORM_2D,
                   2, 0, verts);
}

static bool gfx_scegu_ensure_home_menu_font(void) {
    if (!sHomeMenuFontInitTried) {
        sHomeMenuFontInitTried = true;
        if (intraFontInit()) {
            sHomeMenuFont = intraFontLoad("flash0:/font/ltn0.pgf", INTRAFONT_CACHE_ASCII);
        }
    }
    return sHomeMenuFont != NULL;
}

static void gfx_scegu_draw_home_menu_text(int x, int y, const char *text, float size,
                                          unsigned int color, unsigned int options) {
    if (!gfx_scegu_ensure_home_menu_font()) {
        return;
    }

    gfx_scegu_apply_home_menu_2d_view();
    intraFontActivate(sHomeMenuFont);
    intraFontSetStyle(sHomeMenuFont, size, color, gfx_scegu_rgba(0, 0, 0, 180), 0.0f,
                      options | INTRAFONT_STRING_ASCII);
    intraFontPrint(sHomeMenuFont, (float)x, (float)y, text);
}

static void gfx_scegu_prepare_home_menu_draw(void) {
    gfx_scegu_apply_home_menu_2d_view();
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexEnvColor(0xffffffff);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_FALSE);
}

static void gfx_scegu_render_home_menu_main(int selected_index, uint8_t highlight_red,
                                            uint8_t highlight_green, uint8_t highlight_blue) {
    static const char *items[] = {
        "Resume Game",
        "Controller Mapping",
        "Exit Game",
    };
    int i;

    gfx_scegu_draw_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, gfx_scegu_rgba(0, 0, 0, 96));
    gfx_scegu_draw_rect(102, 44, 276, 184, gfx_scegu_rgba(0, 0, 0, 150));
    gfx_scegu_draw_home_menu_text(SCR_WIDTH / 2, 78, "Super Mario 64", 0.82f,
                                  gfx_scegu_rgba(255, 255, 245, 255), INTRAFONT_ALIGN_CENTER);

    for (i = 0; i < 3; i++) {
        int y = 122 + (i * 38);
        unsigned int color = gfx_scegu_rgba(218, 224, 218, 255);

        if (selected_index == i) {
            gfx_scegu_draw_rect(124, y - 22, 232, 28,
                                gfx_scegu_rgba(highlight_red, highlight_green,
                                               highlight_blue, 205));
            color = gfx_scegu_rgba(255, 255, 245, 255);
        }
        gfx_scegu_draw_home_menu_text(SCR_WIDTH / 2, y, items[i], 0.72f, color,
                                      INTRAFONT_ALIGN_CENTER);
    }
}

static void gfx_scegu_render_controller_mapping(int selected_index, const char *status_message,
                                                 uint8_t highlight_red, uint8_t highlight_green,
                                                 uint8_t highlight_blue) {
    char line[96];
    char value[64];
    int binding_count = psp_home_menu_get_binding_count();
    int deadzone_row = binding_count;
    int save_row = binding_count + 1;
    int reset_row = binding_count + 2;
    int back_row = binding_count + 3;
    int total_rows = back_row + 1;
    int visible_rows = 7;
    int first_row = selected_index - (visible_rows / 2);
    int row;

    if (first_row < 0) {
        first_row = 0;
    }
    if ((first_row + visible_rows) > total_rows) {
        first_row = total_rows - visible_rows;
    }

    gfx_scegu_draw_rect(0, 0, SCR_WIDTH, SCR_HEIGHT, gfx_scegu_rgba(0, 0, 0, 112));
    gfx_scegu_draw_rect(34, 20, 412, 232, gfx_scegu_rgba(0, 0, 0, 166));
    gfx_scegu_draw_home_menu_text(SCR_WIDTH / 2, 50, "Controller Mapping", 0.82f,
                                  gfx_scegu_rgba(255, 255, 245, 255), INTRAFONT_ALIGN_CENTER);

    for (row = first_row; row < first_row + visible_rows; row++) {
        int y = 82 + ((row - first_row) * 22);
        unsigned int color = gfx_scegu_rgba(218, 224, 218, 255);

        if (selected_index == row) {
            gfx_scegu_draw_rect(54, y - 17, 372, 22,
                                gfx_scegu_rgba(highlight_red, highlight_green,
                                               highlight_blue, 205));
            color = gfx_scegu_rgba(255, 255, 245, 255);
        }

        if (row < binding_count) {
            psp_home_menu_get_binding_value_text(row, value, sizeof(value));
            snprintf(line, sizeof(line), "%s: %.40s", psp_home_menu_get_binding_name(row), value);
        } else if (row == deadzone_row) {
            snprintf(line, sizeof(line), "Deadzone: %d", psp_home_menu_get_deadzone());
        } else if (row == save_row) {
            snprintf(line, sizeof(line), "Save sm64config.txt");
        } else if (row == reset_row) {
            snprintf(line, sizeof(line), "Reset defaults");
        } else {
            snprintf(line, sizeof(line), "Back");
        }

        gfx_scegu_draw_home_menu_text(70, y, line, 0.68f, color, INTRAFONT_ALIGN_LEFT);
    }

    gfx_scegu_draw_home_menu_text(
        SCR_WIDTH / 2, 242,
        ((status_message != NULL) && status_message[0])
            ? status_message
            : "Left/Right change  Cross select  Circle back",
        0.48f, gfx_scegu_rgba(185, 195, 190, 255), INTRAFONT_ALIGN_CENTER);
}

static inline uint32_t get_shader_index(uint32_t id) {
    size_t i;
    for (i = 0; i < 27; i++) {
        if (shader_ids[i] == id) {
            return i;
        }
    }
    char msg[32];
    sprintf(msg, "ERROR! Shader not known %lu\n", (unsigned long)id);
    sceIoWrite(2, msg, strlen(msg));
    return 0;
}

static inline uint32_t get_shader_remap(uint32_t id) {
    size_t index = get_shader_index(id);
    return shader_remap[index * 2 + 1];
}

static inline bool is_shader_enabled(uint32_t id) {
    size_t i;
    for (i = 0; i < 27; i++) {
        if (shader_broken[i] == id) {
            return false;
        }
    }
    return true;
}

static struct ShaderProgram *get_shader_from_id(uint32_t id) {
    size_t i;
    for (i = 0; i < shader_program_pool_size; i++) {
        if (shader_program_pool[i].shader_id == id) {
            return &shader_program_pool[i];
        }
    }
    return NULL;
}

static bool gfx_scegu_z_is_from_0_to_1(void) {
    return true;
}

static inline int texenv_set_color(UNUSED struct ShaderProgram *prg) {
    return GU_TFX_MODULATE;
}

static inline int texenv_set_texture(UNUSED struct ShaderProgram *prg) {
    return GU_TFX_MODULATE;
}

static inline int texenv_set_texture_color(struct ShaderProgram *prg) {
    int mode;
    /*@Hack: lord forgive me for this, but this is easier */
    switch (prg->shader_id) {
        case 0x0000038D: // mario's eyes
        case 0x01045A00: // peach letter
        case 0x01200A00: // intro copyright fade in
            mode = GU_TFX_DECAL;
            break;
        case 0x00000551: // goddard
            mode = GU_TFX_BLEND;
            break;
        default:
            mode = GU_TFX_MODULATE;
            break;
    }

    return mode;
}

static inline int texenv_set_texture_texture(UNUSED struct ShaderProgram *prg) {
    /*@Note: hack shader 0x1A00A6F for Bowser/Peach Paintings (still broken, but just fixed on peach)*/
    return GU_TFX_DECAL;
}

static void gfx_scegu_apply_shader(struct ShaderProgram *prg) {
    // If we have textures, Enable otherwise Disable
    if (prg->texture_used[0] || prg->texture_used[1]) {
        sceGuEnable(GU_TEXTURE_2D);
    } else {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    if (prg->shader_id & SHADER_OPT_FOG) {
        sceGuEnable(GU_FOG);

        // Set fog parameters
        //sceGuFog(1.0f, 10.0f, scegu_fog_color);
        sceGuFog(3600.0f, 32000.0f, 0xFFFFFFFF);
    } else {
        sceGuDisable(GU_FOG);
    }

    if (prg->num_inputs) {
        // have colors
        // TODO: more than one color (maybe glSecondaryColorPointer?)
        // HACK: if there's a texture and two colors, one of them is likely for speculars or some shit
        // (see mario head)
        //       if there's two colors but no texture, the real color is likely the second one
        /*
        const int hack = (prg->num_inputs > 1) * (4 - (int)prg->texture_used[0]);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, cur_buf_stride, ofs + hack);
        ofs += 4 * prg->num_inputs;
        */
    }

    if (prg->shader_id & SHADER_OPT_TEXTURE_EDGE) {
        // (horrible) alpha discard
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(GU_GREATER, 0x55, 0xff); /* 0.3f  */
    } else {
        sceGuDisable(GU_ALPHA_TEST);
    }

    if (!prg->enabled) {
        // configure formulae, we only need to do this once
        prg->enabled = true;

        int mode;
        switch (prg->mix) {
            case SH_MT_TEXTURE:
                mode = texenv_set_texture(prg);
                break;
            case SH_MT_TEXTURE_TEXTURE:
                mode = texenv_set_texture_texture(prg);
                break;
            case SH_MT_TEXTURE_COLOR:
                mode = texenv_set_texture_color(prg);
                break;
            default:
                mode = texenv_set_color(prg);
                break;
        }

        /* Transition Screens */
        if (prg->shader_id == 0x01A00045) {
            mode = GU_TFX_REPLACE;
        }
        sceGuTexFunc(mode, GU_TCC_RGBA);
    }
}

static void gfx_scegu_unload_shader(struct ShaderProgram *old_prg) {
    if (cur_shader && (cur_shader == old_prg || !old_prg)) {
        cur_shader->enabled = false;
        cur_shader = NULL;
    }
}

static void gfx_scegu_load_shader(struct ShaderProgram *new_prg) {
    cur_shader = new_prg;
    gfx_scegu_apply_shader(cur_shader);
    if (cur_shader)
        cur_shader->enabled = false;
}

static struct ShaderProgram *gfx_scegu_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures ccf;
    gfx_cc_get_features(shader_id, &ccf);

    struct ShaderProgram *prg = &shader_program_pool[shader_program_pool_size++];

    prg->shader_id = shader_id;
    prg->cc = ccf;
    prg->num_inputs = ccf.num_inputs;
    prg->texture_used[0] = ccf.used_textures[0];
    prg->texture_used[1] = ccf.used_textures[1];

    if (ccf.used_textures[0] && ccf.used_textures[1]) {
        prg->mix = SH_MT_TEXTURE_TEXTURE;
        if (ccf.do_single[1]) {
            prg->texture_ord[0] = 1;
            prg->texture_ord[1] = 0;
        } else {
            prg->texture_ord[0] = 0;
            prg->texture_ord[1] = 1;
        }
    } else if (ccf.used_textures[0] && ccf.num_inputs) {
        prg->mix = SH_MT_TEXTURE_COLOR;
    } else if (ccf.used_textures[0]) {
        prg->mix = SH_MT_TEXTURE;
    } else if (ccf.num_inputs > 1) {
        prg->mix = SH_MT_COLOR_COLOR;
    } else if (ccf.num_inputs) {
        prg->mix = SH_MT_COLOR;
    }

    prg->enabled = false;

    gfx_scegu_load_shader(prg);

    return prg;
}

static struct ShaderProgram *gfx_scegu_lookup_shader(uint32_t shader_id) {
    for (size_t i = 0; i < shader_program_pool_size; i++) {
        if (shader_program_pool[i].shader_id == shader_id) {
            return &shader_program_pool[i];
        }
    }
    return NULL;
}

static void gfx_scegu_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->texture_used[0];
    used_textures[1] = prg->texture_used[1];
}

static uint32_t gfx_scegu_new_texture(void) {
    return texman_create();
}

static uint32_t gfx_cm_to_opengl(uint32_t val) {
    if (val & G_TX_MIRROR)
        return GU_REPEAT;
    if (val & G_TX_CLAMP)
        return GU_CLAMP;
    return GU_REPEAT;
}

static inline int ispow2(uint32_t x) {
    return (x & (x - 1)) == 0;
}

// compute the next highest power of 2 of 32-bit v
static inline int nextpow2(int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v++;
    return v;
}

static inline void gfx_scegu_apply_tmu_state(const int tile) {
    sceGuTexFilter(tmu_state[tile].min_filter, tmu_state[tile].mag_filter);
    sceGuTexWrap(tmu_state[tile].wrap_s, tmu_state[tile].wrap_t);
}

static void gfx_scegu_set_sampler_parameters(const int tile, const bool linear_filter, const uint32_t cms, const uint32_t cmt) {
    const int filter = linear_filter ? GU_LINEAR : GU_NEAREST;

    const int wrap_s = gfx_cm_to_opengl(cms);
    const int wrap_t = gfx_cm_to_opengl(cmt);

    tmu_state[tile].min_filter = filter;
    tmu_state[tile].mag_filter = filter;
    tmu_state[tile].wrap_s = wrap_s;
    tmu_state[tile].wrap_t = wrap_t;

    // set state for the first texture right away
    if (!tile)
        gfx_scegu_apply_tmu_state(tile);
}

static void gfx_scegu_select_texture(int tile, uint32_t texture_id) {
    if (tmu_state[tile].tex != texture_id) {
        tmu_state[tile].tex = texture_id;
        texman_bind_tex(texture_id);
        gfx_scegu_set_sampler_parameters(tile, false, 0, 0);
    }
}

/* Used for rescaling textures ROUGHLY into pow2 dims */
static unsigned int __attribute__((aligned(16))) scaled[256 * 256 * sizeof(unsigned int)]; /* 16kb */
static void gfx_scegu_resample_32bit(const unsigned int *in, int inwidth, int inheight, unsigned int *out, int outwidth, int outheight) {
    int i, j;
    const unsigned int *inrow;
    unsigned int frac, fracstep;

    fracstep = inwidth * 0x10000 / outwidth;
    for (i = 0; i < outheight; i++, out += outwidth) {
        inrow = in + inwidth * (i * inheight / outheight);
        frac = fracstep >> 1;
        for (j = 0; j < outwidth; j += 4) {
            out[j] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 1] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 2] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 3] = inrow[frac >> 16];
            frac += fracstep;
        }
    }
}

static void gfx_scegu_resample_16bit(const unsigned short *in, int inwidth, int inheight, unsigned short *out, int outwidth, int outheight) {
    int i, j;
    const unsigned short *inrow;
    unsigned int frac, fracstep;

    fracstep = inwidth * 0x10000 / outwidth;
    for (i = 0; i < outheight; i++, out += outwidth) {
        inrow = in + inwidth * (i * inheight / outheight);
        frac = fracstep >> 1;
        for (j = 0; j < outwidth; j += 4) {
            out[j] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 1] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 2] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 3] = inrow[frac >> 16];
            frac += fracstep;
        }
    }
}

static void gfx_scegu_resample_8bit(const unsigned char *in, int inwidth, int inheight, unsigned char *out, int outwidth, int outheight) {
    int i, j;
    const unsigned char *inrow;
    unsigned int frac, fracstep;

    fracstep = inwidth * 0x10000 / outwidth;
    for (i = 0; i < outheight; i++, out += outwidth) {
        inrow = in + inwidth * (i * inheight / outheight);
        frac = fracstep >> 1;
        for (j = 0; j < outwidth; j += 4) {
            out[j] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 1] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 2] = inrow[frac >> 16];
            frac += fracstep;
            out[j + 3] = inrow[frac >> 16];
            frac += fracstep;
        }
    }
}

static void gfx_scegu_upload_texture(const uint8_t *rgba32_buf, int width, int height, unsigned int type) {
    if (ispow2(width) && ispow2(height)) {
        texman_upload_swizzle(width, height, type, (void *) rgba32_buf);
    } else {
        int scaled_width = nextpow2(width);
        int scaled_height = nextpow2(height);

        if (type == GU_PSM_8888) {
            gfx_scegu_resample_32bit((const unsigned int *) rgba32_buf, width, height, (void *) scaled, scaled_width, scaled_height);
            texman_upload_swizzle(scaled_width, scaled_height, type, (void *) scaled);
        } else if (type == GU_PSM_5551) {
            gfx_scegu_resample_16bit((const unsigned short *) rgba32_buf, width, height, (void *) scaled, scaled_width, scaled_height);
            texman_upload_swizzle(scaled_width, scaled_height, type, (void *) scaled);
        } else {
            gfx_scegu_resample_8bit((const unsigned char *) rgba32_buf, width, height, (void *) scaled, scaled_width, scaled_height);
            texman_upload_swizzle(scaled_width, scaled_height, type, (void *) scaled);
        }
    }
}

static void gfx_scegu_set_depth_test(bool depth_test) {
    if (depth_test) {
        sceGuEnable(GU_DEPTH_TEST);
    } else {
        sceGuDisable(GU_DEPTH_TEST);
    }
}

static void gfx_scegu_set_depth_mask(bool z_upd) {
    sceGuDepthMask(z_upd ? GU_FALSE : GU_TRUE);
}

static void gfx_scegu_set_zmode_decal(bool zmode_decal) {
    if (zmode_decal) {
        sceGuDepthOffset(32); /* I think we need a little more on psp because of 16bit depth buffer */
    } else {
        sceGuDepthOffset(0);
    }
}

static void gfx_scegu_set_viewport(int x, int y, int width, int height) {
    sceGuViewport(2048 - (SCR_WIDTH / 2) + x + (width / 2), 2048 + (SCR_HEIGHT / 2) - y - (height / 2), width, height);
    sceGuScissor(x, SCR_HEIGHT - y - height, x + width, SCR_HEIGHT - y);
}

static void gfx_scegu_set_scissor(int x, int y, int width, int height) {
    sceGuScissor(x, SCR_HEIGHT - y - height, x + width, SCR_HEIGHT - y);
}

static void gfx_scegu_set_use_alpha(bool use_alpha) {
    gl_blend = use_alpha;
    if (use_alpha) {
        sceGuEnable(GU_BLEND);
    } else {
        sceGuDisable(GU_BLEND);
    }
}

// draws the same triangles as plain fog color + fog intensity as alpha
// on top of the normal tris and blends them to achieve sort of the same effect
// as fog would
static inline void gfx_scegu_blend_fog_tris(void) {
    /*@Todo: figure this out! */
    return;
#if 0
    // if a texture was used, replace it with fog color instead, but still keep the alpha
    if (cur_shader->texture_used[0]) {
        glActiveTexture(GL_TEXTURE0);
        TEXENV_COMBINE_ON();
        // out.rgb = input0.rgb
        TEXENV_COMBINE_SET1(RGB, GL_REPLACE, GL_PRIMARY_COLOR);
        // out.a = texel0.a * input0.a
        TEXENV_COMBINE_SET2(ALPHA, GL_MODULATE, GL_TEXTURE, GL_PRIMARY_COLOR);
    }

    glEnableClientState(GL_COLOR_ARRAY); // enable color array temporarily
    glColorPointer(4, GL_FLOAT, cur_buf_stride, cur_fog_ofs); // set fog colors as primary colors
    if (!gl_blend) glEnable(GL_BLEND); // enable blending temporarily
    glDepthFunc(GL_LEQUAL); // Z is the same as the base triangles

    glDrawArrays(GL_TRIANGLES, 0, 3 * cur_buf_num_tris);

    glDepthFunc(GL_LESS); // set back to default
    if (!gl_blend) glDisable(GL_BLEND); // disable blending if it was disabled
    glDisableClientState(GL_COLOR_ARRAY); // will get reenabled later anyway
#endif
}

static void gfx_scegu_draw_triangles(float buf_vbo[], UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (!is_shader_enabled(cur_shader->shader_id)) {
        gfx_scegu_apply_shader(get_shader_from_id(get_shader_remap(cur_shader->shader_id)));
    }

    gfx_scegu_reserve_list_memory(sizeof(Vertex) * 3 * buf_vbo_num_tris);
    void *buf = sceGuGetMemory(sizeof(Vertex) * 3 * buf_vbo_num_tris);
    memcpy_vfpu(buf, buf_vbo, sizeof(Vertex) * 3 * buf_vbo_num_tris);
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3 * buf_vbo_num_tris, 0, buf);

    // cur_fog_ofs is only set if GL_EXT_fog_coord isn't used
    // if (cur_fog_ofs) gfx_scegu_blend_fog_tris();
}

void gfx_scegu_draw_triangles_2d(float buf_vbo[], UNUSED size_t buf_vbo_len, UNUSED size_t buf_vbo_num_tris) {
    if (!is_shader_enabled(cur_shader->shader_id)) {
        gfx_scegu_apply_shader(get_shader_from_id(get_shader_remap(cur_shader->shader_id)));
    }

    gfx_scegu_reserve_list_memory(sizeof(VertexColor) * 2);
    void *quad_buf = sceGuGetMemory(sizeof(VertexColor) * 2);
    memcpy(quad_buf, buf_vbo, sizeof(VertexColor) * 2);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, quad_buf);
}

static void gfx_scegu_init(void) {
    sceGuInit();

    void *fbp0 = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_5650);
    void *fbp1 = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_5650);
    void *zbp = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_4444);

    sDrawBuffer = fbp0;
    sDisplayBuffer = fbp1;

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_5650, fbp0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, BUF_WIDTH);
    sceGuDepthBuffer(zbp, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2), SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(0xffff, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0x55, 0xff); /* 0.3f  */
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);
    sceGuFrontFace(GU_CCW);
    sceGuDepthMask(GU_FALSE);
    sceGuTexEnvColor(0xffffffff);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    void *texman_buffer = getStaticVramBufferBytes(TEXMAN_BUFFER_SIZE);
    void *texman_aligned = (void *) ((((unsigned int) texman_buffer + TEX_ALIGNMENT - 1) / TEX_ALIGNMENT) * TEX_ALIGNMENT);
    texman_reset(texman_aligned, TEXMAN_BUFFER_SIZE);
    if (!texman_buffer) {
        char msg[32];
        sprintf(msg, "OUT OF MEMORY!\n");
        sceIoWrite(1, msg, strlen(msg));

        sceKernelExitGame();
    }

    /* Load the firmware font before game assets consume the remaining heap. */
    gfx_scegu_ensure_home_menu_font();
}

static void gfx_scegu_start_frame(void) {
    bool has_home_menu_background;

    /* Callback rendering (intraFont in particular) changes GU texture state
     * outside the normal Fast3D cache. Rebind the first texture used by every
     * frame so resuming from the custom HOME menu cannot inherit that state. */
    tmu_state[0].tex = UINT32_MAX;
    tmu_state[1].tex = UINT32_MAX;

    if (sHomeMenuBgCaptureRequested) {
        gfx_scegu_copy_framebuffer_from_vram(sHomeMenuBgBuffer, sDisplayBuffer);
        gfx_scegu_blur_framebuffer_565(sHomeMenuBgBuffer);
        sHomeMenuBgCaptureRequested = false;
        sHomeMenuBgCaptured = true;
    }

    has_home_menu_background = sHomeMenuBgActive && sHomeMenuBgCaptured;
    if (has_home_menu_background) {
        gfx_scegu_copy_framebuffer_to_vram(sDrawBuffer, sHomeMenuBgBuffer);
    }

    sceGuStart(GU_DIRECT, list);
    sceGuDisable(GU_SCISSOR_TEST);
    sceGuDepthMask(GU_TRUE); // Must be set to clear Z-buffer
    sceGuClearDepth(0);
    if (has_home_menu_background) {
        sceGuClear(GU_DEPTH_BUFFER_BIT);
    } else {
        sceGuClearColor(0xFF000000);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    }
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthMask(GU_FALSE);

    // Identity every frame? unsure.
    //sceGuSetMatrix(GU_PROJECTION, (const ScePspFMatrix4 *) identity_matrix);
    sceGuSetMatrix(GU_VIEW, (const ScePspFMatrix4 *) identity_matrix);
    //sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *) identity_matrix);

#if 0
    const int DitherMatrix[2][16] = { { 0, 8, 0, 8,
                         8, 0, 8, 0,
                         0, 8, 0, 8,
                         8, 0, 8, 0 },
                        { 8, 8, 8, 8,
                          0, 8, 0, 8,
                          8, 8, 8, 8,
                          0, 8, 0, 8 } };

    extern int gDoDither;
    extern int gFrame;

    sceGuDisable(GU_DITHER);
    if(gDoDither){
        // every frame
        sceGuSetDither((const ScePspIMatrix4 *)DitherMatrix[(gFrame&1)]);
        sceGuEnable(GU_DITHER);
    }
#endif
}

void gfx_scegu_on_resize(void) {
}

static void gfx_scegu_end_frame(void) {
    void *previous_draw_buffer = sDrawBuffer;

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sDrawBuffer = sceGuSwapBuffers();
    sDisplayBuffer = previous_draw_buffer;
}

static void gfx_scegu_finish_render(void) {
    /* There should be something here! */
}

// clang-format off
struct GfxRenderingAPI gfx_opengl_api = {
    gfx_scegu_z_is_from_0_to_1,
    gfx_scegu_unload_shader,
    gfx_scegu_load_shader,
    gfx_scegu_create_and_load_new_shader,
    gfx_scegu_lookup_shader,
    gfx_scegu_shader_get_info,
    gfx_scegu_new_texture,
    gfx_scegu_select_texture,
    gfx_scegu_upload_texture,
    gfx_scegu_set_sampler_parameters,
    gfx_scegu_set_depth_test,
    gfx_scegu_set_depth_mask,
    gfx_scegu_set_zmode_decal,
    gfx_scegu_set_viewport,
    gfx_scegu_set_scissor,
    gfx_scegu_set_use_alpha,
    gfx_scegu_draw_triangles,
    gfx_scegu_init,
    gfx_scegu_on_resize,
    gfx_scegu_start_frame,
    gfx_scegu_end_frame,
    gfx_scegu_finish_render
};

void gfx_scegu_request_home_menu_background(void) {
    sHomeMenuBgCaptureRequested = true;
}

void gfx_scegu_set_home_menu_background_active(bool active) {
    sHomeMenuBgActive = active;
    if (!active) {
        sHomeMenuBgCaptureRequested = false;
        sHomeMenuBgCaptured = false;
    }
}

void gfx_scegu_render_home_menu(int selected_index, int screen, int control_selected_index,
                                const char *status_message, uint8_t highlight_red,
                                uint8_t highlight_green, uint8_t highlight_blue) {
    gfx_scegu_prepare_home_menu_draw();
    if (screen == 1) {
        gfx_scegu_render_controller_mapping(control_selected_index, status_message, highlight_red,
                                             highlight_green, highlight_blue);
    } else {
        gfx_scegu_render_home_menu_main(selected_index, highlight_red, highlight_green,
                                        highlight_blue);
    }
}

#endif // RAPI_GL_LEGACY
