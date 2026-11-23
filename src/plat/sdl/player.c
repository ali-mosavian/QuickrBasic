/* gfx's lists played with SDL_GPU (qxlist.h), and the canvas they draw shown in the window.
 *
 * # How it works
 * The window's renderer is SDL's GPU renderer; the player shares its device. A list plays on the
 * calling thread, the interpreter's, as one command buffer submitted without a wait: a play waits
 * only for the frame FLIGHT back, and a read for its own. The list's data, its vertices, discs,
 * uploads and palette, is first copied into one staging buffer kept between frames, cycled while a
 * frame behind still reads it, so the list's memory is free when play returns. The vertices and
 * discs go up to a buffer each; then each op in turn, a render pass held open across draws and
 * begun again after an upload. A quad's four vertices are drawn through one index buffer, 0 1 2
 * 0 2 3 each. Every texture is RGBA8 or R8 and a colour target, so TEXTURE clears it by a pass.
 * One pipeline a kind of vertex and blend; the blend's alpha is the pass's blend constant. SHOW
 * copies the canvas to the texture the window's thread shows, cycling it: the copy goes to a
 * fresh one while a frame still samples the last. The window's thread wraps it for its renderer
 * and alone releases it; it binds it under the lock SHOW takes, so the cycle sees the binding.
 */
#include <stdlib.h>
#include <string.h>
#include "plat/pgfx.h"
#include "qx/qxlist.h"
#include "sdl.h"
#include "shaders.h"

enum {
    RGBA = 4,
    BLENDS = 3,
    PLAIN_ATTRIBUTES = 5,
    DISC_ATTRIBUTES = 7,
    SAMPLERS = 2,
    FLIGHT = 2, /* frames the GPU may be behind */
    ALIGN = 16, /* a staged block's offset */
    QUAD = 4,
    QUAD_INDICES = 6,
    DOWN = -1, /* Metal's clip space: y runs up the rows */
};

/* The vertices of VERTICES and of DISCS. */
typedef enum { PLAIN, ROUND, KINDS } Kind;

static_assert(offsetof(QxDisc, v) == 0, "a disc's vertex is a plain one's attributes");

typedef struct {
    uint32_t id, w, h, bpp;
    SDL_GPUTexture *t;
} Texture;

typedef struct {
    SDL_GPUBuffer *b;
    uint32_t cap; /* bytes */
} Buffer;

/* What the open render pass draws with. */
typedef struct {
    SDL_GPUCommandBuffer *cb;
    SDL_GPURenderPass *render;
    SDL_GPUCopyPass *copy;
    uint32_t target; /* the texture drawn into, while aimed */
    bool aimed;
    SDL_Rect clip;
    int32_t bound;          /* the pipeline bound, kind * BLENDS + blend; -1 none */
    bool data;              /* the staging buffer holds the list's data */
    uint32_t staged;        /* the op's data, its offset there */
    uint32_t counts[KINDS]; /* the vertices up */
} Pass;

static struct {
    SDL_GPUDevice *dev;
    SDL_GPUGraphicsPipeline *pipelines[KINDS][BLENDS];
    SDL_GPUSampler *nearest;
    SDL_GPUTexture *palette;
    Buffer arrays[KINDS];
    Buffer indices;
    SDL_GPUTransferBuffer *staging;
    uint32_t capstaging;
    SDL_GPUFence *fences[FLIGHT];
    uint32_t frame;
    Texture *textures;
    uint32_t ntextures;
    SDL_Mutex *lock;         /* over the shown texture */
    SDL_GPUTexture *shown;   /* the canvas as last shown */
    SDL_GPUTexture *retired; /* a shown texture the window's thread is to release */
    uint32_t w, h;
    bool showing; /* shown since open */
} p;

static Texture *texture(const uint32_t id)
{
    for (uint32_t i = 0; i < p.ntextures; i++)
        if (p.textures[i].id == id)
            return &p.textures[i];

    return nullptr;
}

static SDL_GPUTexture *make(const uint32_t w, const uint32_t h, const SDL_GPUTextureFormat f)
{
    return SDL_CreateGPUTexture(
        p.dev,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = f,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            .width = w,
            .height = h,
            .layer_count_or_depth = 1,
            .num_levels = 1,
        }

    );
}

