#if !defined(HANDMADE_H)

/* TODO:
    - Asset streaming

    - Rendering
        - Straighten out all coordinate systems!
            - Screen
            - World
            - Texture
        - Particle systems
        - Lighting
        - Final Optimization

    - Debugging
        - Fonts
        - Logging
        - Diagramming
        - (A LITTLE GUI, but a tiny bit) Switchers / sliders /etc.
        - Draw tile chunks so we can verify that things are aligned / in the chunks

    - Audio
        - Sound effect triggers
        - Ambient Sounds
        - Music

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

    - Metagame / save games?
        - Should we allow saved games? Just for "pausing"
        - Continuous save for crash recovery?

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
#include "handmade_render_group.h"

#define Pi32 3.14159265359f

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

struct hero_bitmaps {
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

enum asset_state {
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};

struct asset_slot {
    uint32 LockCount;
    asset_state State;
    loaded_bitmap *Bitmap;
};

enum game_asset_id {
    GAI_Backdrop,
    GAI_Shadow,
    GAI_Sword,
    GAI_Tree,
    GAI_Stairwell,

    GAI_Count,
};

struct asset_tag {
    uint32 ID;
    real32 Value;
};

struct asset_bitmap_info {
    v2 AlignPercentage;
    real32 WidthOverHeight;
    int32 Width;
    int32 Height;

    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct asset_group {
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};

struct game_assets {
    struct transient_state *TranState;
    memory_arena Arena;
    debug_platform_read_entire_file *ReadEntireFile;
    asset_slot Bitmaps[GAI_Count];

    // NOTE: Array'd assets
    loaded_bitmap Grass[2];
    loaded_bitmap Ground[4];
    loaded_bitmap Tuft[3];

    // NOTE: Structured assets
    hero_bitmaps HeroBitmaps[4];
};

inline loaded_bitmap *GetBitmap(game_assets *Assets, game_asset_id ID) {
    loaded_bitmap *Result = Assets->Bitmaps[ID].Bitmap;

    return (Result);
}

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

    loaded_bitmap TestDiffuse; // TODO: Re-fill this guy with gray.
    loaded_bitmap TestNormal;
};

struct task_with_memory {
    bool32 BeingUsed;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

struct transient_state {
    bool32 IsInitialized;
    memory_arena TransientArena;

    task_with_memory Tasks[4];

    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;
    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    uint32 EnvMapWidth;
    uint32 EnvMapHeight;
    // NOTE: 0 is bottom, 1 is middle, 2 is top
    environment_map EnvMaps[3];

    game_assets Assets;
};

inline memory_index GetAlignmentOffset(memory_arena *Arena, memory_index Alignment) {
    memory_index AlignmentOffset = 0;

    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentMask = Alignment - 1;
    if (ResultPointer & AlignmentMask) {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }

    return (AlignmentOffset);
}

inline memory_index GetArenaSizeRemaining(memory_arena *Arena, memory_index Alignment = 4) {
    memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));
    return (Result);
}

#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count) * sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ## __VA_ARGS__)
inline void *PushSize_(memory_arena *Arena, memory_index SizeInit, memory_index Alignment = 4) {
    memory_index Size = SizeInit;

    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    Size += AlignmentOffset;

    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    Assert(Size >= SizeInit);

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

inline void SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size,
                     memory_index Alignment = 16) {
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize_(Arena, Size, Alignment);
    Result->Used = 0;
    Result->TempCount = 0;
}

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

global_variable platform_add_entry *PlatformAddEntry;
global_variable platform_complete_all_work *PlatformCompleteAllWork;

internal void LoadAsset(game_assets *Assets, game_asset_id ID);

#define HANDMADE_H
#endif
