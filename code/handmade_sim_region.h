#if !defined(HANDMADE_SIM_REGION_H)

#include "handmade_math.h"
#include "handmade_world.h"
#include "types.h"

struct move_spec {
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point {
    uint8 Flags;
    uint8 FilledAmount;
};

enum entity_type {
    EntityType_Null,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword,
};

struct sim_entity;
union entity_reference {
    sim_entity *Ptr;
    uint32 Index;
};

enum sim_entity_flags {
    EntityFlag_Collides = (1 << 0),
    EntityFlag_NonSpatial = (1 << 1),

    EntityFlag_Simming = (1 << 30),
};

struct sim_entity {
    uint32 StorageIndex;
    bool32 Updatable;

    entity_type Type;
    uint32 Flags;

    v2 P;
    v2 dP;

    uint32 ChunkZ;

    real32 Z;
    real32 dZ;
    real32 DistanceLimit;

    real32 Width, Height;

    uint32 FacingDirection;
    real32 tBob;

    int32 dAbsTileZ;

    // TODO: Should hitpoints be entities hrmmm?
    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;
};

struct sim_entity_hash {
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_region {
    world *World;

    world_position Origin;
    rectangle2 Bounds;
    rectangle2 UpdatableBounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity *Entities;

    // TODO: Do I really want the world to marvel at my amazing hash function?
    sim_entity_hash Hash[4096];
};

#define HANDMADE_SIM_REGION_H
#endif
