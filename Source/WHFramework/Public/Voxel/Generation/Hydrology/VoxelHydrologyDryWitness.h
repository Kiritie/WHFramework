#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"

enum class EVoxelHydrologyDryWitnessStep : uint8
{
    None,
    Yielded,
    NeedInput,
    CapacityBlocked,
    Proven,
    Unproven,
    Failed,
    Canceled
};

// 候选只引导搜索；逐格认证到真实湿体的路径才能证明干地，未证明交回完整求解。
class WHFRAMEWORK_API FVoxelHydrologyDryWitness
{
public:
    using FReadCell = FVoxelHydrologySharedSearch::FReadCell;
    using FWetHint = TFunctionRef<bool(const FIntPoint&)>;

    bool Initialize(const FIntPoint& InSeed, const FIntPoint& InMinimum,
        const FIntPoint& InMaximumInclusive, FString& OutError);
    EVoxelHydrologyDryWitnessStep Advance(FReadCell InReadCell, FWetHint InWetHint,
        int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    const FVoxelHydrologySharedCell& GetCertificate() const;
    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    enum class EPhase : uint8
    {
        None,
        ReadSeed,
        FindTarget,
        Search,
        Proven,
        Unproven
    };

    struct FNode
    {
        FVoxelHydrologySharedCell Sample;
        int32 Distance = MAX_int32;
        bool bClosed = false;
    };

    struct FEvent
    {
        int32 Index = INDEX_NONE;
        int32 Distance = 0;
    };

    bool EventLess(const FEvent& InLeft, const FEvent& InRight) const;
    bool Push(const FEvent& InEvent, FString& OutError);
    FEvent Pop();
    bool AddNode(const FVoxelHydrologySharedCell& InSample, int32 InDistance, FString& OutError);
    bool ValidateSample(const FVoxelHydrologySharedCell& InSample, FString& OutError) const;

    TArray<FNode> Nodes;
    TMap<FIntPoint, int32> ByCell;
    TArray<FEvent> Frontier;
    FVoxelHydrologySharedCell Seed;
    FIntPoint Minimum = FIntPoint::ZeroValue;
    FIntPoint Maximum = FIntPoint::ZeroValue;
    FIntPoint Target = FIntPoint::ZeroValue;
    EPhase Phase = EPhase::None;
    int32 MapCapacity = 0;
    int32 ProbeIndex = 0;
    int32 Current = INDEX_NONE;
    int32 Face = 0;
};