/* b at least bytes of usage, made again twice that when smaller. */
static bool fits(Buffer *const b, const uint32_t bytes, const SDL_GPUBufferUsageFlags usage)
{
    if (bytes <= b->cap)
        return true;

    SDL_ReleaseGPUBuffer(p.dev, b->b);
    b->cap = bytes * 2;
    b->b = SDL_CreateGPUBuffer(p.dev, &(SDL_GPUBufferCreateInfo){.usage = usage, .size = b->cap});

    if (!b->b)
        b->cap = 0;

    return b->b;
}

/* f waited for and released, if any. */
static void waited(SDL_GPUFence **const f)
{
    if (*f) {
        SDL_WaitForGPUFences(p.dev, true, f, 1);
        SDL_ReleaseGPUFence(p.dev, *f);
    }

    *f = nullptr;
}

/* The command buffer submitted and waited for. */
static void finish(SDL_GPUCommandBuffer *const cb)
{
    SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    waited(&f);
}

/* Every frame played, drawn. */
static void settle(void)
{
    for (uint32_t i = 0; i < FLIGHT; i++)
        waited(&p.fences[i]);
}

static void end(Pass *const s)
{
    if (s->render)
        SDL_EndGPURenderPass(s->render);

    if (s->copy)
        SDL_EndGPUCopyPass(s->copy);

    s->render = nullptr;
    s->copy = nullptr;
    s->bound = -1;
}

static SDL_GPUCopyPass *copying(Pass *const s)
{
    if (!s->copy) {
        end(s);
        s->copy = SDL_BeginGPUCopyPass(s->cb);
    }

    return s->copy;
}

/* t cleared to 0 by a pass of its own. */
static void cleared(Pass *const s, SDL_GPUTexture *const t)
{
    end(s);
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(
        s->cb,
        &(SDL_GPUColorTargetInfo){.texture = t,
                                  .load_op = SDL_GPU_LOADOP_CLEAR,
                                  .store_op = SDL_GPU_STOREOP_STORE},
        1,
        nullptr
    ));
}

static void add(
    Pass *const s,
    const uint32_t id,
    const uint32_t w,
    const uint32_t h,
    const uint32_t format
)
{
    const SDL_GPUTextureFormat f = format == QX_TEXTURE_INDEX
                                       ? SDL_GPU_TEXTUREFORMAT_R8_UNORM
                                       : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    Texture *const grown = SDL_realloc(p.textures, (p.ntextures + 1) * sizeof *grown);

    if (!grown)
        return;

    p.textures = grown;
    p.textures[p.ntextures++] =
        (Texture){id, w, h, format == QX_TEXTURE_INDEX ? 1 : RGBA, make(w, h, f)};

    cleared(s, p.textures[p.ntextures - 1].t);
}

static void drop(const uint32_t id)
{
    Texture *const t = texture(id);

    if (!t)
        return;

    SDL_ReleaseGPUTexture(p.dev, t->t);
    *t = p.textures[--p.ntextures];
}

static const void *pointer(const uint32_t *const a)
{
    return (const void *)(uintptr_t)((uint64_t)a[1] << 32 | a[0]);
}

/* The bytes op's data takes staged, to ALIGN; copied to to unless null. */
static uint32_t staged(const uint32_t *const op, uint8_t *const to)
{
    const uint32_t *const a = op + 1;
    const void *from = nullptr;
    uint32_t bytes = 0;

    switch ((QxListOp)(op[0] & 0xFF)) {
    case QX_LIST_VERTICES:
        bytes = a[0] * (uint32_t)sizeof(QxVertex);
        from = pointer(a + 1);
        break;

    case QX_LIST_DISCS:
        bytes = a[0] * (uint32_t)sizeof(QxDisc);
        from = pointer(a + 1);
        break;

    case QX_LIST_UPLOAD:
        bytes = a[4] * a[5];
        from = pointer(a + 6);
        break;

    case QX_LIST_PALETTE:
        bytes = QX_ROWS * QX_COLOURS * RGBA;
        from = pointer(a);
        break;

    default:
        break;
    }

    if (to && bytes)
        memcpy(to, from, bytes);

    return (bytes + ALIGN - 1) / ALIGN * ALIGN;
}

/* The list's data copied to to, or its bytes when null. */
static uint32_t stage(const uint32_t *const list, const uint32_t words, uint8_t *const to)
{
    uint32_t bytes = 0;

    for (uint32_t at = 0, size; at < words && (size = list[at] >> QX_LIST_SIZE); at += size)
        bytes += staged(list + at, to ? to + bytes : nullptr);

    return bytes;
}

