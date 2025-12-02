#include "handmade.h"
#include "win32_handmade.h"

#include <fileapi.h>
#include <math.h>

#include <stdio.h>
#include <winbase.h>
#include <windows.h>
#include <malloc.h>
#include <winuser.h>
#include <Xinput.h>
#include <dsound.h>

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return (ERROR_DEVICE_NOT_CONNECTED); }

#define X_INPUT_SET_STATE(name)                                                          \
    DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return (ERROR_DEVICE_NOT_CONNECTED); }

global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFreq;
global_variable bool32 GlobalPause;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

#define DIRECT_SOUND_CREATE(name)                                                        \
    HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
    if (Memory) {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
    debug_read_file_result Result = {};

    HANDLE FileHandle =
        CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize)) {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents =
                VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents) {
                DWORD BytesRead = 0;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    (FileSize32 == BytesRead)) {
                    Result.ContentsSize = FileSize32;
                } else {
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            } else {
            }
        } else {
        }
        CloseHandle(FileHandle);
    } else {
    }
    return (Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten = 0;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
            Result = (BytesWritten == MemorySize);
        } else {
        }

        CloseHandle(FileHandle);
    } else {
    }
    return (Result);
}

struct win32_game_code {
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;

    // IMPORTANT: Must be checked before calling
    game_get_sound_samples *GetSoundSamples;
    game_update_and_render *UpdateAndRender;

    bool32 IsValid;
};

inline FILETIME Win32GetLastWriteTime(const char *Filename) {
    FILETIME LastWriteTime = {};
    WIN32_FILE_ATTRIBUTE_DATA Data = {};
    if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data)) {
        LastWriteTime = Data.ftLastWriteTime;
    }
    return (LastWriteTime);
}

