#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"


enum class EVoxelHydrologyClosureRead : uint8
{
    None,
    Ready,
    Pending,
    Failed
};

enum class EVoxelHydrologyClosureStatus : uint8
{
    None,
    Ready,
    Yielded,
    NeedTerrain,
    NeedConnector,
    CapacityBlocked,
    Conflict,
    Failed,
    Canceled
};

struct WHFRAMEWORK_API FVoxelHydrologyClosureStep
{
    EVoxelHydrologyClosureStatus Status = EVoxelHydrologyClosureStatus::Yielded;
    FIntPoint RequestedCell = FIntPoint::ZeroValue;
    FIntPoint FromCell = FIntPoint::ZeroValue;
    FString Diagnostic;
};

class WHFRAMEWORK_API FVoxelHydrologyBasinClosure
{
public:
    using FReadGround = TFunctionRef<EVoxelHydrologyClosureRead(
        const FIntPoint& InCell, int32& OutOriginalGround, FString& OutError)>;

    using FReadConnector = TFunctionRef<EVoxelHydrologyClosureRead(
        const FIntPoint& InFrom, const FInt64Vector2& InTo,
        TOptional<int32>& OutPlane, FString& OutError)>;

    FVoxelHydrologyBasinClosure() = default;
    FVoxelHydrologyBasinClosure(const FVoxelHydrologyBasinClosure&) = delete;
    FVoxelHydrologyBasinClosure& operator=(const FVoxelHydrologyBasinClosure&) = delete;


    bool Initialize(const FIntPoint& InSeed, int32 InWaterPlane, FString& OutError);
    FVoxelHydrologyClosureStep Advance(FReadGround InReadGround,
        FReadConnector InReadConnector, int32 InFaceBudget, int32 InMaxWaterCells,
        const TAtomic<bool>* InCancel = nullptr);

    bool IsReady() const { return bReady; }
    int32 GetWaterPlane() const { return WaterPlane; }
    FIntPoint GetSeed() const { return Seed; }

    const TArray<FIntPoint>& GetCells() const { check(bReady); return Cells; }
    const FVoxelBasinCertificate& GetCertificate() const { check(bReady); return Certificate; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    bool AppendCell(const FIntPoint& InCell, int32 InMaxWaterCells, FString& OutError);
    FVoxelHydrologyClosureStep Stop(EVoxelHydrologyClosureStatus InStatus,
        const FIntPoint& InCell, const FIntPoint& InFrom, const FString& InDiagnostic) const;

    TArray<FIntPoint> Cells;
    TSet<FIntPoint> Seen;
    int32 SeenCapacity = 0;
    int32 Cursor = 0;
    uint8 Face = 0;
    int32 WaterPlane = 0;
    FIntPoint Seed = FIntPoint::ZeroValue;
    uint64 ClosedEdges = 0;
    uint64 ConnectorEdges = 0;
    FVoxelBasinCertificate Certificate;
    bool bInitialized = false;
    bool bReady = false;
    bool bConflict = false;
};
