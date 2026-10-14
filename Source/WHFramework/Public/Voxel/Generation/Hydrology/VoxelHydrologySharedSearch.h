#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"


struct WHFRAMEWORK_API FVoxelHydrologySharedCell
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    int32 GroundZ = 0;
    int32 ReceiverWaterZ = MIN_int32;
    FVoxelStableId ReceiverId;
    uint32 RainWeight = 0;
    int32 SpillZ = MAX_int32;
    int32 PlateauDistance = MAX_int32;
    int32 Parent = INDEX_NONE;
    bool bIncluded = false;


    bool bHasLocalOutlet = false;
    FIntPoint LocalOutletCell = FIntPoint::ZeroValue;
    int32 LocalOutletBarrier = MAX_int32;
    int32 LocalOutletWaterZ = MIN_int32;
    FVoxelStableId LocalOutletId;
    uint64 LocalOutletWitness = 0;
    bool bParentIsLocalOutlet = false;
    bool IsReceiver() const { return ReceiverWaterZ != MIN_int32; }
};

enum class EVoxelHydrologySharedSearchStep : uint8
{
    None,
    Yielded,
    NeedInput,
    CapacityBlocked,
    Ready,
    NoReceiver,
    Failed,
    Canceled
};

class WHFRAMEWORK_API FVoxelHydrologySharedSearch
{
public:
    using FReadCell = TFunctionRef<EVoxelHydrologyClosureRead(
        const FIntPoint&, FVoxelHydrologySharedCell&, FString&)>;

    bool Initialize(const FIntPoint& InSeed, const FIntPoint& InMinimum,
        const FIntPoint& InMaximumInclusive, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadCell InReadCell, int32 InFaceBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    bool IsReady() const { return bReady; }
    const TArray<FVoxelHydrologySharedCell>& GetCells() const { check(bReady); return Cells; }
    const FVoxelHydrologySharedCell& GetSeedResult() const { check(bReady); return Cells[0]; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    struct FEvent
    {
        int32 Index = INDEX_NONE;
        int32 Plane = 0;
    };
    bool EventLess(const FEvent& A, const FEvent& B) const;
    bool Push(const FEvent& Event, FString& OutError);
    FEvent Pop();
    bool Append(const FVoxelHydrologySharedCell& InCell, int32 InEntryPlane,
        int32& OutIndex, FString& OutError);
    EVoxelHydrologySharedSearchStep SolveCertificate(int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel);
    bool Neighbor(const FIntPoint& Cell, int32 Face, FIntPoint& OutCell) const;

    TArray<FVoxelHydrologySharedCell> Cells;
    TMap<FIntPoint, int32> ByCell;
    TArray<FEvent> Frontier;
    TArray<int32> PlateauQueue;
    uint8 SolveStage = 0;
    int32 SolveCursor = 0;
    int32 SolveFace = 0;
    FEvent SolveEvent;
    int32 MapCapacity = 0;
    FIntPoint Seed = FIntPoint::ZeroValue;
    FIntPoint Minimum = FIntPoint::ZeroValue;
    FIntPoint Maximum = FIntPoint::ZeroValue;
    int32 Current = INDEX_NONE;
    int32 CurrentPlane = 0;
    int32 Face = 0;
    int32 Threshold = MAX_int32;
    bool bInitialized = false;
    bool bSearchComplete = false;
    bool bReady = false;
};
