// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "cache.h"
#include "fetch.h"
#include "img.h"
#include "nor.h"

#define SECT(id) (img.sect[id].off)

// Logical offset of record k (sz bytes) of a table at byte base of section
// id. An index beyond the section is an engine or data error.
static uint32_t sect_rec(int id, uint32_t base, uint32_t sz, uint32_t k)
{
    const img_sect_t *s = &img.sect[id];
    if (base > s->size || k >= (s->size - base) / sz)
        dmc_fatal("index beyond its directory");
    return s->off + base + sz * k;
}

// ---------------------------------------------------------------- previous column

#define REUSE_SLOTS 4

typedef struct {
    const uint8_t *buf;
    int32_t key;     // what the buffer holds, -1 nothing
    uint16_t start;  // first row read
    uint16_t len;    // rows read, from start around the stored height
    int ticket;      // last read into the buffer, 0 when known to be done
} reuse_t;

static reuse_t reuse[REUSE_SLOTS];
static unsigned reuse_next;

void fetch_reset(void)
{
    memset(reuse, 0, sizeof(reuse));
}

static reuse_t *reuse_slot(const uint8_t *buf)
{
    for (int i = 0; i < REUSE_SLOTS; i++)
        if (reuse[i].buf == buf)
            return &reuse[i];
    reuse_t *r = &reuse[reuse_next++ % REUSE_SLOTS];
    r->buf = buf;
    r->key = -1;
    r->ticket = 0;
    return r;
}

// The buffer already holds the data: the ticket of the read that is still
// filling it, or 0.
static int reuse_ticket(reuse_t *r)
{
    if (r->ticket && nor_done(r->ticket))
        r->ticket = 0;
    return r->ticket;
}

void fetch_wait(int ticket)
{
    if (ticket)
        nor_wait(ticket);
}

// ---------------------------------------------------------------- textures

int tex_count(void)
{
    return rec_u16(SECT(DMC_SECT_TEXDIR));
}

void tex_entry(int tex, dmc_texent_t *e)
{
    rec_get(sect_rec(DMC_SECT_TEXDIR, 16, 16, (uint32_t)tex), e, 16);
}

int tex_col_request(int tex, int col, int level, int y0, int y1, uint8_t *buf)
{
    dmc_texent_t e;
    tex_entry(tex, &e);
    return tex_col_fetch(tex, &e, col, level, y0, y1, buf);
}

// Logical offset of column col (level units) of level `level`, *c receives
// the column masked to the stored width.
static uint32_t tex_col_off(const dmc_texent_t *e, int col, int level, unsigned *c)
{
    int base = tex_base(e);
    if (level < base || level >= base + e->nlevels)
        dmc_fatal("texture level not stored");
    *c = (unsigned)col & ((1u << (e->log2_w - level)) - 1u);
    return SECT(DMC_SECT_TEXDATA) + e->data + tex_level_off(e->log2_w, e->log2_sh, base, level) +
           (*c << (e->log2_sh - (unsigned)level));
}

int tex_col_fetch(int tex, const dmc_texent_t *e, int col, int level, int y0, int y1, uint8_t *buf)
{
    unsigned c;
    uint32_t off = tex_col_off(e, col, level, &c);
    unsigned n = 1u << (e->log2_sh - (unsigned)level);

    unsigned a = (unsigned)y0 & (n - 1), b = (unsigned)y1 & (n - 1);
    unsigned len = (b >= a ? b - a : n - a + b) + 1;
    if (y1 - y0 + 1 >= (int)n) {
        a = 0;
        len = n;
    }

    reuse_t *r = reuse_slot(buf);
    int32_t key = (int32_t)((uint32_t)tex << 14 | (uint32_t)level << 11 | c);
    if (r->key == key && ((a - r->start) & (n - 1)) + len <= r->len)
        return reuse_ticket(r);
    r->key = key;
    r->start = (uint16_t)a;
    r->len = (uint16_t)len;

    // requests of one priority complete in order, the last ticket covers both
    if (a + len <= n) {
        r->ticket = img_read_async(off + a, buf + a, len, NOR_PRIO_COL);
    } else {
        img_read_async(off + a, buf + a, n - a, NOR_PRIO_COL);
        r->ticket = img_read_async(off, buf, a + len - n, NOR_PRIO_COL);
    }
    return r->ticket;
}

