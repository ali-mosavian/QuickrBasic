/* GPU lists (qx/qxlist.h) played with SDL_GPU, and the texture they show drawn in the window.
 *
 * # How it works
 * The window's renderer is SDL's GPU renderer; the player shares its device. A list plays on the
 * calling thread, the interpreter's, as one command buffer submitted without a wait: a play waits
 * only for the frame FLIGHT back, a read for its own. The list's writes and uploads are first
 * copied into one staging buffer kept between frames, cycled while a frame behind still reads it,
 * so the list's memory is free when play returns. Then each op in turn, a render pass held open
 * across draws and begun again after a copy. A buffer written is cycled; a texture written in part
 * is not, SDL ordering the write after the frames that read it. Every texture is RGBA8 or R8 and a
 * colour target, so TEXTURE clears it by a pass; a pipeline draws into RGBA8, the draw's alpha
 * its blend constant. SHOW copies a texture to the one the window's thread shows, cycling it: the
 * copy goes to a fresh one while a frame still samples the last. The window's thread wraps it for
 * its renderer and alone releases it; it binds it under the lock SHOW takes, so the cycle sees the
 * binding.
 */
#include <stdlib.h>
#include <string.h>
#include "plat/pgfx.h"
#include "qx/qx.h"
#include "qx/qxlist.h"
#include "sdl.h"

enum {
    RGBA = 4,
    BLENDS = 3,
    FLIGHT = 2,        /* frames the GPU may be behind */
    ALIGN = 16,        /* a staged block's offset */
    DRAW_TEXTURES = 6, /* a DRAW's first texture, its argument */
    CODE_WORDS = 3,
    ATTRIBUTE_WORDS = 3,
    PIPELINE_HEAD = 4 + 2 * CODE_WORDS, /* a PIPELINE's first attribute, its word, the op's after */
    MOST_ATTRIBUTES = 16,
    MOST_SAMPLERS = 8,
};

/* Each kept by the list's id, which comes first. */
typedef struct {
    uint32_t id, w, h, bpp;
    SDL_GPUTexture *t;
} Texture;

typedef struct {
    uint32_t id, cap; /* bytes */
    SDL_GPUBuffer *b;
} Buffer;

typedef struct {
    uint32_t id, samplers;
    SDL_GPUGraphicsPipeline *p;
} Pipeline;

typedef struct {
    void *at;
    uint32_t n, size;
} Table;

/* What the open render pass draws with. */
typedef struct {
    SDL_GPUCommandBuffer *cb;
    SDL_GPURenderPass *render;
    SDL_GPUCopyPass *copy;
    uint32_t target; /* the texture drawn into, while aimed */
    bool aimed;
    SDL_Rect clip;
    uint32_t pipeline, vertices, indices; /* bound; 0 none */
    bool data;                            /* the staging buffer holds the list's data */
    uint32_t staged;                      /* the op's data, its offset there */
} Pass;

static struct {
    SDL_GPUDevice *dev;
    SDL_GPUSampler *nearest;
    Table textures, buffers, pipelines;
    SDL_GPUTransferBuffer *staging;
    uint32_t capstaging;
    SDL_GPUFence *fences[FLIGHT];
    uint32_t frame;
    SDL_Mutex *lock;         /* over the shown texture */
    SDL_GPUTexture *shown;   /* what was last shown */
    SDL_GPUTexture *retired; /* a shown texture the window's thread is to release */
    uint32_t w, h;
    bool showing; /* shown since open */
} p;

/* t's item with id: null, none. */
static void *find(const Table *const t, const uint32_t id)
{
    for (uint32_t i = 0; i < t->n; i++) {
        uint8_t *const item = (uint8_t *)t->at + i * t->size;

        if (*(const uint32_t *)item == id)
            return item;
    }

    return nullptr;
}

/* A new item with id on t, zeroed but its id: null, no memory. */
static void *added(Table *const t, const uint32_t id)
{
    uint8_t *const grown = SDL_realloc(t->at, (t->n + 1) * t->size);

    if (!grown)
        return nullptr;

    t->at = grown;
    uint8_t *const item = grown + t->n++ * t->size;
    memset(item, 0, t->size);
    *(uint32_t *)item = id;
    return item;
}

/* item, on t, taken off it: the last moved to its place. */
static void dropped(Table *const t, void *const item)
{
    memmove(item, (uint8_t *)t->at + --t->n * t->size, t->size);
}