internal win32_game_code Win32LoadGameCode(const char *Filename,
                                           const char *TempDLLName) {
    win32_game_code Result = {};

    Result.DLLLastWriteTime = Win32GetLastWriteTime(Filename);

    BOOL CopyFileStatus = CopyFileA(Filename, TempDLLName, FALSE);

    Result.GameCodeDLL = LoadLibraryA(TempDLLName);
    if (Result.GameCodeDLL) {
        Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(
            Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(
            Result.GameCodeDLL, "GameGetSoundSamples");
        Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
    }

    if (!Result.IsValid) {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return (Result);
}

internal void Win32UnloadGameCode(win32_game_code *GameCode) {
    if (GameCode->GameCodeDLL) {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void Win32LoadXInput(void) {
    // NOTE: This supports playstation controller
    HMODULE XInputLibrary = LoadLibrary("xinputuap.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibrary("xinput1_4.dll");
    }
    if (!XInputLibrary) {
        XInputLibrary = LoadLibrary("xinput9_1_0.dll");
    }
    if (!XInputLibrary) {
        XInputLibrary = LoadLibrary("xinput1_3.dll");
    }

    if (XInputLibrary) {
        XInputGetState =
            (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState =
            (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal void CatStrings(size_t SourceACount, const char *SourceA, size_t SourceBCount,
                         const char *SourceB, size_t DestCount, char *Dest) {
    for (int Index = 0; Index < SourceACount; ++Index) {
        *Dest++ = *SourceA++;
    }

    for (int Index = 0; Index < SourceBCount; ++Index) {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal void Win32GetEXEFileName(win32_state *State) {
    DWORD SizeOfFileName =
        GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePathLastExeFileNameSlash = State->EXEFileName;
    for (char *Scan = State->EXEFileName; *Scan; ++Scan) {
        if (*Scan == '\\') {
            State->OnePathLastExeFileNameSlash = Scan + 1;
        }
    }
}

// NOTE: Does not count null terminator
internal int StringLength(const char *str) {
    int Count = 0;
    while (*str++) {
        ++Count;
    }
    return (Count);
}

internal void Win32BuildEXEPath(win32_state *State, const char *Filename, int DestCount,
                                char *Dest) {
    CatStrings(State->OnePathLastExeFileNameSlash - State->EXEFileName,
               State->EXEFileName, StringLength(Filename), Filename, DestCount, Dest);
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
    // Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary) {
        // Get a DirectSound Object
        direct_sound_create *DirectSoundCreate =
            (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign =
                (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec =
                WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                             &PrimaryBuffer, 0))) {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                        OutputDebugStringA("Primary buffer was set.\n");
                    } else {
                    }
                } else {
                }
            } else {
            }
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                         &GlobalSecondaryBuffer, 0))) {
            }
        } else {
        }
    } else {
    }
}
internal void Win32ClearBuffer(win32_sound_output *SoundOutput) {
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                              &Region1, &Region1Size, &Region2,
                                              &Region2Size, 0))) {
        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock,
                                   DWORD BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer) {
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1,
                                              &Region1Size, &Region2, &Region2Size, 0))) {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
    win32_window_dimension Result = {};
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return (Result);
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width,
                                    int Height) {
    if (Buffer->Memory) {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // BitMap is top-down
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitMapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitMapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth,
                                         int WindowHeight,
                                         win32_offscreen_buffer *Buffer) {
    if ((WindowWidth >= Buffer->Width * 2) && (WindowHeight >= Buffer->Height * 2)) {
        StretchDIBits(DeviceContext, 0, 0, 2 * Buffer->Width, 2 * Buffer->Height, 0, 0,
                      Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
    } else {
        int OffsetX = 10;
        int OffsetY = 10;

        PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
        PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight,
               BLACKNESS);
        PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight,
               BLACKNESS);

        // Note: For prototyping, no stretching the pixels. Much prefer 1-to-1
        // pixels.
        // StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0,
        // 0,
        //               Buffer->Width, Buffer->Height, Buffer->Memory,
        //               &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
        StretchDIBits(DeviceContext, OffsetX, OffsetY, Buffer->Width, Buffer->Height, 0,
                      0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
    }
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown) {
    if (NewState->EndedDown != IsDown) {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                              game_button_state *OldState,
                                              game_button_state *NewState,
                                              DWORD ButtonBit) {
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32GetInputFileLocation(win32_state *State, bool32 InputStream,
                                        int SlotIndex, int DestCount, char *Dest) {
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
    Win32BuildEXEPath(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *Win32GetReplayBuffer(win32_state *State, int Index) {
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer *Result = &State->ReplayBuffers[Index];
    return (Result);
}

internal void Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex) {
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if (ReplayBuffer->MemoryBlock) {
        State->InputRecordingIndex = InputRecordingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(FileName),
                                  FileName);
        State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE | GENERIC_READ, 0, 0,
                                             CREATE_ALWAYS, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif

        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void Win32EndRecordingInput(win32_state *State) {
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayBack(win32_state *State, int InputPlaybackIndex) {
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlaybackIndex);
    if (ReplayBuffer->MemoryBlock) {
        State->InputPlayingIndex = InputPlaybackIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlaybackIndex, sizeof(FileName),
                                  FileName);
        State->PlaybackHandle =
            CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif

        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void Win32EndPlaybackInput(win32_state *State) {
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state *State, game_input *Input) {
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, Input, sizeof(*Input), &BytesWritten, 0);
}

internal void Win32PlaybackInput(win32_state *State, game_input *Input) {
    DWORD BytesRead = 0;
    if (ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0)) {
        if (BytesRead == 0) {
            // NOTE: Hit end of stream
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndPlaybackInput(State);
            Win32BeginInputPlayBack(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0);
        }
    }
}

internal void ToggleFullScreen(HWND Window) {
    // NOTE: This is from Raymond Chen's blog
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY),
                           &MonitorInfo)) {
            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP, MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                         SWP_FRAMECHANGED);
    }
}

