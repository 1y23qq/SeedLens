#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum SLKind {
    SL_VILLAGE,
    SL_DESERT_TEMPLE,
    SL_JUNGLE_TEMPLE,
    SL_HUT,
    SL_IGLOO,
    SL_MONUMENT,
    SL_MANSION,
    SL_OUTPOST,
    SL_PORTAL,
    SL_PORTAL_N,
    SL_ANCIENT_CITY,
    SL_TREASURE,
    SL_MINESHAFT,
    SL_STRONGHOLD,
    SL_SHIPWRECK,
    SL_OCEAN_RUIN,
    SL_TRAIL_RUINS,
    SL_TRIAL_CHAMBERS,
    SL_CAMP,
    SL_FORTRESS,
    SL_BASTION,
    SL_NETHER_FOSSIL,
    SL_END_CITY,
    SL_GATEWAY,
    SL_END_ISLAND,
    SL_DESERT_WELL,
    SL_GEODE,
    SL_DUNGEON,
    SL_RAVINE,
    SL_LAVA_LAKE,
    SL_FOSSIL,
    SL_KIND_COUNT
};
enum {
    SL_ABANDONED = 1,
    SL_BASEMENT = 2,
    SL_GIANT = 4,
    SL_INNER_GATEWAY = 8,
    SL_APPROXIMATE = 16,
    SL_END_SHIP = 32,
    SL_DRIED_GHAST = 64
};
typedef struct {
    int id;
    const char* name;
} SLVersion;
typedef struct {
    int kind, x, y, z, biome;
    unsigned flags;
    int variant;
} SLMarker;
typedef int (*SLCancel)(void*);
typedef struct {
    const SLVersion* versions;
    int version_count;
    void* (*create)(int version, uint64_t seed, int dim, int large);
    void (*destroy)(void*);
    int (*generate)(void*, int scale, int x, int z, int w, int h, int y, int* out);
    int (*biome)(void*, int x, int y, int z);
    const char* (*biome_name)(int version, int id);
    int (*biome_exists)(int version, int id, int dim);
    int (*supports)(void*, int kind);
    int (*scan)(void*, int kind, int x0, int z0, int x1, int z1, SLMarker*, int cap, SLCancel, void*);
    int (*slime)(uint64_t seed, int chunk_x, int chunk_z);
    void (*spawn)(void*, int* x, int* z);
    void (*palette)(unsigned char colors[256][3]);
} SLBackend;

extern const SLBackend sl_java;
extern const SLBackend sl_bedrock;
#ifdef __cplusplus
}
#endif