/* The list's data in the staging buffer, made again twice that when smaller. */
static bool staging(const uint32_t *const list, const uint32_t words)
{
    const uint32_t bytes = stage(list, words, nullptr);

    if (bytes > p.capstaging) {
        SDL_ReleaseGPUTransferBuffer(p.dev, p.staging);
        p.capstaging = bytes * 2;
        p.staging = SDL_CreateGPUTransferBuffer(
            p.dev,
            &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                               .size = p.capstaging}

        );

        if (!p.staging)
            p.capstaging = 0;
    }

    uint8_t *const to =
        bytes && p.staging ? SDL_MapGPUTransferBuffer(p.dev, p.staging, true) : nullptr;

    if (to) {
        stage(list, words, to);
        SDL_UnmapGPUTransferBuffer(p.dev, p.staging);
    }

    return to;
}

/* The index buffer, for n quads at least. */
static bool indexed(Pass *const s, const uint32_t n)
{
    static const uint32_t corners[QUAD_INDICES] = {0, 1, 2, 0, 2, 3};

    const uint32_t row = QUAD_INDICES * sizeof(uint32_t);

    if (n * row <= p.indices.cap)
        return true;

    if (!fits(&p.indices, n * row, SDL_GPU_BUFFERUSAGE_INDEX))
        return false;

    SDL_GPUTransferBuffer *const tb = SDL_CreateGPUTransferBuffer(
        p.dev,
        &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                           .size = p.indices.cap}

    );
    uint32_t *const to = tb ? SDL_MapGPUTransferBuffer(p.dev, tb, false) : nullptr;

    if (to) {
        for (uint32_t q = 0; q < p.indices.cap / row; q++)
            for (uint32_t i = 0; i < QUAD_INDICES; i++)
                to[q * QUAD_INDICES + i] = q * QUAD + corners[i];

        SDL_UnmapGPUTransferBuffer(p.dev, tb);
        SDL_UploadToGPUBuffer(
            copying(s),
            &(SDL_GPUTransferBufferLocation){.transfer_buffer = tb},
            &(SDL_GPUBufferRegion){.buffer = p.indices.b, .size = p.indices.cap},
            false
        );
    } else
        p.indices.cap = 0;

    SDL_ReleaseGPUTransferBuffer(p.dev, tb);
    return to;
}

/* n vertices of kind k, size bytes each, from the staging buffer to their own. */
static void array(Pass *const s, const Kind k, const uint32_t n, const uint32_t size)
{
    s->counts[k] = 0;

    if (!n || !s->data || !fits(&p.arrays[k], n * size, SDL_GPU_BUFFERUSAGE_VERTEX) ||
        !indexed(s, n / QUAD))
        return;

    SDL_UploadToGPUBuffer(
        copying(s),
        &(SDL_GPUTransferBufferLocation){.transfer_buffer = p.staging, .offset = s->staged},
        &(SDL_GPUBufferRegion){.buffer = p.arrays[k].b, .size = n * size},
        true
    );

    s->counts[k] = n;
}

/* The op's rows from the staging buffer into r of t, row texels each. */
static void upload(Pass *const s, SDL_GPUTexture *const t, const SDL_Rect r, const uint32_t row)
{
    if (!s->data)
        return;

    SDL_UploadToGPUTexture(
        copying(s),
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = p.staging,
            .offset = s->staged,
            .pixels_per_row = row,
        },
        &(SDL_GPUTextureRegion){
            .texture = t,
            .x = (uint32_t)r.x,
            .y = (uint32_t)r.y,
            .w = (uint32_t)r.w,
            .h = (uint32_t)r.h,
            .d = 1,
        },
        false
    );
}

/* The render pass into t, the target, its pixels kept, begun if none is open. */
static SDL_GPURenderPass *drawing(Pass *const s, const Texture *const t)
{
    if (s->render)
        return s->render;

    end(s);
    s->render = SDL_BeginGPURenderPass(
        s->cb,
        &(SDL_GPUColorTargetInfo){.texture = t->t,
                                  .load_op = SDL_GPU_LOADOP_LOAD,
                                  .store_op = SDL_GPU_STOREOP_STORE},
        1,
        nullptr
    );

    const struct {
        float w, h, down, pad;
    } size = {(float)t->w, (float)t->h, DOWN, 0};

    SDL_SetGPUViewport(s->render, &(SDL_GPUViewport){0, 0, (float)t->w, (float)t->h, 0, 1});

    SDL_SetGPUScissor(s->render, &s->clip);
    SDL_PushGPUVertexUniformData(s->cb, 0, &size, sizeof size);
    SDL_BindGPUIndexBuffer(
        s->render,
        &(SDL_GPUBufferBinding){.buffer = p.indices.b},
        SDL_GPU_INDEXELEMENTSIZE_32BIT
    );

    return s->render;
}