internal void Win32ProcessPendingMessages(win32_state *State,
                                          game_controller_input *KeyboardController) {
    MSG Message;

    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
        switch (Message.message) {
            case WM_QUIT: {
                GlobalRunning = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if (WasDown != IsDown) {
                    if (VKCode == VK_OEM_COMMA || VKCode == 'W') {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    } else if (VKCode == 'A') {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft,
                                                    IsDown);
                    } else if (VKCode == 'O' || VKCode == 'S') {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown,
                                                    IsDown);
                    } else if (VKCode == 'E' || VKCode == 'D') {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight,
                                                    IsDown);
                    } else if (VKCode == VK_OEM_7 || VKCode == 'Q') {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder,
                                                    IsDown);
                    } else if (VKCode == VK_OEM_PERIOD) {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder,
                                                    IsDown);
                    } else if (VKCode == VK_UP) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp,
                                                    IsDown);
                    } else if (VKCode == VK_LEFT) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft,
                                                    IsDown);
                    } else if (VKCode == VK_DOWN) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown,
                                                    IsDown);
                    } else if (VKCode == VK_RIGHT) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight,
                                                    IsDown);
                    } else if (VKCode == VK_ESCAPE) {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    } else if (VKCode == VK_SPACE) {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
#if HANDMADE_INTERNAL
                    else if (VKCode == 'P') {
                        if (IsDown) {
                            GlobalPause = !GlobalPause;
                        }
                    } else if (VKCode == 'L') {
                        if (IsDown) {
                            if (State->InputPlayingIndex == 0) {
                                if (State->InputRecordingIndex == 0) {
                                    Win32BeginRecordingInput(State, 1);
                                } else {
                                    Win32EndRecordingInput(State);
                                    Win32BeginInputPlayBack(State, 1);
                                }
                            } else {
                                Win32EndPlaybackInput(State);
                            }
                        }
                    }
#endif
                    if (IsDown) {
                        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                        if ((VKCode == VK_F4) && AltKeyWasDown) {
                            GlobalRunning = false;
                        }

                        if ((VKCode == VK_RETURN) && AltKeyWasDown) {
                            if (Message.hwnd) {
                                ToggleFullScreen(Message.hwnd);
                            }
                        }
                    }
                }

            } break;

            default: {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
    real32 Result = 0;
    if (Value < -DeadZoneThreshold) {
        Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    } else if (Value > DeadZoneThreshold) {
        Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    return (Result);
}

LRESULT CALLBACK Win32MainWindowCallBack(HWND Window, UINT uMsg, WPARAM WParam,
                                         LPARAM LParam) {
    LRESULT Result = 0;
    switch (uMsg) {
        case WM_SIZE: {
        } break;

        case WM_SETCURSOR: {
            if (DEBUGGlobalShowCursor) {
                Result = DefWindowProcA(Window, uMsg, WParam, LParam);
            } else {
                SetCursor(0);
            }
        } break;

        case WM_DESTROY: {
            GlobalRunning = false;
        } break;

        case WM_CLOSE: {
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP: {
#if 0
            if (WParam == TRUE) {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255,
                                           LWA_ALPHA);
            } else {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            Assert(!"Keyboard input came in through a non-dispatch message");
        } break;

        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                       &GlobalBackBuffer);
            EndPaint(Window, &Paint);
        } break;

        default: {
            Result = DefWindowProcA(Window, uMsg, WParam, LParam);
        } break;
    }
    return (Result);
}

inline LARGE_INTEGER Win32GetWallClock(void) {
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return (Result);
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
    real32 Result =
        ((real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFreq);
    return (Result);
}

internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer, int X, int Top,
                                     int Bottom, uint32 Color) {
    if (Top <= 0) {
        Top = 0;
    }
    if (Bottom >= BackBuffer->Height) {
        Bottom = BackBuffer->Height;
    }

    if ((X >= 0) && (X < BackBuffer->Width)) {
        uint8 *Pixel = ((uint8 *)BackBuffer->Memory + X * BackBuffer->BytesPerPixel +
                        Top * BackBuffer->Pitch);
        for (int Y = Top; Y < Bottom; ++Y) {
            *(uint32 *)Pixel = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer,
                                       win32_sound_output *SoundOutput, real32 C,
                                       int PadX, int Top, int Bottom, DWORD Value,
                                       uint32 Color) {
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;
    Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

#if 0
internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer, int MarkerCount,
                                    win32_debug_time_marker *Markers,
                                    int CurrentMarkerIndex,
                                    win32_sound_output *SoundOutput,
                                    real32 TargetSecondsPerFrame) {
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    real32 C =
        (real32)(BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0XFFFF0000;
        DWORD ExpectedFlipColor = 0x0000FFFF;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if (MarkerIndex == CurrentMarkerIndex) {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                       ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                       ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                       ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(
                BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom,
                                       ThisMarker->ExpectedFlipPlayCursor,
                                       ExpectedFlipColor);
        }

        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                   ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                   ThisMarker->FlipPlayCursor +
                                   480 * SoundOutput->BytesPerSample,
                                   PlayWindowColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom,
                                   ThisMarker->FlipWriteCursor, WriteColor);
    }
}
#endif

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
                     int nShowCmd) {
    win32_state Win32State = {};

    LARGE_INTEGER PerfCountFreqResult;
    QueryPerformanceFrequency(&PerfCountFreqResult);
    GlobalPerfCountFreq = PerfCountFreqResult.QuadPart;

    // NOTE: NOT FOR PROD
    Win32GetEXEFileName(&Win32State);
    char SourceGameCodeDLLFullPath[MAX_PATH];
    Win32BuildEXEPath(&Win32State, "handmade.dll", sizeof(SourceGameCodeDLLFullPath),
                      SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[MAX_PATH];
    Win32BuildEXEPath(&Win32State, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath),
                      TempGameCodeDLLFullPath);

    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    Win32LoadXInput();

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif
    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallBack;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);

    if (RegisterClassA(&WindowClass)) {
        HWND Window = CreateWindowExA(
            // WS_EX_TOPMOST | WS_EX_LAYERED,
            0, WindowClass.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (Window) {
            HDC RefreshDC = GetDC(Window);
            int MonitorRefreshHz = 240; // This is my default monitor hz
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 1) {
                MonitorRefreshHz = Win32RefreshRate;
            }
            // NOTE: This is to get 30 fps from 240
            int GameUpdateHz = (int)(MonitorRefreshHz / 8.0f);
            real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize =
                SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSecond *
                                             (real32)SoundOutput.BytesPerSample) /
                                            (real32)GameUpdateHz / 3.0f);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
                            SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

