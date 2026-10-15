#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyDryWitness.h"


struct WHFRAMEWORK_API FVoxelHydrologyNaturalWaterKey
{
    FIntPoint MinimumMember = FIntPoint::ZeroValue;
    int32 WaterZ = MIN_int32;
    bool operator==(const FVoxelHydrologyNaturalWaterKey& Other) const
    { return MinimumMember == Other.MinimumMember && WaterZ == Other.WaterZ; }
};

struct WHFRAMEWORK_API FVoxelHydrologyFineWaterResult
{
    int32 WaterZ = MIN_int32;
    FVoxelHydrologyNaturalWaterKey NaturalKey;
    FVoxelStableId FixedId;
    bool bNaturalWater = false;
    bool bFixedWater = false;
    bool IsWet() const { return bNaturalWater || bFixedWater; }
};

// 仅保留已认证干列与真实湿根的消费事实，不携带搜索图的父索引。
struct WHFRAMEWORK_API FVoxelHydrologyFineWaterCertificate
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    int32 GroundZ = 0;
    int32 SpillZ = MAX_int32;
    int32 ReceiverWaterZ = MIN_int32;
    FVoxelStableId ReceiverId;
    bool bIncluded = false;

    bool IsReceiver() const { return ReceiverWaterZ != MIN_int32; }
};

enum class EVoxelHydrologyFineWaterStep : uint8
{
    None,
    Yielded,
    NeedTiles,
    CapacityBlocked,
    Ready,
    NoReceiver,
    Failed,
    Canceled
};

class WHFRAMEWORK_API FVoxelHydrologyFineWaterQuery
{
public:
    static constexpr int32 DryTileSide = 16;
    using FReadCell = FVoxelHydrologySharedSearch::FReadCell;
    using FWetHint = FVoxelHydrologyDryWitness::FWetHint;
    FVoxelHydrologyFineWaterQuery() = default;
    FVoxelHydrologyFineWaterQuery(const FVoxelHydrologyFineWaterQuery&) = delete;
    FVoxelHydrologyFineWaterQuery& operator=(const FVoxelHydrologyFineWaterQuery&) = delete;
    bool Initialize(const FIntPoint& Seed, const FIntPoint& Minimum,
        const FIntPoint& MaximumInclusive, FString& OutError);
    EVoxelHydrologyFineWaterStep Advance(FReadCell ReadCertifiedFineCell, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr, const FWetHint* WetHint = nullptr);
    // 从现有干证明沿不降的四邻路径逐格扩展；原始受水格和未证明低洼不加入。
    EVoxelHydrologyFineWaterStep ExpandCertifiedDryTile(FReadCell ReadOriginalFineCell, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    bool IsReady() const { return bReady; }
    int32 GetSearchCellCount() const { return Search.GetCellCount(); }
    int32 GetSearchThreshold() const { return Search.GetThreshold(); }
    int32 GetProgressStage() const { return int32(Stage) * 10 + Search.GetSolveStage(); }
    const FVoxelHydrologyFineWaterResult& GetResult() const { check(bReady); return Result; }

    const TArray<FIntPoint>& GetCells() const { check(bReady); return Members; }
    int32 GetMemberGround(int32 MemberIndex) const;
    int32 GetSeedGround() const { check(bReady); return SeedGround; }
    const TArray<FVoxelHydrologyFineWaterCertificate>& GetDrainageCertificate() const { check(bReady); return Certificates; }


    TArray<FIntPoint> TakeCells() { check(bReady); return MoveTemp(Members); }
    TArray<FVoxelHydrologySharedCell> TakeFixedContacts() { check(bReady); return MoveTemp(FixedContacts); }

    const TArray<FVoxelHydrologySharedCell>& GetFixedContacts() const { check(bReady); return FixedContacts; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    bool AddMember(const FIntPoint& Position, int32 CertificateIndex, FString& OutError);
    bool AddCertificate(const FVoxelHydrologySharedCell& InCell, FString& OutError);
    FVoxelHydrologySharedSearch Search;
    FVoxelHydrologyDryWitness DryWitness;
    TArray<FVoxelHydrologyFineWaterCertificate> Certificates;
    TArray<uint8> Visited;
    TArray<FIntPoint> Members;
    TArray<int32> MemberGrounds;
    TArray<FIntPoint> SortBuffer;
    TArray<int32> GroundSortBuffer;
    TArray<FVoxelHydrologySharedCell> FixedContacts;
    FIntPoint WorldMinimum, WorldMaximum;
    FVoxelHydrologyFineWaterResult Result;
    int32 SeedGround = 0;
    int32 Cursor = 0;
    int32 Face = 0;
    int64 SortWidth = 1;
    int64 SortRun = 0;
    int64 SortLeft = 0;
    int64 SortRight = 0;
    int64 SortWrite = 0;
    bool bMergeStarted = false;
    uint8 Stage = 0;
    bool bInitialized = false;
    bool bReady = false;
    bool bWitnessFinished = false;
};

class WHFRAMEWORK_API FVoxelHydrologyFineWaterCoverage
{
public:
    bool Initialize(const FIntPoint& CoverageMinimum, const FIntPoint& CoverageMaximumInclusive,
        const FIntPoint& WorldMinimum, const FIntPoint& WorldMaximumInclusive, FString& OutError);
    EVoxelHydrologyFineWaterStep Advance(FVoxelHydrologyFineWaterQuery::FReadCell Reader,
        int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    bool IsReady() const { return bReady; }
    const FVoxelHydrologyFineWaterResult* Find(const FIntPoint& Position) const;
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    FVoxelHydrologyFineWaterQuery Query;
    TMap<FIntPoint, FVoxelHydrologyFineWaterResult> Samples;
    FIntPoint Minimum, Maximum, WorldMin, WorldMax, Current;
    int32 MapCapacity = 0;
    int32 PublishCursor = 0;
    bool bQueryStarted = false;
    bool bInitialized = false;
    bool bReady = false;
};
