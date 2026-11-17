#include "backend.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
    Generator g;
    SurfaceNoise end_surface;
#if !SL_BEDROCK
    BlendedNoise nether_terrain;
#else
    TerrainNoise* terrain;
    TerrainShaper* shaper;
    CaveNoise* caves;
#endif
} SLContext;

static const SLVersion sl_versions[] = {
#if SL_BEDROCK
    {MC_26_60, "26.60 - 26.63 *"},
    {MC_26_50, "26.50 - 26.52"},
    {MC_26_40, "26.40 - 26.45 *"},
    {MC_26_30, "26.30 - 26.35 *"},
    {MC_26_20, "26.20 - 26.23"},
    {MC_1_21_60, "1.21.60+"},
    {MC_1_21_50, "1.21.50"},
    {MC_1_21, "1.21"},
    {MC_1_20, "1.20"},
    {MC_1_19, "1.19"},
    {MC_1_18, "1.18"},
    {MC_1_17_30, "1.17.30"},
    {MC_1_17, "1.17"},
    {MC_1_16, "1.16"},
    {MC_1_14, "1.14"},
    {MC_1_13, "1.13"},
    {MC_1_12, "1.12"},
    {MC_1_11, "1.11"},
    {MC_1_10, "1.10"},
    {MC_1_9, "1.9"},
    {MC_1_8, "1.8"},
    {MC_1_7, "1.7"},
    {MC_1_6, "1.6"},
    {MC_1_5, "1.5"},
    {MC_1_4, "1.4"},
    {MC_1_2, "1.2"},
    {MC_1_1, "1.1"},
    {MC_1_0, "1.0"}
#else
    {MC_26_3, "26.3"},
    {MC_26_2, "26.2"},
    {MC_26_1, "26.1"},
    {MC_1_21_11, "1.21.11"},
    {MC_1_21_9, "1.21.9 - 1.21.10"},
    {MC_1_21_6, "1.21.6 - 1.21.8"},
    {MC_1_21_5, "1.21.5"},
    {MC_1_21_4, "1.21.4"},
    {MC_1_21_3, "1.21.2 - 1.21.3"},
    {MC_1_21_1, "1.21 - 1.21.1"},
    {MC_1_20, "1.20.6"},
    {MC_1_19_4, "1.19.4"},
    {MC_1_19_2, "1.19.2"},
    {MC_1_18, "1.18.2"},
    {MC_1_17, "1.17.1"},
    {MC_1_16, "1.16.5"},
    {MC_1_16_1, "1.16.1"},
    {MC_1_15, "1.15.2"},
    {MC_1_14, "1.14.4"},
    {MC_1_13, "1.13.2"},
    {MC_1_12, "1.12.2"},
    {MC_1_11, "1.11.2"},
    {MC_1_10, "1.10.2"},
    {MC_1_9, "1.9.4"},
    {MC_1_8, "1.8.9"},
    {MC_1_7, "1.7.10"},
    {MC_1_6, "1.6.4"},
    {MC_1_5, "1.5.2"},
    {MC_1_4, "1.4.7"},
    {MC_1_3, "1.3.2"},
    {MC_1_2, "1.2.5"},
    {MC_1_1, "1.1"},
    {MC_1_0, "1.0"},
    {MC_B1_8, "Beta 1.8 (experimental)"},
    {MC_B1_7, "Beta 1.7 (experimental)"}
#endif
};

static int sl_native_kind(int k) {
    static const int types[SL_KIND_COUNT] = {
        Village,   Desert_Pyramid, Jungle_Temple,   Swamp_Hut,      Igloo,          Monument,  Mansion,
        Outpost,   Ruined_Portal,  Ruined_Portal_N, Ancient_City,   Treasure,       Mineshaft, Stronghold,
        Shipwreck, Ocean_Ruin,     Trail_Ruins,     Trial_Chambers, Abandoned_Camp, Fortress,  Bastion,
#if SL_BEDROCK
        -1,
#else
        Nether_Fossil,
#endif
        End_City,  End_Gateway,    End_Island,      Desert_Well,    Geode,
#if SL_BEDROCK
        Dungeon,   Ravine,         Lava_Lake,
#else
        -1, -1, -1,
#endif
        -1};
    return k >= 0 && k < SL_KIND_COUNT ? types[k] : -1;
}