static void draw(Pass *const s, const Kind k, const uint32_t *const a)
{
    const Texture *const source = texture(a[2]);
    const Texture *const target = s->aimed ? texture(s->target) : nullptr;
    const uint32_t first = a[0], count = a[1], blend = a[3];

    if (!target || !source || blend >= BLENDS || first + count > s->counts[k])
        return;

    SDL_GPURenderPass *const r = drawing(s, target);
    const int32_t bound = (int32_t)(k * BLENDS + blend);
    const float alpha = (float)a[4] / 255;

    if (s->bound != bound) {
        SDL_BindGPUGraphicsPipeline(r, p.pipelines[k][blend]);
        SDL_BindGPUVertexBuffers(r, 0, &(SDL_GPUBufferBinding){.buffer = p.arrays[k].b}, 1);

        s->bound = bound;
    }

    SDL_SetGPUBlendConstants(r, (SDL_FColor){alpha, alpha, alpha, alpha});

    SDL_BindGPUFragmentSamplers(
        r,
        0,
        (const SDL_GPUTextureSamplerBinding[]){{source->t, p.nearest}, {p.palette, p.nearest}},
        SAMPLERS
    );

    SDL_DrawGPUIndexedPrimitives(
        r,
        count / QUAD * QUAD_INDICES,
        1,
        first / QUAD * QUAD_INDICES,
        0,
        0
    );
}

/* The canvas copied to the texture the window shows. */
static void show(Pass *const s)
{
    const Texture *const canvas = texture(QX_CANVAS);

    if (!canvas)
        return;

    SDL_LockMutex(p.lock);
    SDL_CopyGPUTextureToTexture(
        copying(s),
        &(SDL_GPUTextureLocation){.texture = canvas->t},
        &(SDL_GPUTextureLocation){.texture = p.shown},
        canvas->w,
        canvas->h,
        1,
        true
    );

    p.showing = true;
    SDL_UnlockMutex(p.lock);
}

static void player_play(const uint32_t *const list, const uint32_t words)
{
    Pass s = {.cb = SDL_AcquireGPUCommandBuffer(p.dev), .bound = -1};

    if (!s.cb)
        return;

    s.data = staging(list, words);

    for (uint32_t at = 0, size; at < words && (size = list[at] >> QX_LIST_SIZE); at += size) {
        const uint32_t *const a = list + at + 1;

        switch ((QxListOp)(list[at] & 0xFF)) {
        case QX_LIST_VERTICES:
            array(&s, PLAIN, a[0], sizeof(QxVertex));
            break;

        case QX_LIST_DISCS:
            array(&s, ROUND, a[0], sizeof(QxDisc));
            break;

        case QX_LIST_TEXTURE:
            add(&s, a[0], a[1], a[2], a[3]);
            break;

        case QX_LIST_FREE:
            end(&s);
            drop(a[0]);
            break;

        case QX_LIST_UPLOAD: {
            const Texture *const t = texture(a[0]);

            if (t)
                upload(
                    &s,
                    t->t,
                    (SDL_Rect){(int)a[1], (int)a[2], (int)a[3], (int)a[4]},
                    a[5] / t->bpp
                );

            break;
        }

        case QX_LIST_PALETTE:
            upload(&s, p.palette, (SDL_Rect){0, 0, QX_COLOURS, QX_ROWS}, QX_COLOURS);

            break;

        case QX_LIST_PASS: {
            const Texture *const t = texture(a[0]);

            end(&s);
            s.target = a[0];
            s.aimed = t;
            s.clip = (SDL_Rect){0, 0, t ? (int)t->w : 0, t ? (int)t->h : 0};

            break;
        }

        case QX_LIST_CLIP:
            s.clip = (SDL_Rect){(int)a[0], (int)a[1], (int)a[2], (int)a[3]};

            if (s.render)
                SDL_SetGPUScissor(s.render, &s.clip);

            break;

        case QX_LIST_DRAW:
            draw(&s, PLAIN, a);
            break;

        case QX_LIST_DRAW_DISCS:
            draw(&s, ROUND, a);
            break;

        case QX_LIST_SHOW:
            show(&s);
            break;

        case QX_LIST_END:
            break;
        }

        s.staged += staged(list + at, nullptr);
    }

    end(&s);

    SDL_GPUFence **const behind = &p.fences[p.frame++ % FLIGHT];
    waited(behind);
    *behind = SDL_SubmitGPUCommandBufferAndAcquireFence(s.cb);
}

