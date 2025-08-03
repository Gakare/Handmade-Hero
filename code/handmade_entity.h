#if !defined(HANDMADE_ENTITY_H)
#include "handmade_sim_region.h"
#include "types.h"

#define InvalidP V2(100000.0f, 100000.0f)

inline bool32 IsSet(sim_entity *Entity, uint32 Flag) {
    bool32 Result = Entity->Flags & Flag;

    return (Result);
}

inline void ClearFlag(sim_entity *Entity, uint32 Flag) { Entity->Flags &= ~Flag; }

inline void AddFlag(sim_entity *Entity, uint32 Flag) { Entity->Flags |= Flag; }

inline void MakeEntityNonSpatial(sim_entity *Entity) {
    AddFlag(Entity, EntityFlag_NonSpatial);
    Entity->P = InvalidP;
}

inline void MakeEntitySpatial(sim_entity *Entity, v2 P, v2 dP) {
    ClearFlag(Entity, EntityFlag_NonSpatial);
    Entity->P = P;
    Entity->dP = dP;
}

#define HANDMADE_ENTITY_H
#endif