static void* sl_create(int version, uint64_t seed, int dim, int large) {
    int valid = 0;
    for (unsigned i = 0; i < sizeof(sl_versions) / sizeof(*sl_versions); ++i)
        if (sl_versions[i].id == version)
            valid = 1;
    if (!valid || (dim != -1 && dim != 0 && dim != 1))
        return NULL;
#if !SL_BEDROCK
    if (dim == DIM_END && version < MC_1_0)
        return NULL;
#endif
    SLContext* ctx = (SLContext*)calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    setupGenerator(&ctx->g, version, large && !SL_BEDROCK ? LARGE_BIOMES : 0);
    applySeed(&ctx->g, dim, seed);
    if (dim == DIM_END)
        initSurfaceNoise(&ctx->end_surface, DIM_END, seed);
#if !SL_BEDROCK
    if (dim == DIM_NETHER)
        initBlendedNoise(&ctx->nether_terrain, seed, DIM_NETHER);
#endif
    return ctx;
}

static void sl_destroy(void* ctx) {
#if SL_BEDROCK
    SLContext* c = (SLContext*)ctx;
    if (c) {
        free(c->terrain);
        free(c->shaper);
        free(c->caves);
    }
#endif
    free(ctx);
}

static int sl_generate(void* ctx, int scale, int x, int z, int w, int h, int y, int* out) {
    if (!ctx || !out || w <= 0 || h <= 0 || w > 4096 || h > 4096)
        return -1;
    if (scale != 1 && scale != 4 && scale != 16 && scale != 64 && scale != 256)
        return -1;
    SLContext* c = (SLContext*)ctx;
    Range r = {scale, x, z, w, h, scale == 1 ? y : (int)floor(y / 4.0), 1};
    int* cache = allocCache(&c->g, r);
    if (!cache)
        return -1;
    int result = genBiomes(&c->g, cache, r);
    if (!result)
        memcpy(out, cache, (size_t)w * h * sizeof(int));
    free(cache);
    return result;
}

static int sl_biome(void* ctx, int x, int y, int z) {
    return getBiomeAt(&((SLContext*)ctx)->g, 1, x, y, z);
}

static int sl_biome_exists(int version, int id, int dim) {
    return biomeExists(version, id) && getDimension(id) == dim;
}

static int sl_supports(void* ctx, int kind) {
    SLContext* c = (SLContext*)ctx;
    int type = sl_native_kind(kind);
    if (type < 0)
        return 0;
#if !SL_BEDROCK
    if (kind == SL_STRONGHOLD)
        return c->g.dim == DIM_OVERWORLD && c->g.mc >= MC_B1_8;
#endif
    StructureConfig sc;
    return getStructureConfig(type, c->g.mc, &sc) && sc.dim == c->g.dim;
}

static void sl_fill(SLContext* c, int kind, Pos p, SLMarker* m) {
    memset(m, 0, sizeof(*m));
    m->kind = kind;
    m->x = p.x;
    m->z = p.z;
    m->y = INT_MIN;
    m->biome = -1;
    if (kind == SL_ANCIENT_CITY)
        m->y = -51;
    if (kind == SL_STRONGHOLD || kind == SL_MINESHAFT || kind == SL_GATEWAY || kind == SL_NETHER_FOSSIL)
        return;
    int type = sl_native_kind(kind);
    StructureVariant sv;
    memset(&sv, 0, sizeof(sv));
    int b = getBiomeAt(&c->g, 1, p.x, kind == SL_ANCIENT_CITY ? -51 : 320, p.z);
    m->biome = b;
    if (getVariant(&sv, type, c->g.mc, c->g.seed, p.x, p.z, b)) {
        if (sv.abandoned)
            m->flags |= SL_ABANDONED;
        if (sv.basement)
            m->flags |= SL_BASEMENT;
        if (sv.giant)
            m->flags |= SL_GIANT;
        m->variant = sv.start;
        if (kind == SL_TRIAL_CHAMBERS || kind == SL_GEODE) {
            m->y = sv.y;
            m->biome = getBiomeAt(&c->g, 1, p.x, sv.y, p.z);
        }
    }
    if (kind == SL_END_CITY) {
        Piece* pieces = (Piece*)malloc(END_CITY_PIECES_MAX * sizeof(Piece));
        if (pieces) {
            int count = getEndCityPieces(pieces, c->g.seed, p.x >> 4, p.z >> 4);
            for (int i = 0; i < count; ++i)
                if (pieces[i].type == END_SHIP)
                    m->flags |= SL_END_SHIP;
            free(pieces);
        }
    }
}