#if 0
            // NOTE: This tests the PlayCursor/WriteCursor update frequency
            while (1) {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor,
                                                          &WriteCursor);
                char Buffer[256];
                _snprintf_s(Buffer, sizeof(Buffer), "PC: %u WC:%u\n",
                            PlayCursor, WriteCursor);
                OutputDebugStringA(Buffer);
            }
#endif
            int16 *Samples =
                (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
            LPVOID BaseAddress = 0;
#endif

            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
            Win32State.TotalSize =
                GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            // TODO: Use MEM_LARGE_PAGES and call adjust token privileges when
            // not on Windows Xp?
            Win32State.GameMemoryBlock =
                VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage =
                ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

            for (int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
                 ++ReplayIndex) {
                win32_replay_buffer *ReplayBuffer =
                    &Win32State.ReplayBuffers[ReplayIndex];

                Win32GetInputFileLocation(&Win32State, false, ReplayIndex,
                                          sizeof(ReplayBuffer->FileName),
                                          ReplayBuffer->FileName);

                ReplayBuffer->FileHandle =
                    CreateFileA(ReplayBuffer->FileName, GENERIC_WRITE | GENERIC_READ, 0,
                                0, CREATE_ALWAYS, 0, 0);

                DWORD MaxSizeHigh = (Win32State.TotalSize >> 32);
                DWORD MaxSizeLow = (Win32State.TotalSize & 0xFFFFFFFF);
                ReplayBuffer->MemoryMap =
                    CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
                                      MaxSizeHigh, MaxSizeLow, 0);
                DWORD Error = GetLastError();

                ReplayBuffer->MemoryBlock =
                    MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0,
                                  Win32State.TotalSize);
                if (ReplayBuffer->MemoryBlock) {
                } else {
                    // TODO: Change this to a log message
                }
            }

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[30] = {0};

                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                bool32 SoundIsValid = false;

                win32_game_code Game =
                    Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

                uint64 LastCycleCount = __rdtsc();
                while (GlobalRunning) {
                    NewInput->dTForFrame = TargetSecondsPerFrame;
                    NewInput->ExecutableReloaded = false;

                    FILETIME NewDLLWriteTime =
                        Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                 TempGameCodeDLLFullPath);
                        NewInput->ExecutableReloaded = true;
                    }

                    game_controller_input *OldKeyboardController =
                        GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController =
                        GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for (int ButtonIndex = 0;
                         ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                         ++ButtonIndex) {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

                    if (!GlobalPause) {
                        POINT MouseP = {};
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0; // NOTE: For Mousewheel
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
                                                    GetKeyState(VK_LBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1],
                                                    GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2],
                                                    GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3],
                                                    GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4],
                                                    GetKeyState(VK_XBUTTON2) & (1 << 15));

                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if (MaxControllerCount >
                            (ArrayCount(NewInput->Controllers) - 1)) {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                        }

                        for (DWORD ControllerIndex = 0;
                             ControllerIndex < MaxControllerCount; ++ControllerIndex) {
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController =
                                GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController =
                                GetController(NewInput, OurControllerIndex);

                            XINPUT_STATE ControllerState;
                            if (XInputGetState(ControllerIndex, &ControllerState) ==
                                ERROR_SUCCESS) {
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                                NewController->StickAverageX =
                                    Win32ProcessXInputStickValue(
                                        Pad->sThumbLX,
                                        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY =
                                    Win32ProcessXInputStickValue(
                                        Pad->sThumbLY,
                                        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                if ((NewController->StickAverageX != 0.0f) ||
                                    (NewController->StickAverageY != 0.0f)) {
                                    NewController->IsAnalog = true;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                                    NewController->StickAverageY = 1.0f;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                                    NewController->StickAverageY = -1.0f;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                                    NewController->StickAverageX = -1.0f;
                                }
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                                    NewController->StickAverageX = 1.0f;
                                }

                                real32 Threshold = 0.5f;
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX < -Threshold) ? 1 : 0,
                                    &OldController->MoveLeft, &NewController->MoveLeft,
                                    1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX > Threshold) ? 1 : 0,
                                    &OldController->MoveRight, &NewController->MoveRight,
                                    1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < -Threshold) ? 1 : 0,
                                    &OldController->MoveDown, &NewController->MoveDown,
                                    1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY > Threshold) ? 1 : 0,
                                    &OldController->MoveUp, &NewController->MoveUp, 1);

                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->ActionDown,
                                    &NewController->ActionDown, XINPUT_GAMEPAD_A);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->ActionRight,
                                    &NewController->ActionRight, XINPUT_GAMEPAD_B);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->ActionLeft,
                                    &NewController->ActionLeft, XINPUT_GAMEPAD_X);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->ActionUp,
                                    &NewController->ActionUp, XINPUT_GAMEPAD_Y);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->LeftShoulder,
                                    &NewController->LeftShoulder,
                                    XINPUT_GAMEPAD_LEFT_SHOULDER);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->RightShoulder,
                                    &NewController->RightShoulder,
                                    XINPUT_GAMEPAD_RIGHT_SHOULDER);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->Start,
                                    &NewController->Start, XINPUT_GAMEPAD_START);
                                Win32ProcessXInputDigitalButton(
                                    Pad->wButtons, &OldController->Back,
                                    &NewController->Back, XINPUT_GAMEPAD_BACK);
                            } else {
                                NewController->IsConnected = false;
                            }
                        }

                        thread_context Thread = {};

                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
                        if (Win32State.InputRecordingIndex) {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        if (Win32State.InputPlayingIndex) {
                            Win32PlaybackInput(&Win32State, NewInput);
                        }

                        if (Game.UpdateAndRender) {
                            Game.UpdateAndRender(Thread, &GameMemory, &Buffer, NewInput);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds =
                            Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

                        DWORD PlayCursor = 0;
                        DWORD WriteCursor = 0;
                        if (GlobalSecondaryBuffer->GetCurrentPosition(
                                &PlayCursor, &WriteCursor) == DS_OK) {
                            if (!SoundIsValid) {
                                SoundOutput.RunningSampleIndex =
                                    WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }
                            DWORD ByteToLock = ((SoundOutput.RunningSampleIndex *
                                                 SoundOutput.BytesPerSample) %
                                                SoundOutput.SecondaryBufferSize);

                            DWORD ExpectedSoundBytesPerFrame =
                                (int)((real32)(SoundOutput.SamplesPerSecond *
                                               SoundOutput.BytesPerSample)) /
                                GameUpdateHz;
                            real32 SecondsLeftUntilFlip =
                                TargetSecondsPerFrame - FromBeginToAudioSeconds;

                            // NOTE: THIS IS IN VIDEO, BUT NOT WORKING TOO GOOD
                            //  DWORD ExpectedBytesUntilFlip =
                            //      (DWORD)((SecondsLeftUntilFlip /
                            //               TargetSecondsPerFrame) *
                            //              (real32)ExpectedSoundBytesPerFrame);
                            // DWORD ExpectedFrameBoundaryByte =
                            //    PlayCursor + ExpectedBytesUntilFlip;
                            DWORD ExpectedFrameBoundaryByte =
                                PlayCursor + ExpectedSoundBytesPerFrame;

                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor) {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;

                            bool32 AudioCardIsLowLatency =
                                (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency) {
                                TargetCursor = ExpectedFrameBoundaryByte +
                                               ExpectedSoundBytesPerFrame;
                            } else {
                                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame +
                                               SoundOutput.SafetyBytes;
                            }
                            TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor) {
                                BytesToWrite =
                                    (SoundOutput.SecondaryBufferSize - ByteToLock);
                                BytesToWrite += TargetCursor;
                            } else {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }

                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount =
                                BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if (Game.GetSoundSamples) {
                                Game.GetSoundSamples(Thread, &GameMemory, &SoundBuffer);
                            }

#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker =
                                &DebugTimeMarkers[DebugTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (UnwrappedWriteCursor < PlayCursor) {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            AudioLatencySeconds = (((real32)AudioLatencyBytes /
                                                    (real32)SoundOutput.BytesPerSample) /
                                                   (real32)SoundOutput.SamplesPerSecond);

#if 0
                            char TextBuffer[256];
                            _snprintf_s(TextBuffer, sizeof(TextBuffer),
                                        "BTL:%u TC:%u BTW:%u - PC:%u WC:%u "
                                        "DELTA:%u (%fs)\n",
                                        ByteToLock, TargetCursor, BytesToWrite,
                                        PlayCursor, WriteCursor, AudioLatencyBytes,
                                        AudioLatencySeconds);
                            OutputDebugStringA(TextBuffer);
#endif
#endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite,
                                                 &SoundBuffer);
                        } else {
                            SoundIsValid = false;
                        }

                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed =
                            Win32GetSecondsElapsed(LastCounter, WorkCounter);

                        // TODO: MIGHT CHANGE
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                            if (SleepIsGranular) {
                                DWORD SleepMS =
                                    (DWORD)(1000.0f * (TargetSecondsPerFrame -
                                                       SecondsElapsedForFrame));
                                // TODO: BUG: This is what is causing my ms to not be
                                // a stable rate
                                if (SleepMS > 0) {
                                    Sleep(SleepMS);
                                }
                            }
                            while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(
                                    LastCounter, Win32GetWallClock());
                            }
                            real32 TestSecondsElapsedForFrame =
                                Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
                                OutputDebugStringA("Missed Sleep\n");
                            }

                        } else { // MISSED FRAME RATE
                        }
                        win32_window_dimension Dimension =
                            Win32GetWindowDimension(Window);

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real64 MSPerFrame =
                            1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter = EndCounter;
                        // #if HANDMADE_INTERNAL
                        //  Win32DebugSyncDisplay(&GlobalBackBuffer,
                        //                        ArrayCount(DebugTimeMarkers),
                        //                        DebugTimeMarkers, DebugTimeMarkerIndex -
                        //                        1, &SoundOutput, TargetSecondsPerFrame);
                        // #endif
                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,
                                                   Dimension.Height, &GlobalBackBuffer);
                        ReleaseDC(Window, DeviceContext);

