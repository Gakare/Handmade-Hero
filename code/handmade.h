#if !defined(HANDMADE_H)

/* TODO:
    - Rendering
        - Lighting
        - Straighten out all coordinate systems!
            - Screen
            - World
            - Texture
        - Optimization

    ARCHITECTURE EXPLORATION
    - Z!
        - Figure out how you go "up" and "down", and how is this rendered?
        - Go through and define how tall everything should be

    - Collision detection
        - Clean up predicate proliferation
        - Entry/exit
        - What's the plan for robustness / shape definition?

    - Implement multiple sims regions per frame
        - Per-entity clocking
        - Sim region merging? For multiple players?

    - Debugging
        - Logging
        - Diagramming
        - (A LITTLE GUI, but a tiny bit) Switchers / sliders /etc.
        - Draw tile chunks so we can verify that things are aligned / in the chunks

    - Metagame / save games?
        - Should we allow saved games? Just for "pausing"
        - Continuous save for crash recovery?

    - Audio
        - Sound effect triggers
        - Ambient Sounds
        - Music
    - Asset streaming

    - Rudimentary world gen
        - Placement of background things
        - Connectivity
        - Non-overlapping
        - Map Display
            - Magnets - que?
    - AI
        - Pathfinding
        - AI "storage"

    - Animation, should probably lead into rendering
        - Skeletal animation
        - Particle systems

    PRODUCTION
    -> Game
        - Entity system
        - World generation

*/

#include "types.h"
#include "handmade_platform.h"
#include "handmade_sim_region.h"
#include "handmade_world.h"

#define Pi32 3.14159265359f

#if HANDMADE_SLOW
#define Assert(Expression)                                                               \
    if (!(Expression)) {                                                                 \
        *(volatile int *)0 = 0;                                                          \
    }
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase                                                               \
    default: {                                                                           \
        InvalidCodePath;                                                                 \
    } break

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)
#define Terabytes(Value) (Gigabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

inline uint32 SafeTruncateUInt64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return (Result);
}

inline game_controller_input *GetController(game_input *Input, int ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return (Result);
}

struct memory_arena {
    memory_index Size;
    uint8 *Base;
    memory_index Used;

    uint32 TempCount;
};

struct temporary_memory {
    memory_arena *Arena;
    memory_index Used;
};

internal void InitializeArena(memory_arena *Arena, memory_index Size, void *Base) {
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

struct loaded_bitmap {
    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;
};

struct environment_map {
    loaded_bitmap LOD[4];
};

struct hero_bitmaps {
    v2 Align;
    loaded_bitmap Head;
    loaded_bitmap Cape;
    loaded_bitmap Torso;
};

struct low_entity {
    world_position P;
    sim_entity Sim;
};

struct controlled_hero {
    uint32 EntityIndex;
    // NOTE: These are the input requests
    v2 ddP;
    v2 dSword;
    real32 dZ;
};

struct pairwise_collision_rule {
    bool32 CanCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;
    pairwise_collision_rule *NextInHash;
};

struct game_state;
void AddCollisionRule(game_state *GameState, uint32 StorageIndexA, uint32 StorageIndexB,
                      bool32 ShouldCollide);

struct ground_buffer {
    world_position P;
    loaded_bitmap Bitmap;
};

struct game_state {
    memory_arena WorldArena;
    world *World;

    real32 TypicalFloorheight;
    // TODO: Allow split-screen for me and Laura
    // OR multiplayer support and just a seperate camera for each player entity window
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

    uint32 LowEntityCount;
    low_entity LowEntities[100000];

    loaded_bitmap Grass[2];
    loaded_bitmap Ground[4];
    loaded_bitmap Tuft[3];

    loaded_bitmap Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bitmap Shadow;

    loaded_bitmap Sword;
    loaded_bitmap Tree;
    loaded_bitmap Stairwell;

    real32 MetersToPixels;
    real32 PixelsToMeters;

    // TODO: Must be power of two rn
    pairwise_collision_rule *CollisionRuleHash[256];
    pairwise_collision_rule *FirstFreeCollisionRule;

    sim_entity_collision_volume_group *NullCollision;
    sim_entity_collision_volume_group *SwordCollision;
    sim_entity_collision_volume_group *StairCollision;
    sim_entity_collision_volume_group *PlayerCollision;
    sim_entity_collision_volume_group *FamiliarCollision;
    sim_entity_collision_volume_group *MonsterCollision;
    sim_entity_collision_volume_group *WallCollision;
    sim_entity_collision_volume_group *StandardRoomCollision;

    real32 Time;

    loaded_bitmap TestDiffuse;
    loaded_bitmap TestNormal;
};

struct transient_state {
    bool32 IsInitialized;
    memory_arena TransientArena;
    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;

    uint32 EnvMapWidth;
    uint32 EnvMapHeight;
    // NOTE: 0 is bottom, 1 is middle, 2 is top
    environment_map EnvMaps[3];
};

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)
inline void *PushSize_(memory_arena *Arena, memory_index Size) {
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return (Result);
}

inline temporary_memory BeginTemporaryMemory(memory_arena *Arena) {
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    ++Arena->TempCount;
    return (Result);
}

inline void EndTemporaryMemory(temporary_memory TempMem) {
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void CheckArena(memory_arena *Arena) { Assert(Arena->TempCount == 0); }

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void ZeroSize(memory_index Size, void *Ptr) {
    // TODO: Check this guy for performance
    uint8 *Byte = (uint8 *)Ptr;
    while (Size--) {
        *Byte++ = 0;
    }
}

inline low_entity *GetLowEntity(game_state *GameState, uint32 Index) {
    low_entity *Result = 0;

    if ((Index > 0) && (Index < GameState->LowEntityCount)) {
        Result = GameState->LowEntities + Index;
    }

    return (Result);
}

#define HANDMADE_H
#endif
