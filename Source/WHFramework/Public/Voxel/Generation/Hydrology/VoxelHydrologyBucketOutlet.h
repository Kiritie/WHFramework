#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySectionCertificate.h"

struct WHFRAMEWORK_API FVoxelHydrologyBucketOutletResult
{
    bool bHasOutlet = false;
    FIntPoint Terminal = FIntPoint::ZeroValue;
    int32 WaterZ = MIN_int32;
    FVoxelStableId BodyId;
    int32 Barrier = MAX_int32;
    int32 PathLength = 0;
    uint64 Witness = 0;
};

class WHFRAMEWORK_API FVoxelHydrologyBucketOutlet
{
public:
    using FReadSample = FVoxelHydrologySectionCertificate::FReadSample;
    bool Initialize(const FIntPoint& InAnchor, const FIntPoint& InBucketMinimum,
        const FIntPoint& InBucketMaximum, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadSample Read, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    bool IsReady() const { return bReady; }
    const FVoxelHydrologyBucketOutletResult& GetResult() const { check(bReady); return Result; }
    const TArray<FIntPoint>& GetWitnessPath() const { check(bReady); return Path; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    struct FCell
    {
        FVoxelHydrologySectionSample Sample;
        int32 Barrier = MAX_int32;
        int32 HeapIndex = INDEX_NONE;
        int32 Distance = INDEX_NONE;
        int32 Parent = INDEX_NONE;
    };
    FIntPoint Position(int32 Index) const;
    int32 Neighbor(int32 Index, int32 Face) const;
    bool HeapLess(int32 A, int32 B) const;
    void HeapRaise(int32 Index);
    int32 HeapPop();
    void Hash(uint64 Value);
    TArray<FCell> Cells;
    TArray<int32> Heap;
    TArray<int32> Queue;
    TArray<FIntPoint> Path;
    FIntPoint Anchor, Minimum, Maximum;
    FVoxelHydrologyBucketOutletResult Result;
    int32 Width = 0, Count = 0, AnchorIndex = 0;
    int32 Stage = 0, Cursor = 0, Chosen = INDEX_NONE;
    int32 Threshold = MAX_int32, WetCount = 0;
    uint64 PathHash = 1469598103934665603ull;
    bool bInitialized = false, bReady = false;
};
