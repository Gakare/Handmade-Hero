#if !defined(HANDMADE_PLATFORM_H)
/*
 * HANDMADE_INTERNAL:
 * 0 - Build for public release
 * 1 - Build for development
 *
 * HANDMADE_SLOW:
 * 0 - Not slow code allowed!
 * 1 - Slow code welcome.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_MSVC
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO: More Compilers
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SSE/NEON optimizations are not available for this compiler yet!!!
#endif

#include "types.h"
#include <float.h>

#define Real32Maximum FLT_MAX

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

#define Align16(Value) ((Value + 15) & ~15)

typedef struct game_button_state {
    int HalfTransitionCount;
    bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input {
    bool32 IsConnected;
    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union {
        game_button_state Buttons[12];
        struct {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Start;
            game_button_state Back;
        };
    };
} game_controller_input;

typedef struct game_input {
    game_button_state MouseButtons[5];
    int32 MouseX, MouseY, MouseZ;

    bool32 ExecutableReloaded;
    real32 dTForFrame;

    game_controller_input Controllers[5];
} game_input;

inline uint32 SafeTruncateUInt64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return (Result);
}

typedef struct thread_context {
    int Placeholder;
} thread_context;

#if HANDMADE_INTERNAL
/* IMPORTANT:
* These are NOT for doing anything in the shipping game - they are blocking and the
* write doesn't protect against lost data!
*/
typedef struct debug_read_file_result {
    uint32 ContentsSize;
    void *Contents;
} debug_read_file_result;
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)                                            \
    debug_read_file_result name(thread_context *Thread, const char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name)                                            \
    void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name)                                           \
    bool32 name(thread_context *Thread, const char *Filename, uint32 MemorySize,         \
                void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

enum {
    /* 0 */ DebugCycleCounter_GameUpdateAndRender,
    /* 1 */ DebugCycleCounter_RenderGroupToOutput,
    /* 2 */ DebugCycleCounter_DrawRectangleSlowly,
    /* 3 */ DebugCycleCounter_ProcessPixel,
    /* 4 */ DebugCycleCounter_DrawRectangleQuickly,
   DebugCycleCounter_Count,
};
typedef struct debug_cycle_counter {
    uint64 CycleCount;
    uint32 HitCount;
} debug_cycle_counter;

extern struct game_memory *DebugGlobalMemory;
#if _MSC_VER
#define BEGIN_TIMED_BLOCK(ID) uint64 StartCycleCount##ID = __rdtsc();
#define END_TIMED_BLOCK(ID)  DebugGlobalMemory->Counters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount##ID;\
                             ++DebugGlobalMemory->Counters[DebugCycleCounter_##ID].HitCount;
// TODO: Clamp END_TIME_BLOCK_COUNTED so that if the calc is wrong, it won't overflow!
#define END_TIMED_BLOCK_COUNTED(ID, Count)  DebugGlobalMemory->Counters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount##ID;\
                             DebugGlobalMemory->Counters[DebugCycleCounter_##ID].HitCount += Count;

#else
#define BEGIN_TIMED_BLOCK(ID) 
#define END_TIMED_BLOCK(ID)  
#define END_TIMED_BLOCK_COUNTED(ID, Count)
#endif

#endif

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback,
                           void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);

typedef struct game_memory {
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void *PermanentStorage; // Required to be cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; // Required to be cleared to zero at startup

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    platform_add_entry *PlatformAddEntry;
    platform_complete_all_work *PlatformCompleteAllWork;

    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;

#if HANDMADE_INTERNAL
    debug_cycle_counter Counters[DebugCycleCounter_Count];
#endif
} game_memory;

// Controller/Keyboard input, bitmap buffer to use,
// sound buffer to use
#define BITMAP_BYTES_PER_PIXEL 4
typedef struct game_offscreen_buffer {
    // NOTE: Pixels are always 32-bits wide
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer {
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
} game_sound_output_buffer;

#define GAME_UPDATE_AND_RENDER(name)                                                     \
    void name(thread_context Thread, game_memory *Memory, game_offscreen_buffer *Buffer, \
              game_input *Input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name)                                                     \
    void name(thread_context Thread, game_memory *Memory,                                \
              game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#define HANDMADE_PLATFORM_H
#endif