static Texture *texture(const uint32_t id) { return find(&p.textures, id); }

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
    s->pipeline = s->vertices = s->indices = 0;
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
    const bool index = format == QX_TEXTURE_INDEX;
    SDL_GPUTexture *const made =
        make(w, h, index ? SDL_GPU_TEXTUREFORMAT_R8_UNORM : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    Texture *const t = made ? added(&p.textures, id) : nullptr;

    if (!t) {
        SDL_ReleaseGPUTexture(p.dev, made);
        return;
    }

    *t = (Texture){id, w, h, index ? 1 : RGBA, made};

    cleared(s, made);
}

static void drop(const uint32_t id)
{
    Texture *const t = texture(id);

    if (!t)
        return;

    SDL_ReleaseGPUTexture(p.dev, t->t);
    dropped(&p.textures, t);
}

/* Buffer id at least bytes of kind, made again twice that when smaller. */
static void buffer(const uint32_t id, const uint32_t bytes, const uint32_t kind)
{
    Buffer *const had = find(&p.buffers, id);
    Buffer *const b = had ? had : added(&p.buffers, id);

    if (!b || bytes <= b->cap)
        return;

    SDL_ReleaseGPUBuffer(p.dev, b->b);
    b->cap = bytes * 2;
    b->b = SDL_CreateGPUBuffer(
        p.dev,
        &(SDL_GPUBufferCreateInfo){
            .usage =
                kind == QX_BUFFER_INDEX ? SDL_GPU_BUFFERUSAGE_INDEX : SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = b->cap,
        }

    );

    if (!b->b)
        b->cap = 0;
}

static const void *pointer(const uint32_t *const a)
{
    return (const void *)(uintptr_t)((uint64_t)a[1] << 32 | a[0]);
}

/* The bytes op's data takes staged, to ALIGN; copied to to unless null. */
static uint32_t staged(const uint32_t *const op, uint8_t *const to)
{
    const uint32_t *const a = op + 1;
    const QxListOp code = (QxListOp)(op[0] & 0xFF);
    const uint32_t bytes = code == QX_LIST_WRITE ? a[1] : code == QX_LIST_UPLOAD ? a[4] * a[5] : 0;

    if (to && bytes)
        memcpy(to, pointer(code == QX_LIST_WRITE ? a + 2 : a + 6), bytes);

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

/* The op's bytes from the staging buffer to buffer id's first. */
static void write_buffer(Pass *const s, const uint32_t id, const uint32_t bytes)
{
    const Buffer *const b = find(&p.buffers, id);

    if (!bytes || !s->data || !b || !b->b || bytes > b->cap)
        return;

    SDL_UploadToGPUBuffer(
        copying(s),
        &(SDL_GPUTransferBufferLocation){.transfer_buffer = p.staging, .offset = s->staged},
        &(SDL_GPUBufferRegion){.buffer = b->b, .size = bytes},
        true
    );
}

/* The op's rows from the staging buffer into region a (x, y, w, h, pitch) of texture id. */
static void upload(Pass *const s, const uint32_t *const a)
{
    const Texture *const t = texture(a[0]);

    if (!s->data || !t)
        return;

    SDL_UploadToGPUTexture(
        copying(s),
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = p.staging,
            .offset = s->staged,
            .pixels_per_row = a[5] / t->bpp,
        },
        &(SDL_GPUTextureRegion){
            .texture = t->t,
            .x = a[1],
            .y = a[2],
            .w = a[3],
            .h = a[4],
            .d = 1,
        },
        false
    );
}

static SDL_GPUShader *shader(
    const uint32_t *const code,
    const SDL_GPUShaderStage stage,
    const uint32_t samplers,
    const uint32_t uniforms
)
{
    return SDL_CreateGPUShader(
        p.dev,
        &(SDL_GPUShaderCreateInfo){
            .code_size = code[0],
            .code = pointer(code + 1),
            .entrypoint = "main0",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = stage,
            .num_samplers = samplers,
            .num_uniform_buffers = uniforms,
        }

    );
}

