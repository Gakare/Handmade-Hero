#if !defined(HANDMADE_INTRINSICS_H)
// TODO: Do these all myself
#include "math.h"
#include "types.h"

inline int32 SignOf(int32 Value) {
    int32 Result = (Value >= 0) ? 1 : -1;
    return Result;
}

inline real32 Sqrt(real32 Value) {
    real32 Result = sqrtf(Value);

    return (Result);
}

inline real32 AbsoluteValue(real32 Value) {
    real32 Result = (real32)fabs(Value);
    return (Result);
}

inline uint32 RotateLeft(uint32 Value, int32 Amount) {
#if COMPILER_MSVC
    uint32 Result = _rotl(Value, Amount);
#else
    Amount &= 31;
    uint32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
#endif

    return (Result);
}

inline uint32 RotateRight(uint32 Value, int32 Amount) {
#if COMPILER_MSVC
    uint32 Result = _rotr(Value, Amount);
#else
    Amount &= 31;
    uint32 Result = ((Value >> Amount) | (Value << (32 - Amount)));
#endif

    return (Result);
}

inline int32 RoundReal32ToInt32(real32 Value) {
    int32 Result = (int32)roundf(Value);
    return (Result);
}

inline uint32 RoundReal32ToUInt32(real32 Value) {
    uint32 Result = (uint32)round(Value);
    return (Result);
}

inline int32 FloorReal32ToInt32(real32 Value) {
    int32 Result = (int32)floorf(Value);
    return (Result);
}

inline int32 CeilReal32ToInt32(real32 Value) {
    int32 Result = (int32)ceilf(Value);
    return (Result);
}

inline int32 TruncateReal32ToInt32(real32 Value) {
    int32 Result = (int32)Value;
    return (Result);
}

inline real32 Sin(real32 Angle) {
    real32 Result = sinf(Angle);
    return (Result);
}

inline real32 Cos(real32 Angle) {
    real32 Result = cosf(Angle);
    return (Result);
}

inline real32 ATan2(real32 Y, real32 X) {
    real32 Result = atan2f(Y, X);
    return (Result);
}

struct bit_scan_result {
    bool32 Found;
    uint32 Index;
};

internal bit_scan_result FindLSB1(uint32 Value) {
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
    for (uint32 Test = 0; Test < 32; ++Test) {
        if (Value & (1 << Test)) {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif

    return (Result);
}
#define HANDMADE_INTRINSICS_H
#endif