/* w by h at x, y of t into out, RGBA. */
static void download(
    SDL_GPUTexture *const t,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    const uint32_t bytes = (uint32_t)(w * h) * RGBA;
    SDL_GPUTransferBuffer *const tb =
        t ? SDL_CreateGPUTransferBuffer(
                p.dev,
                &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                   .size = bytes}

            )
          : nullptr;
    SDL_GPUCommandBuffer *const cb = tb ? SDL_AcquireGPUCommandBuffer(p.dev) : nullptr;

    memset(out, 0, bytes);

    if (!cb)
        return;

    SDL_GPUCopyPass *const c = SDL_BeginGPUCopyPass(cb);
    SDL_DownloadFromGPUTexture(
        c,
        &(SDL_GPUTextureRegion){
            .texture = t,
            .x = (uint32_t)x,
            .y = (uint32_t)y,
            .w = (uint32_t)w,
            .h = (uint32_t)h,
            .d = 1,
        },
        &(SDL_GPUTextureTransferInfo){.transfer_buffer = tb, .pixels_per_row = (uint32_t)w}

    );
    SDL_EndGPUCopyPass(c);
    finish(cb);

    const uint8_t *const from = SDL_MapGPUTransferBuffer(p.dev, tb, false);

    if (from)
        memcpy(out, from, bytes);

    SDL_UnmapGPUTransferBuffer(p.dev, tb);
    SDL_ReleaseGPUTransferBuffer(p.dev, tb);
}

static void player_read(
    const uint32_t id,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    const Texture *const t = texture(id);
    download(t ? t->t : nullptr, x, y, w, h, out);
}

static bool player_open(const uint16_t w, const uint16_t h)
{
    SDL_GPUCommandBuffer *const cb = SDL_AcquireGPUCommandBuffer(p.dev);
    SDL_GPUTexture *const shown = make(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    Pass s = {.cb = cb, .bound = -1};

    if (!cb || !shown)
        return false;

    p.w = w;
    p.h = h;
    add(&s, QX_CANVAS, w, h, QX_TEXTURE_RGBA);
    cleared(&s, shown);
    end(&s);
    finish(cb);

    SDL_LockMutex(p.lock);
    p.shown = shown;
    p.showing = false;
    SDL_UnlockMutex(p.lock);
    return true;
}

static void player_close(void)
{
    settle();

    while (p.ntextures)
        drop(p.textures[0].id);

    SDL_LockMutex(p.lock);
    p.retired = p.shown;
    p.shown = nullptr;
    p.showing = false;
    SDL_UnlockMutex(p.lock);
}

static SDL_GPUShader *shader(
    const char *const code,
    const SDL_GPUShaderStage stage,
    const uint32_t samplers,
    const uint32_t uniforms
)
{
    return SDL_CreateGPUShader(
        p.dev,
        &(SDL_GPUShaderCreateInfo){
            .code_size = strlen(code),
            .code = (const uint8_t *)code,
            .entrypoint = "main0",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = stage,
            .num_samplers = samplers,
            .num_uniform_buffers = uniforms,
        }

    );
}

/* The shader's pipeline for vertices of kind k with blend: NONE, MIX or ADD, by the constant's
   alpha; drawn is opaque. A disc's first attributes are its vertex's. */
static SDL_GPUGraphicsPipeline *pipeline(
    SDL_GPUShader *const vs,
    SDL_GPUShader *const fs,
    const Kind k,
    const QxBlend blend
)
{
    static const SDL_GPUVertexAttribute attributes[DISC_ATTRIBUTES] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(QxVertex, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(QxVertex, u)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT4, offsetof(QxVertex, rx)},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4, offsetof(QxVertex, r)},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4, offsetof(QxVertex, row)},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(QxDisc, disc)},
        {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(QxDisc, sweep)},
    };

    static const uint32_t pitches[KINDS] = {sizeof(QxVertex), sizeof(QxDisc)};

    static const uint32_t used[KINDS] = {PLAIN_ATTRIBUTES, DISC_ATTRIBUTES};

    const SDL_GPUColorTargetBlendState blends[BLENDS] = {
        {.enable_blend = false},
        {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .enable_blend = true,
        },
        {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .enable_blend = true,
        },
    };

    return SDL_CreateGPUGraphicsPipeline(
        p.dev,
        &(SDL_GPUGraphicsPipelineCreateInfo){
            .vertex_shader = vs,
            .fragment_shader = fs,
            .vertex_input_state =
                {
                    .vertex_buffer_descriptions =
                        &(SDL_GPUVertexBufferDescription){.pitch = pitches[k]},
                    .num_vertex_buffers = 1,
                    .vertex_attributes = attributes,
                    .num_vertex_attributes = used[k],
                },
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info = {
                .color_target_descriptions =
                    &(SDL_GPUColorTargetDescription){
                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                        .blend_state = blends[blend],
                    },
                .num_color_targets = 1,
            },
        }

    );
}