int tex_masked_request(int tex, int col, uint8_t *buf, int *len)
{
    dmc_texent_t e;
    tex_entry(tex, &e);
    if (!(e.flags & DMC_TF_MASKED))
        dmc_fatal("texture not masked");
    unsigned c = ((unsigned)col & ((1u << e.log2_w) - 1u)) >> tex_base(&e);
    uint32_t base = SECT(DMC_SECT_TEXDATA);
    uint32_t ci = rec_u32(base + e.colinfo + 4u * c);
    unsigned n = DMC_COLINFO_LEN(ci);

    reuse_t *r = reuse_slot(buf);
    int32_t key = (int32_t)(0x40000000u | ((uint32_t)tex << 12) | c);
    *len = (int)n;
    if (r->key == key)
        return reuse_ticket(r);
    r->key = key;
    r->start = 0;
    r->len = (uint16_t)n;
    r->ticket = img_read_async(base + e.data + DMC_COLINFO_OFF(ci), buf, n, NOR_PRIO_COL);
    return r->ticket;
}

// ---------------------------------------------------------------- patches

uint32_t patch_loff(int patch)
{
    return SECT(DMC_SECT_PATCHDATA) + rec_u32(sect_rec(DMC_SECT_PATCHDIR, 4, 4, (uint32_t)patch));
}

void patch_header(int patch, dmc_patch_t *h)
{
    rec_get(patch_loff(patch), h, 16);
}

int spr_col_request(int patch, int col, uint8_t *buf, int *len)
{
    dmc_patch_t h;
    uint32_t p = patch_loff(patch);
    rec_get(p, &h, 16);
    unsigned k = (unsigned)col >> h.xshift;
    if (k >= h.ncols)
        dmc_fatal("patch column out of range");
    uint32_t ci = rec_u32(p + h.colinfo + 4u * k);
    unsigned n = DMC_COLINFO_LEN(ci);

    reuse_t *r = reuse_slot(buf);
    int32_t key = (int32_t)(0x20000000u | ((uint32_t)patch << 12) | k);
    *len = (int)n;
    if (r->key == key)
        return reuse_ticket(r);
    r->key = key;
    r->start = 0;
    r->len = (uint16_t)n;
    r->ticket = img_read_async(p + DMC_COLINFO_OFF(ci), buf, n, NOR_PRIO_COL);
    return r->ticket;
}

#ifdef DMC_HOST
uint32_t tex_col_peek(int tex, int col, int level)
{
    dmc_texent_t e;
    unsigned c;
    img_peek(sect_rec(DMC_SECT_TEXDIR, 16, 16, (uint32_t)tex), &e, 16);
    return tex_col_off(&e, col, level, &c);
}

uint32_t tex_masked_peek(int tex, int col, uint32_t *len)
{
    dmc_texent_t e;
    uint32_t ci;
    img_peek(sect_rec(DMC_SECT_TEXDIR, 16, 16, (uint32_t)tex), &e, 16);
    if (!(e.flags & DMC_TF_MASKED))
        dmc_fatal("texture not masked");
    unsigned c = ((unsigned)col & ((1u << e.log2_w) - 1u)) >> tex_base(&e);
    uint32_t base = SECT(DMC_SECT_TEXDATA);
    img_peek(base + e.colinfo + 4u * c, &ci, 4);
    *len = DMC_COLINFO_LEN(ci);
    return base + e.data + DMC_COLINFO_OFF(ci);
}

uint32_t spr_col_peek(int patch, int col, uint32_t *len)
{
    dmc_patch_t h;
    uint32_t p, ci;
    img_peek(sect_rec(DMC_SECT_PATCHDIR, 4, 4, (uint32_t)patch), &p, 4);
    p += SECT(DMC_SECT_PATCHDATA);
    img_peek(p, &h, 16);
    unsigned k = (unsigned)col >> h.xshift;
    if (k >= h.ncols)
        dmc_fatal("patch column out of range");
    img_peek(p + h.colinfo + 4u * k, &ci, 4);
    *len = DMC_COLINFO_LEN(ci);
    return p + DMC_COLINFO_OFF(ci);
}
#endif

int spr_count(void)
{
    return rec_u16(SECT(DMC_SECT_SPRDEF));
}

int spr_frame(int sprite, int frame, dmc_sprframe_t *f)
{
    dmc_sprite_t s;
    uint32_t base = SECT(DMC_SECT_SPRDEF);
    uint32_t frames = rec_u32(base + offsetof(dmc_sprdef_hdr_t, frames_off));
    if ((unsigned)sprite >= (unsigned)spr_count())
        dmc_fatal("sprite beyond SPRDEF");
    rec_get(base + 16u + 4u * (unsigned)sprite, &s, 4);
    if (frame >= s.numframes)
        return -1;
    rec_get(sect_rec(DMC_SECT_SPRDEF, frames, 16, s.first + (unsigned)frame), f, 16);
    return 0;
}

int ui_patch(int ui)
{
    if (ui < 0 || ui >= img.sect[DMC_SECT_UIDIR].flags)
        return -1;
    uint16_t p = rec_u16(SECT(DMC_SECT_UIDIR) + 2u * (unsigned)ui);
    return p == DMC_NONE16 ? -1 : p;
}

