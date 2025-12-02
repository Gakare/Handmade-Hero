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
#pragma intrinsic(_BitScanForward)
#endif

#include "types.h"
#include <float.h>

#define Real32Maximum FLT_MAX

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

typedef struct thread_context {
    int Placeholder;
} thread_context;

#if HANDMADE_INTERNAL
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

#endif

typedef struct game_memory {
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void *PermanentStorage; // Required to be cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; // Required to be cleared to zero at startup

    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
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
