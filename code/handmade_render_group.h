#if !defined(HANDMADE_RENDER_GROUP_H)
#include "handmade_math.h"
#include "types.h"

/* NOTE:
    1) Everywhere outside the renderer, Y _always_ goes upward, X to the right.

    2) All bitmaps including the render target are assumed to be bottom-up
        (meaning that the first row pointer points to the bottom-most row
        when viewed on screen).

    3) It is mandatory that all inputs to the renderer are in world
   coordinate ("meters"), NOT pixels. If for some reason something absolutely
   has to be specified in pixels, that will be explicitly marked in the API, but this
   should occur exceedingly sparingly.

    4) Z is a special coordinate because it is broken up into discrete slices,
        and the renderer actually understands these slices. Z slices are what
        control the _scaling_ of things, whereas Z offsets inside a slice are what
        control Y offsetting.

    5) All color values specified to the renderer as V4's are in NON-premultiplied alpha.
*/

struct loaded_bitmap {
    v2 AlignPercentage;
    real32 WidthOverHeight;

    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;
};

struct environment_map {
    loaded_bitmap LOD[4];
    real32 Pz;
};

struct render_basis {
    v3 P;
};

// NOTE: render_group_entry is a "compact discriminated union"
enum render_group_entry_type {
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system,
};

struct render_group_entry_header {
    render_group_entry_type Type;
};

struct render_entry_clear {
    v4 Color;
};

// NOTE: This is only for test
// {
struct render_entry_coordinate_system {
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;
    loaded_bitmap *Texture;
    loaded_bitmap *NormalMap;

    real32 PixelsToMeters; // TODO: Need to store this for lighting!

    environment_map *Top;
    environment_map *Middle;
    environment_map *Bottom;
};
// }

struct render_entry_bitmap {
    loaded_bitmap *Bitmap;

    v4 Color;
    v2 P;
    v2 Size;
};

struct render_entry_rectangle {
    v4 Color;
    v2 P;
    v2 Dim;
};

struct render_transform {
    bool32 Orthographic;

    real32 MetersToPixels; // NOTE: This translates meters on the monitor into pixels on the monitor
    v2 ScreenCenter;

    // NOTE: Camera parameters
    real32 FocalLength;
    real32 DistanceAboveTarget;

    v3 OffsetP;
    real32 Scale;
};

struct render_group {
    struct game_assets *Assets;
    real32 GlobalAlpha;

    v2 MonitorHalfDimInMeters;
    render_transform Transform;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;

    uint32 MissingResourceCount;
};

// NOTE: Renderer API
#if 0
inline void PushBitmap(render_group *Group, loaded_bitmap *Bitmap, v2 Offset,
                       real32 OffsetZ, v4 Color = V4(1, 1, 1, 1)) {
inline void PushRect(render_group *Group, v2 Offset, real32 OffsetZ, v2 Dim, v4 Color);
inline void PushRectOutline(render_group *Group, v2 Offset, real32 OffsetZ, v2 Dim,
                            v4 Color);
inline void Clear(render_group *Group, v4 Color);
#endif

#define HANDMADE_RENDER_GROUP_H
#endif