/* PIPELINE's a, n words: id, blend, stride, samplers, its code and attributes. */
static void pipeline(const uint32_t *const a, const uint32_t n)
{
    static const SDL_GPUVertexElementFormat formats[] = {
        [QX_ATTRIBUTE_FLOAT2] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        [QX_ATTRIBUTE_FLOAT4] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        [QX_ATTRIBUTE_USHORT4] = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4,
        [QX_ATTRIBUTE_UBYTE4] = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4,
    };

    static const SDL_GPUColorTargetBlendState blends[BLENDS] = {
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

    const uint32_t count = (n - PIPELINE_HEAD) / ATTRIBUTE_WORDS, samplers = a[3];
    SDL_GPUVertexAttribute attributes[MOST_ATTRIBUTES];

    if (n < PIPELINE_HEAD || a[1] >= BLENDS || count > MOST_ATTRIBUTES ||
        samplers > MOST_SAMPLERS || find(&p.pipelines, a[0]))
        return;

    for (uint32_t i = 0; i < count; i++) {
        const uint32_t *const t = a + PIPELINE_HEAD + i * ATTRIBUTE_WORDS;
        const uint32_t format = t[1] < sizeof formats / sizeof *formats ? t[1] : 0;
        attributes[i] = (SDL_GPUVertexAttribute){t[0], 0, formats[format], t[2]};
    }

    SDL_GPUShader *const vs = shader(a + 4, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader *const fs = shader(a + 4 + CODE_WORDS, SDL_GPU_SHADERSTAGE_FRAGMENT, samplers, 0);
    SDL_GPUGraphicsPipeline *const made =
        vs && fs ? SDL_CreateGPUGraphicsPipeline(
                       p.dev,
                       &(SDL_GPUGraphicsPipelineCreateInfo){
                           .vertex_shader = vs,
                           .fragment_shader = fs,
                           .vertex_input_state =
                               {
                                   .vertex_buffer_descriptions =
                                       &(SDL_GPUVertexBufferDescription){.pitch = a[2]},
                                   .num_vertex_buffers = 1,
                                   .vertex_attributes = attributes,
                                   .num_vertex_attributes = count,
                               },
                           .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                           .target_info =
                               {
                                   .color_target_descriptions =
                                       &(SDL_GPUColorTargetDescription){
                                           .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                           .blend_state = blends[a[1]],
                                       },
                                   .num_color_targets = 1,
                               },
                       }

                   )
                 : nullptr;

    SDL_ReleaseGPUShader(p.dev, vs);
    SDL_ReleaseGPUShader(p.dev, fs);

    Pipeline *const kept = made ? added(&p.pipelines, a[0]) : nullptr;

    if (kept)
        *kept = (Pipeline){a[0], samplers, made};
    else
        SDL_ReleaseGPUGraphicsPipeline(p.dev, made);
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

    SDL_SetGPUViewport(s->render, &(SDL_GPUViewport){0, 0, (float)t->w, (float)t->h, 0, 1});

    SDL_SetGPUScissor(s->render, &s->clip);
    return s->render;
}

/* DRAW's a, n words: pipeline, vertices, indices, first, count, alpha, textures. */
static void draw(Pass *const s, const uint32_t *const a, const uint32_t n)
{
    const Pipeline *const pl = find(&p.pipelines, a[0]);
    const Buffer *const vb = find(&p.buffers, a[1]);
    const Buffer *const ib = find(&p.buffers, a[2]);
    const Texture *const target = s->aimed ? texture(s->target) : nullptr;
    SDL_GPUTextureSamplerBinding samplers[MOST_SAMPLERS];

    if (!pl || !vb || !vb->b || !ib || !ib->b || !target || n != DRAW_TEXTURES + pl->samplers ||
        (uint64_t)(a[3] + a[4]) * sizeof(uint32_t) > ib->cap)
        return;

    for (uint32_t i = 0; i < pl->samplers; i++) {
        const Texture *const t = texture(a[DRAW_TEXTURES + i]);

        if (!t)
            return;

        samplers[i] = (SDL_GPUTextureSamplerBinding){t->t, p.nearest};
    }

    SDL_GPURenderPass *const r = drawing(s, target);
    const float alpha = (float)a[5] / 255;

    if (s->pipeline != a[0])
        SDL_BindGPUGraphicsPipeline(r, pl->p);

    if (s->vertices != a[1])
        SDL_BindGPUVertexBuffers(r, 0, &(SDL_GPUBufferBinding){.buffer = vb->b}, 1);

    if (s->indices != a[2])
        SDL_BindGPUIndexBuffer(
            r,
            &(SDL_GPUBufferBinding){.buffer = ib->b},
            SDL_GPU_INDEXELEMENTSIZE_32BIT
        );

    s->pipeline = a[0];
    s->vertices = a[1];
    s->indices = a[2];
    SDL_SetGPUBlendConstants(r, (SDL_FColor){alpha, alpha, alpha, alpha});

    SDL_BindGPUFragmentSamplers(r, 0, samplers, pl->samplers);
    SDL_DrawGPUIndexedPrimitives(r, a[4], 1, a[3], 0, 0);
}

/* Texture id copied to the one the window shows. */
static void show(Pass *const s, const uint32_t id)
{
    const Texture *const t = texture(id);

    if (!t)
        return;

    SDL_LockMutex(p.lock);
    SDL_CopyGPUTextureToTexture(
        copying(s),
        &(SDL_GPUTextureLocation){.texture = t->t},
        &(SDL_GPUTextureLocation){.texture = p.shown},
        t->w < p.w ? t->w : p.w,
        t->h < p.h ? t->h : p.h,
        1,
        true
    );

    p.showing = true;
    SDL_UnlockMutex(p.lock);
}

static void player_play(const uint32_t *const list, const uint32_t words)
{
    Pass s = {.cb = SDL_AcquireGPUCommandBuffer(p.dev)};

    if (!s.cb)
        return;

    s.data = staging(list, words);

    for (uint32_t at = 0, size; at < words && (size = list[at] >> QX_LIST_SIZE); at += size) {
        const uint32_t *const a = list + at + 1;
        const uint32_t n = size - 1;

        switch ((QxListOp)(list[at] & 0xFF)) {
        case QX_LIST_BUFFER:
            buffer(a[0], a[1], a[2]);
            break;

        case QX_LIST_WRITE:
            write_buffer(&s, a[0], a[1]);
            break;

        case QX_LIST_TEXTURE:
            add(&s, a[0], a[1], a[2], a[3]);
            break;

        case QX_LIST_FREE:
            end(&s);
            drop(a[0]);
            break;

        case QX_LIST_UPLOAD:
            upload(&s, a);
            break;

        case QX_LIST_PIPELINE:
            pipeline(a, n);
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

        case QX_LIST_UNIFORMS:
            SDL_PushGPUVertexUniformData(s.cb, 0, pointer(a + 1), a[0]);
            break;

        case QX_LIST_DRAW:
            draw(&s, a, n);
            break;

        case QX_LIST_SHOW:
            show(&s, a[0]);
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
    Pass s = {.cb = cb};

    if (!cb || !shown)
        return false;

    p.w = w;
    p.h = h;
    cleared(&s, shown);
    end(&s);
    finish(cb);

    SDL_LockMutex(p.lock);
    p.shown = shown;
    p.showing = false;
    SDL_UnlockMutex(p.lock);
    return true;
}

/* The screen's textures, buffers and pipelines released. */
static void player_close(void)
{
    settle();

    while (p.textures.n)
        drop(((const Texture *)p.textures.at)->id);

    for (uint32_t i = 0; i < p.buffers.n; i++)
        SDL_ReleaseGPUBuffer(p.dev, ((const Buffer *)p.buffers.at)[i].b);

    for (uint32_t i = 0; i < p.pipelines.n; i++)
        SDL_ReleaseGPUGraphicsPipeline(p.dev, ((const Pipeline *)p.pipelines.at)[i].p);

    p.buffers.n = p.pipelines.n = 0;

    SDL_LockMutex(p.lock);
    p.retired = p.shown;
    p.shown = nullptr;
    p.showing = false;
    SDL_UnlockMutex(p.lock);
}

bool sdl_player_init(SDL_GPUDevice *const dev)
{
    static const PgfxPlayer player = {
        .shaders = 1 << QX_SHADER_MSL,
        .y_down = false,
        .open = player_open,
        .close = player_close,
        .play = player_play,
        .read = player_read,
    };

    p.dev = dev;
    p.textures.size = sizeof(Texture);
    p.buffers.size = sizeof(Buffer);
    p.pipelines.size = sizeof(Pipeline);

    if (!(SDL_GetGPUShaderFormats(dev) & SDL_GPU_SHADERFORMAT_MSL))
        return false;

    p.lock = SDL_CreateMutex();
    p.nearest = SDL_CreateGPUSampler(dev, &(SDL_GPUSamplerCreateInfo){});

    if (!p.lock || !p.nearest)
        return false;

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
