#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"
#include "Voxel/Generation/VoxelGenerationPagedArray.h"


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
    // 已发布细水事实证明本列可在不高于地面的障碍下排至真实湿根；不是湿接收体。
    bool bHasCertifiedDryOutlet = false;
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

enum class EVoxelHydrologySharedSearchPurpose : uint8
{
    DrainageParents,
    FineWaterBody
};

class WHFRAMEWORK_API FVoxelHydrologySharedSearch
{
public:
    using FReadCell = TFunctionRef<EVoxelHydrologyClosureRead(
        const FIntPoint&, FVoxelHydrologySharedCell&, FString&)>;

    class WHFRAMEWORK_API FCellView
    {
    public:
        explicit FCellView(const FVoxelHydrologySharedSearch* InSearch) : Search(InSearch) {}
        int32 Num() const;
        FVoxelHydrologySharedCell operator[](int32 InIndex) const;
    private:
        const FVoxelHydrologySharedSearch* Search;
    };

    bool Initialize(const FIntPoint& InSeed, const FIntPoint& InMinimum,
        const FIntPoint& InMaximumInclusive, FString& OutError,
        EVoxelHydrologySharedSearchPurpose InPurpose = EVoxelHydrologySharedSearchPurpose::DrainageParents);
    EVoxelHydrologySharedSearchStep Advance(FReadCell InReadCell, int32 InFaceBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    bool IsReady() const { return bReady; }
    FCellView GetCells() const { check(bReady); return FCellView(this); }
    FVoxelHydrologySharedCell GetSeedResult() const { check(bReady); return ReadCell(0); }
    int32 FindCellIndex(const FIntPoint& InCell) const;
    int32 GetCellCount() const { return Cells.Num(); }
    int32 GetThreshold() const { return Threshold; }
    uint8 GetSolveStage() const { return SolveStage; }
    bool TryGetSeedDryCertificate(FVoxelHydrologySharedCell& OutCell) const;
    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    // 大部分细水列没有接收体/局部出口；公共读取视图按需还原这些稀疏详情。
    struct FSearchCell
    {
        FIntPoint Cell = FIntPoint::ZeroValue;
        int32 GroundZ = 0;
        uint32 RainWeight = 0;
        int32 SpillZ = MAX_int32;
        int32 PlateauDistance = MAX_int32;
        int32 Parent = INDEX_NONE;
        int32 DetailIndex = INDEX_NONE;
        bool bIncluded = false;
        bool bHasLocalOutlet = false;
        bool bParentIsLocalOutlet = false;
        bool bReceiver = false;
        bool bHasCertifiedDryOutlet = false;
        bool IsReceiver() const { return bReceiver; }
    };
    static_assert(sizeof(FSearchCell) <= 40);

    FVoxelHydrologySharedCell ReadCell(int32 InIndex) const;
    const FVoxelHydrologySharedCell& GetDetails(const FSearchCell& InCell) const;
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

    TVoxelGenerationPagedArray<FSearchCell> Cells;
    TVoxelGenerationPagedArray<FVoxelHydrologySharedCell> Details;
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
    bool bPreferDescendingWitness = false;
    bool bFineWaterOnly = false;
};