// ---------------------------------------------------------------- flats

#ifdef DMC_FW
static uint8_t (*flat_data)[DMC_FLAT_SIZE];
#else
static uint8_t flat_data[FLAT_SLOTS_MAX][DMC_FLAT_SIZE];
#endif
static uint16_t flat_tag[FLAT_SLOTS_MAX];
static uint8_t flat_age[FLAT_SLOTS_MAX];
static int flat_ticket[FLAT_SLOTS_MAX];
static int flat_slots = FLAT_SLOTS_MAX;
static int flat_cur = -1;  // slot flat_get returned last, its texels may be in use

int flat_count(void)
{
    return img.sect[DMC_SECT_FLATS].flags;
}

static void flat_check(int flat)
{
    if ((unsigned)flat >= (unsigned)flat_count())
        dmc_fatal("flat beyond FLATS");
}

void flat_reset(void)
{
    for (int i = 0; i < FLAT_SLOTS_MAX; i++) {
        fetch_wait(flat_ticket[i]);
        flat_tag[i] = DMC_NONE16;
        flat_age[i] = (uint8_t)i;
        flat_ticket[i] = 0;
    }
    flat_cur = -1;
}

#ifdef DMC_FW
void flat_place(void *base)
{
    flat_reset();
    flat_data = base;
}
#endif

void flat_set_slots(int k)
{
    flat_slots = k < 0 ? 0 : k > FLAT_SLOTS_MAX ? FLAT_SLOTS_MAX : k;
    flat_reset();
}

// Age 0 is the most recent use.
static void flat_touch(int s)
{
    for (int i = 0; i < flat_slots; i++)
        if (flat_age[i] < flat_age[s])
            flat_age[i]++;
    flat_age[s] = 0;
}

static int flat_find(int flat)
{
    for (int i = 0; i < flat_slots; i++)
        if (flat_tag[i] == flat)
            return i;
    return -1;
}

static int flat_loading(int s)
{
    if (flat_ticket[s] && nor_done(flat_ticket[s]))
        flat_ticket[s] = 0;
    return flat_ticket[s] != 0;
}

// Least recently used slot other than exclude, slots still loading only
// with busy set. -1 when there is none.
static int flat_victim(int exclude, int busy)
{
    int s = -1;
    for (int i = 0; i < flat_slots; i++)
        if (i != exclude && (busy || !flat_loading(i)) && (s < 0 || flat_age[i] > flat_age[s]))
            s = i;
    return s;
}

// A loaded slot counts as just used, so the next load takes another one.
static void flat_load(int s, int flat)
{
    fetch_wait(flat_ticket[s]);
    flat_tag[s] = (uint16_t)flat;
    flat_ticket[s] = img_read_async(SECT(DMC_SECT_FLATS) + DMC_FLAT_SIZE * (uint32_t)flat,
                                    flat_data[s], DMC_FLAT_SIZE, NOR_PRIO_FLAT);
    flat_touch(s);
}

const uint8_t *flat_get(int flat)
{
    if (!flat_slots)
        return NULL;
    flat_check(flat);
    int s = flat_find(flat);
    if (s < 0) {
        // the previous flat is done with, slots of prefetches are kept
        // unless every slot is loading
        s = flat_victim(-1, 0);
        if (s < 0)
            s = flat_victim(-1, 1);
        flat_load(s, flat);
    }
    fetch_wait(flat_ticket[s]);
    flat_ticket[s] = 0;
    flat_touch(s);
    flat_cur = s;
    return flat_data[s];
}

void flat_prefetch(int flat)
{
    flat_check(flat);
    if (flat_slots < 2 || flat_find(flat) >= 0)
        return;
    // never the slot in use or one still loading
    int s = flat_victim(flat_cur, 0);
    if (s >= 0)
        flat_load(s, flat);
}

uint8_t flat_avg(int flat)
{
    uint8_t v;
    flat_check(flat);
    rec_get(SECT(DMC_SECT_FLATS) + DMC_FLAT_SIZE * (uint32_t)flat_count() + (unsigned)flat, &v, 1);
    return v;
}

void flat_finale(uint32_t off, uint8_t *buf, uint32_t n)
{
    img_read(SECT(DMC_SECT_FLATS) + DMC_FINALE_FLAT_OFF(flat_count()) + off, buf, n);
}

// ---------------------------------------------------------------- other sections

void pal_read(int pal, uint16_t *rgb565)
{
    img_read(sect_rec(DMC_SECT_PALETTE, 0, 512, (uint32_t)pal), rgb565, 512);
}