bool sdl_player_init(SDL_GPUDevice *const dev)
{
    static const PgfxPlayer player = {player_open, player_close, player_play, player_read};

    const char *const vertex[KINDS] = {draw_vert_msl, disc_vert_msl};

    p.dev = dev;

    if (!(SDL_GetGPUShaderFormats(dev) & SDL_GPU_SHADERFORMAT_MSL))
        return false;

    SDL_GPUShader *const fs = shader(draw_frag_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, SAMPLERS, 0);
    SDL_GPUCommandBuffer *const cb = SDL_AcquireGPUCommandBuffer(dev);
    Pass s = {.cb = cb, .bound = -1};

    bool made = fs;

    for (Kind k = 0; k < KINDS; k++) {
        SDL_GPUShader *const vs = shader(vertex[k], SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);

        for (QxBlend b = 0; b < BLENDS; b++)
            made = made && vs && (p.pipelines[k][b] = pipeline(vs, fs, k, b));

        SDL_ReleaseGPUShader(dev, vs);
    }

    SDL_ReleaseGPUShader(dev, fs);

    p.lock = SDL_CreateMutex();
    p.nearest = SDL_CreateGPUSampler(dev, &(SDL_GPUSamplerCreateInfo){});

    p.palette = make(QX_COLOURS, QX_ROWS, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    if (!cb || !made || !p.lock || !p.nearest || !p.palette)
        return false;

    cleared(&s, p.palette);
    end(&s);
    finish(cb);
    pgfx_set_player(&player);
    return true;
}

bool sdl_player_size(uint32_t *const w, uint32_t *const h)
{
    SDL_LockMutex(p.lock);
    const bool shown = p.showing;

    if (shown) {
        *w = p.w;
        *h = p.h;
    }

    SDL_UnlockMutex(p.lock);
    return shown;
}

void sdl_player_draw(SDL_Renderer *const r)
{
    static SDL_Texture *wrapper;
    static SDL_GPUTexture *wrapped;

    SDL_LockMutex(p.lock);

    if (wrapped != p.shown) {
        SDL_DestroyTexture(wrapper);
        wrapper = nullptr;
        wrapped = p.shown;

        if (p.retired)
            SDL_ReleaseGPUTexture(p.dev, p.retired);

        p.retired = nullptr;
    }

    if (p.shown && !wrapper) {
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, p.shown);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA32);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, p.w);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, p.h);
        wrapper = SDL_CreateTextureWithProperties(r, props);
        SDL_DestroyProperties(props);
        SDL_SetTextureScaleMode(wrapper, SDL_SCALEMODE_PIXELART);
        SDL_SetTextureBlendMode(wrapper, SDL_BLENDMODE_NONE);
    }

    if (p.showing && wrapper) {
        SDL_RenderTexture(r, wrapper, nullptr, nullptr);
        SDL_FlushRenderer(r);
    }

    SDL_UnlockMutex(p.lock);
}

bool sdl_player_pixels(uint8_t **const rgba, uint32_t *const w, uint32_t *const h)
{
    SDL_LockMutex(p.lock);
    const bool shown = p.showing && (*rgba = malloc((size_t)p.w * p.h * RGBA));

    if (shown) {
        *w = p.w;
        *h = p.h;
        download(p.shown, 0, 0, (int32_t)p.w, (int32_t)p.h, *rgba);
    }

    SDL_UnlockMutex(p.lock);
    return shown;
}