#if HANDMADE_INTERNAL
                        { // Debug code
                            if (GlobalSecondaryBuffer->GetCurrentPosition(
                                    &PlayCursor, &WriteCursor) == DS_OK) {
                                Assert(DebugTimeMarkerIndex <
                                       ArrayCount(DebugTimeMarkers));
                                win32_debug_time_marker *Marker =
                                    &DebugTimeMarkers[DebugTimeMarkerIndex];
                                Marker->FlipPlayCursor = PlayCursor;
                                Marker->FlipWriteCursor = WriteCursor;
                            }
                        }
#endif

                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;
#if 0
                        uint64 EndCycleCount = __rdtsc();
                        int64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        real64 FPS = 0.0f;
                        real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

                        char FPSBuffer[256];
                        _snprintf_s(FPSBuffer, sizeof(FPSBuffer),
                                    "%0.2fms/f / %0.2ff/s, %0.2fc/f\n", MSPerFrame, FPS,
                                    MCPF);
                        OutputDebugStringA(FPSBuffer);
#endif

#if HANDMADE_INTERNAL
                        ++DebugTimeMarkerIndex;
                        if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)) {
                            DebugTimeMarkerIndex = 0;
                        }
#endif
                    }
                }
            } else {
            }
        } else {
        }
    } else {
    }

    return (0);
}