const char *str_get(int id, char *buf, int max)
{
    uint32_t base = SECT(DMC_SECT_STRINGS);
    uint32_t end = base + img.sect[DMC_SECT_STRINGS].size;
    if ((unsigned)id >= rec_u16(base))
        dmc_fatal("string beyond STRINGS");
    uint32_t o = base + rec_u32(base + 4u + 4u * (unsigned)id);
    int n = 0;
    // whole cache lines would evict map records, so plain reads in chunks
    while (n < max - 1 && o < end) {
        int k = max - 1 - n;
        if (k > 32)
            k = 32;
        if ((uint32_t)k > end - o)
            k = (int)(end - o);
        img_read(o, buf + n, (uint32_t)k);
        for (int i = 0; i < k; i++)
            if (!buf[n + i])
                return buf;
        n += k;
        o += (uint32_t)k;
    }
    buf[n] = 0;
    return buf;
}

int str_part(int id, uint32_t off, char *buf, int n)
{
    uint32_t base = SECT(DMC_SECT_STRINGS);
    uint32_t end = base + img.sect[DMC_SECT_STRINGS].size;
    if ((unsigned)id >= rec_u16(base))
        dmc_fatal("string beyond STRINGS");
    uint32_t o = base + rec_u32(base + 4u + 4u * (unsigned)id) + off;
    if (o >= end)
        return 0;
    if ((uint32_t)n > end - o)
        n = (int)(end - o);
    img_read(o, buf, (uint32_t)n);
    for (int i = 0; i < n; i++)
        if (!buf[i])
            return i;
    return n;
}

int snd_get(int sfx, uint32_t *loff, uint16_t *len)
{
    dmc_sound_t s;
    uint32_t base = SECT(DMC_SECT_SOUNDS);
    if (sfx < 0 || sfx >= rec_u16(base))
        return -1;
    rec_get(base + 8u + 8u * (unsigned)sfx, &s, 8);
    *loff = base + s.off;
    *len = s.len;
    return s.len ? 0 : -1;
}

int anim_count(void)
{
    return img.sect[DMC_SECT_ANIMS].flags;
}

void anim_get(int k, dmc_anim_t *a)
{
    if ((unsigned)k >= (unsigned)anim_count())
        dmc_fatal("anim beyond ANIMS");
    rec_get(SECT(DMC_SECT_ANIMS) + 8u * (unsigned)k, a, 8);
}

// Texture numbers are global ids + 1
#define ANIM_RANGES 24
#define ANIM_FLAT 0x80
static struct {
    uint16_t first;  // texture or flat number of the first frame
    uint8_t frames;  // | ANIM_FLAT
    uint8_t offset;
} ranges[ANIM_RANGES];
static int nranges, past[2];  // past the last animated texture, flat

int anim_ranges(void)
{
    dmc_anim_t a;
    nranges = past[0] = past[1] = 0;
    for (int k = 0; k < anim_count(); k++) {
        anim_get(k, &a);
        if (a.kind == DMC_ANIM_SWITCH)
            continue;
        int flat = a.kind != DMC_ANIM_TEXTURE, n = a.last - a.first + 1;
        if (a.last <= a.first || n >= ANIM_FLAT || !a.speed || nranges == ANIM_RANGES)
            return -1;
        ranges[nranges].first = (uint16_t)(a.first + !flat);
        ranges[nranges].frames = (uint8_t)(n | (flat ? ANIM_FLAT : 0));
        ranges[nranges].offset = 0;
        if (past[flat] < a.last + !flat + 1)
            past[flat] = a.last + !flat + 1;
        nranges++;
    }
    return nranges;
}

void anim_offset(int i, int o)
{
    ranges[i].offset = (uint8_t)o;
}

int anim_frame(int n, int flat)
{
    if (n >= past[flat])
        return n;
    // a later cycle wins, as vanilla wrote its translation last
    for (int k = nranges - 1; k >= 0; k--) {
        unsigned frames = ranges[k].frames & ~ANIM_FLAT, i = (unsigned)(n - ranges[k].first);
        if ((ranges[k].frames >> 7) == (unsigned)flat && i < frames) {
            i += ranges[k].offset;
            return ranges[k].first + (int)(i < frames ? i : i - frames);
        }
    }
    return n;
}

int demo_get(int k, uint32_t *loff, uint32_t *size)
{
    uint32_t base = SECT(DMC_SECT_DEMOS);
    unsigned n = rec_u16(base);
    if (k < 0 || (unsigned)k >= n)
        return -1;
    uint32_t o = rec_u32(base + 4u + 4u * (unsigned)k);
    uint32_t e = (unsigned)k + 1 < n ? rec_u32(base + 8u + 4u * (unsigned)k) : img.sect[DMC_SECT_DEMOS].size;
    *loff = base + o;
    *size = e - o;
    return 0;
}