static int sl_inside(Pos p, int x0, int z0, int x1, int z1) {
    return p.x >= x0 && p.x <= x1 && p.z >= z0 && p.z <= z1;
}

static int sl_scan(void* ctx, int kind, int x0, int z0, int x1, int z1, SLMarker* out, int cap,
                   SLCancel cancel, void* user) {
    SLContext* c = (SLContext*)ctx;
    if (!out || cap <= 0 || !sl_supports(ctx, kind))
        return 0;
    int n = 0, type = sl_native_kind(kind);
    if (kind == SL_STRONGHOLD) {
        StrongholdIter sh;
        memset(&sh, 0, sizeof(sh));
#if SL_BEDROCK
        for (int i = 0; i < 3; ++i) {
            if (cancel && cancel(user))
                return n;
            nextVillageStronghold(&sh, &c->g);
            if (sl_inside(sh.pos, x0, z0, x1, z1) && n < cap)
                sl_fill(c, kind, sh.pos, &out[n++]);
        }
#else
        initFirstStronghold(&sh, c->g.mc, c->g.seed);
        int remaining;
        do {
            if (cancel && cancel(user))
                return n;
            remaining = nextStronghold(&sh, &c->g);
            if (sl_inside(sh.pos, x0, z0, x1, z1) && n < cap)
                sl_fill(c, kind, sh.pos, &out[n++]);
        } while (remaining > 0 && n < cap);
        return n;
#endif
    }
    if (kind == SL_GATEWAY) {
        Pos fixed[20];
        getFixedEndGateways(c->g.mc, c->g.seed, fixed);
        for (int i = 0; i < 20 && n < cap; ++i) {
            if (!sl_inside(fixed[i], x0, z0, x1, z1))
                continue;
            sl_fill(c, kind, fixed[i], &out[n]);
            out[n].y = 75;
            out[n++].flags |= SL_INNER_GATEWAY;
        }
    }
    StructureConfig sc;
    if (!getStructureConfig(type, c->g.mc, &sc))
        return n;
    int span = sc.regionSize * 16;
    int rx0 = (int)floor(x0 / (double)span), rz0 = (int)floor(z0 / (double)span);
    int rx1 = (int)floor(x1 / (double)span), rz1 = (int)floor(z1 / (double)span);
    for (int rz = rz0; rz <= rz1 && n < cap; ++rz) {
        if (cancel && cancel(user))
            break;
        for (int rx = rx0; rx <= rx1 && n < cap; ++rx) {
            if (cancel && cancel(user))
                return n;
            Pos p;
            if (kind == SL_END_ISLAND) {
                if (getBiomeAt(&c->g, 16, rx, 0, rz) != small_end_islands)
                    continue;
                EndIsland islands[2];
                int count = getEndIslands(islands, c->g.mc, c->g.seed, rx, rz);
                for (int i = 0; i < count && n < cap; ++i) {
                    p.x = islands[i].x;
                    p.z = islands[i].z;
                    if (!sl_inside(p, x0, z0, x1, z1))
                        continue;
                    memset(&out[n], 0, sizeof(*out));
                    out[n].kind = kind;
                    out[n].x = p.x;
                    out[n].z = p.z;
                    out[n].y = islands[i].y;
                    out[n++].biome = small_end_islands;
                }
                continue;
            }
#if SL_BEDROCK
            if (kind == SL_DUNGEON) {
                if (x1 - x0 > 256 || z1 - z0 > 256)
                    return -2;
                if (!c->terrain) {
                    c->terrain = (TerrainNoise*)calloc(1, sizeof(TerrainNoise));
                    c->shaper = (TerrainShaper*)calloc(1, sizeof(TerrainShaper));
                    c->caves = (CaveNoise*)calloc(1, sizeof(CaveNoise));
                    if (!c->terrain || !c->shaper || !c->caves)
                        return -1;
                    initTerrainNoise(c->terrain, c->g.seed);
                    initTerrainShaper(c->shaper);
                    initCaveNoise(c->caves, c->g.seed);
                }
                DungeonData data[DUNGEON_MAX_PER_CHUNK];
                int count = getDungeons(&c->g.bn, c->terrain, c->shaper, c->caves, c->g.mc, c->g.seed, rx, rz,
                                        data, DUNGEON_MAX_PER_CHUNK);
                for (int i = 0; i < count && i < DUNGEON_MAX_PER_CHUNK && n < cap; ++i) {
                    p.x = data[i].pos.x;
                    p.z = data[i].pos.z;
                    if (!sl_inside(p, x0, z0, x1, z1))
                        continue;
                    memset(&out[n], 0, sizeof(*out));
                    out[n].kind = kind;
                    out[n].x = p.x;
                    out[n].z = p.z;
                    out[n].y = data[i].pos.y;
                    out[n].biome = -1;
                    out[n++].variant = data[i].mobType;
                }
                continue;
            }
            if (kind == SL_GATEWAY) {
                if (!getEndGatewayPos(c->g.seed, c->g.en, c->end_surface, rx, rz, &p))
                    continue;
            } else
#endif
                if (!getStructurePos(type, c->g.mc, c->g.seed, rx, rz, &p))
                continue;
            if (!sl_inside(p, x0, z0, x1, z1))
                continue;
#if !SL_BEDROCK
            if (kind == SL_NETHER_FOSSIL) {
                StructureVariant sv;
                memset(&sv, 0, sizeof(sv));
                if (!getVariant(&sv, Nether_Fossil, c->g.mc, c->g.seed, p.x, p.z, soul_sand_valley))
                    continue;
                if (!isViableNetherFossilTerrain(p.x >> 4, p.z >> 4, &sv, &c->nether_terrain, c->g.mc))
                    continue;
                int px = p.x + sv.x, pz = p.z + sv.z;
                if (getBiomeAt(&c->g, 1, px, sv.y, pz) != soul_sand_valley)
                    continue;
                sl_fill(c, kind, p, &out[n]);
                out[n].x = px;
                out[n].z = pz;
                out[n].y = sv.y;
                out[n].biome = soul_sand_valley;
                if (c->g.mc >= MC_1_21_6 && netherFossilHasGhast(p.x >> 4, p.z >> 4, c->g.seed, &sv))
                    out[n].flags |= SL_DRIED_GHAST;
                if (sl_inside((Pos){px, pz}, x0, z0, x1, z1))
                    ++n;
                continue;
            }
#endif
            if (!isViableStructurePos(type, &c->g, p.x, p.z, 0))
                continue;
            if ((kind == SL_DESERT_TEMPLE || kind == SL_JUNGLE_TEMPLE || kind == SL_MANSION) &&
                !isViableStructureTerrain(type, &c->g, p.x, p.z))
                continue;
            if (kind == SL_END_CITY && !isViableEndCityTerrain(&c->g, &c->end_surface, p.x, p.z))
                continue;
            int duplicate = 0;
            if (kind == SL_STRONGHOLD) {
                for (int i = 0; i < n; ++i)
                    if (out[i].x == p.x && out[i].z == p.z)
                        duplicate = 1;
            }
            if (!duplicate)
                sl_fill(c, kind, p, &out[n++]);
        }
    }
    return n;
}

static int sl_slime(uint64_t seed, int x, int z) {
#if SL_BEDROCK
    (void)seed;
    return isSlimeChunk(x, z);
#else
    return isSlimeChunk(seed, x, z);
#endif
}

static void sl_spawn(void* ctx, int* x, int* z) {
    SLContext* c = (SLContext*)ctx;
    SLContext* overworld = c;
    if (c->g.dim != DIM_OVERWORLD)
        overworld = (SLContext*)sl_create(c->g.mc, c->g.seed, DIM_OVERWORLD, c->g.flags & LARGE_BIOMES);
    Pos p = estimateSpawn(&overworld->g, NULL);
    *x = p.x;
    *z = p.z;
    if (overworld != c)
        sl_destroy(overworld);
}

const SLBackend SL_BACKEND_NAME = {sl_versions,    sizeof(sl_versions) / sizeof(*sl_versions),
                                   sl_create,      sl_destroy,
                                   sl_generate,    sl_biome,
                                   biome2str,      sl_biome_exists,
                                   sl_supports,    sl_scan,
                                   sl_slime,       sl_spawn,
                                   initBiomeColors};
