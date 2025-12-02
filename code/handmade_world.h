#if !defined(HANDMADE_WORLD_H)
#include "handmade_math.h"
#include "types.h"

struct world_position {
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    // NOTE: These are the offsets from the chunk center
    v3 Offset_;
};

struct world_entity_block {
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block *Next;
};

struct world_chunk {
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    // TODO: Profile this and determine if a pointer would be better here
    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world {
    v3 ChunkDimInMeters;

    world_entity_block *FirstFree;

    world_chunk ChunkHash[4096];
};

#define HANDMADE_WORLD_H
#endif
