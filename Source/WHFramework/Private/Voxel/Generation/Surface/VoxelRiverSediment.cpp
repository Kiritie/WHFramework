#include "Voxel/Generation/Surface/VoxelRiverSediment.h"

namespace
{
    uint64 SedimentMix(uint64 Value)
    {
        Value = (Value ^ (Value >> 30)) * 0xbf58476d1ce4e5b9ull;
        Value = (Value ^ (Value >> 27)) * 0x94d049bb133111ebull;
        return Value ^ (Value >> 31);
    }

    int64 SedimentFloorDivide(const int64 Value, const int32 Period)
    {
        const int64 Quotient = Value / Period;
        return Quotient - (Value % Period < 0 ? 1 : 0);
    }

    int32 SedimentCorner(const int32 Seed, const int64 X, const int64 Y, const uint64 Salt)
    {
        const uint64 Hash = SedimentMix(uint64(uint32(Seed)) ^ Salt ^
            SedimentMix(uint64(X)) ^ SedimentMix(uint64(Y) ^ 0x9e3779b97f4a7c15ull));
        return int32(Hash & 65535ull) - 32768;
    }

    int64 SedimentSmooth(const int64 FractionQ16)
    {
        const int64 Square = FractionQ16 * FractionQ16 / 65536;
        return Square * (196608 - 2 * FractionQ16) / 65536;
    }

    int32 SedimentLerp(const int32 A, const int32 B, const int64 AlphaQ16)
    {
        return int32(int64(A) + (int64(B) - A) * AlphaQ16 / 65536);
    }

    int32 SedimentNoise(const int32 Seed, const int32 X, const int32 Y,
        const int32 Period, const uint64 Salt)
    {
        const int64 IX = SedimentFloorDivide(X, Period);
        const int64 IY = SedimentFloorDivide(Y, Period);
        const int64 FX = SedimentSmooth((int64(X) - IX * Period) * 65536 / Period);
        const int64 FY = SedimentSmooth((int64(Y) - IY * Period) * 65536 / Period);
        return SedimentLerp(
            SedimentLerp(SedimentCorner(Seed, IX, IY, Salt),
                SedimentCorner(Seed, IX + 1, IY, Salt), FX),
            SedimentLerp(SedimentCorner(Seed, IX, IY + 1, Salt),
                SedimentCorner(Seed, IX + 1, IY + 1, Salt), FX), FY);
    }
}

bool VoxelRiverSediment::HasDryBankDeposit(const int32 InSeed,
    const int32 InWorldX, const int32 InWorldY,
    const int32 InSignedWetEdgeDistanceCells,
    const int32 InGroundZ, const int32 InRiverWaterPlane)
{
    if (InRiverWaterPlane == MIN_int32 || InSignedWetEdgeDistanceCells < 0 ||
        InSignedWetEdgeDistanceCells > 7 ||
        int64(InGroundZ) < int64(InRiverWaterPlane) ||
        int64(InGroundZ) > int64(InRiverWaterPlane) + 2)
    {
        return false;
    }
    const int32 Large = SedimentNoise(InSeed, InWorldX, InWorldY, 64, 0x5345445041544348ull);
    const int32 Medium = SedimentNoise(InSeed, InWorldX, InWorldY, 16, 0x5345445749445448ull);
    const int32 Small = SedimentNoise(InSeed, InWorldX, InWorldY, 4, 0x5345444544474521ull);

    if ((3 * Large + Medium) / 4 <= 4000)
    {
        return false;
    }
    const int32 Width = 2 + int32((int64(Medium) + 32768) * 5 / 65536);

    return int64(InSignedWetEdgeDistanceCells) * 32768 <=
        int64(Width) * 32768 + Small;
}
