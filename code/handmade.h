#include "types.h"
#if !defined(HANDMADE_H)

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
};

struct loaded_bitmap {
    int32 Width;
    int32 Height;
    uint32 *Pixels;
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

struct entity_visible_piece {
    loaded_bitmap *Bitmap;
    v2 Offset;
    real32 OffsetZ;
    real32 EntityZC;
    real32 R, G, B, A;
    v2 Dim;
};

struct controlled_hero {
    uint32 EntityIndex;
    // NOTE: These are the input requests
    v2 ddP;
    v2 dSword;
    real32 dZ;
};

struct game_state {
    memory_arena WorldArena;
    world *World;

    // TODO: Allow split-screen for me and Laura
    // OR multiplayer support and just a seperate camera for each player entity window
    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

    uint32 LowEntityCount;
    low_entity LowEntities[100000];

    loaded_bitmap Backdrop;
    hero_bitmaps HeroBitmaps[4];
    loaded_bitmap Shadow;

    loaded_bitmap Sword;
    loaded_bitmap Tree;

    real32 MetersToPixels;
};

// TODO: Will be part of the renderer
struct entity_visible_piece_group {
    game_state *GameState;
    uint32 Count;
    entity_visible_piece Pieces[32];
};

internal void InitializeArena(memory_arena *Arena, memory_index Size, void *Base) {
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
inline void *PushSize_(memory_arena *Arena, memory_index Size) {
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return (Result);
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

#define HANDMADE_H
#endif
