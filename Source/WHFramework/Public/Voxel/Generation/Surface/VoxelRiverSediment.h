#pragma once

#include "CoreMinimal.h"

namespace VoxelRiverSediment
{
    WHFRAMEWORK_API bool HasDryBankDeposit(int32 InSeed, int32 InWorldX,
        int32 InWorldY, int32 InSignedWetEdgeDistanceCells,
        int32 InGroundZ, int32 InRiverWaterPlane);
}
