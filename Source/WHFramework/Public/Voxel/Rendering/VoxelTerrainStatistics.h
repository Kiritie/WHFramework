#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

enum class EVoxelTerrainMetricRepresentation : uint8
{
    None = 0,
    F0,
    F1,
    F2,
    F3Plus,
    Proxy,
    Surface,
    Macro,
    Count
};

enum class EVoxelTerrainFrameMetric : uint8
{
    GameThread,
    TerrainGameThread,
    RenderThread,
    GPU,
    Count
};

struct WHFRAMEWORK_API FVoxelTerrainMetricEvidence
{
    struct FTarget
    {
        uint64 WorldEpoch = 0;
        FIntVector Min = FIntVector::ZeroValue;
        FIntVector Max = FIntVector::ZeroValue;
        EVoxelTerrainMetricRepresentation Representation = EVoxelTerrainMetricRepresentation::None;
        bool operator==(const FTarget&) const = default;
        bool IsValid() const { return WorldEpoch && Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z &&
            Representation > EVoxelTerrainMetricRepresentation::None && Representation < EVoxelTerrainMetricRepresentation::Count; }
        friend uint32 GetTypeHash(const FTarget& InTarget)
        {
            return HashCombineFast(HashCombineFast(::GetTypeHash(InTarget.WorldEpoch), ::GetTypeHash(InTarget.Min)),
                HashCombineFast(::GetTypeHash(InTarget.Max), ::GetTypeHash(uint8(InTarget.Representation))));
        }
    };
    FTarget Target;
    EVoxelTerrainMetricRepresentation Representation = EVoxelTerrainMetricRepresentation::None;
    double NearestDistanceMeters = 0.0;
    bool bWanted = false;
    bool bDataReady = false;
    bool bMeshReady = false;
    bool bPresented = false;
    bool bCollisionReady = false;
    bool bLatestPresented = false;
    int64 OwnershipAtoms = 1;
    int64 PresentedOwnershipAtoms = 0;
    int32 SampleStepCells = 1;
    uint64 Vertices = 0;
    uint64 Triangles = 0;
    uint64 MeshBytes = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainReadinessCounters
{
    int64 Wanted = 0;
    int64 DataReady = 0;
    int64 MeshReady = 0;
    int64 Presented = 0;
    int64 CollisionReady = 0;
    int64 LatestPresented = 0;
    int64 WantedAtoms = 0;
    int64 PresentedAtoms = 0;
    int64 SampleStepTargets[25] = {};
    uint64 Vertices = 0;
    uint64 Triangles = 0;
    uint64 MeshBytes = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainFrameQuantiles
{
    int32 Samples = 0;
    double P50 = 0.0;
    double P95 = 0.0;
    double P99 = 0.0;
    double Maximum = 0.0;
};

struct WHFRAMEWORK_API FVoxelTerrainWorkAccounting
{
    uint64 Completed = 0;
    uint64 Used = 0;
    uint64 Discarded = 0;
    uint64 Unresolved = 0;
    double CompletedMilliseconds = 0.0;
    double UsedMilliseconds = 0.0;
    double DiscardedMilliseconds = 0.0;
    double UnresolvedMilliseconds = 0.0;
    double EffectiveRatio() const;
};

// 诊断镜像不参与 Ready、退休或 owner 判定；只消费真实所有者的事件。
class WHFRAMEWORK_API FVoxelTerrainStatistics
{
public:
    static constexpr int32 DistanceBucketCount = 9;
    static int32 DistanceBucket(double InMeters);
    bool Initialize(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
        const FVoxelGenerationConsumerKey& InConsumer, int32 InMaximumTargets = 65536, int32 InMaximumAttempts = 16384);
    uint64 GetStorageReservationBytes() const;
    int32 GetStorageReservationLeaseCount() const;
    bool CanCompleteWork() const;
    bool RegisterWork(const FGuid& InAttempt);
    void AbandonWork(const FGuid& InAttempt);
    bool SetEvidence(const FVoxelTerrainMetricEvidence& InEvidence);
    void VisitEvidence(int32& InOutCursor, TFunctionRef<FVoxelTerrainMetricEvidence(const FVoxelTerrainMetricEvidence&)> InRefresh);
    void RemoveEvidence(const FVoxelTerrainMetricEvidence::FTarget& InTarget);
    FVoxelTerrainReadinessCounters Get(EVoxelTerrainMetricRepresentation InRepresentation, int32 InDistanceBucket) const;
    bool CompleteWork(const FGuid& InAttempt, double InExecuteMilliseconds);
    void ResolveWork(const FGuid& InAttempt, bool bInUsed);
    void AddFrame(double InFrameMilliseconds, EVoxelTerrainFrameMetric InMetric = EVoxelTerrainFrameMetric::GameThread);
    FVoxelTerrainFrameQuantiles CaptureFrameQuantiles(EVoxelTerrainFrameMetric InMetric = EVoxelTerrainFrameMetric::GameThread) const;
    FVoxelTerrainWorkAccounting GetWorkAccounting() const;
    FString ToCsv() const;
    void Reset();

private:
    struct FAttempt
    {
        double Milliseconds = 0.0;
        bool bComplete = false;
        bool bResolved = false;
    };
    bool RegisterWorkLocked(const FGuid& InAttempt);
    mutable FCriticalSection Mutex;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StorageLease;
    int32 MaximumTargets = 65536;
    int32 MaximumAttempts = 16384;
    int32 NextAttempt = 0;
    void Accumulate(const FVoxelTerrainMetricEvidence& InEvidence, int32 InSign);
    TMap<FVoxelTerrainMetricEvidence::FTarget, FVoxelTerrainMetricEvidence> Evidence;
    TArray<FVoxelTerrainMetricEvidence::FTarget> EvidenceOrder;
    TMap<FVoxelTerrainMetricEvidence::FTarget, int32> EvidenceIndices;
    FVoxelTerrainReadinessCounters Counters[static_cast<uint8>(EVoxelTerrainMetricRepresentation::Count)][DistanceBucketCount];
    TMap<FGuid, FAttempt> Attempts;
    TArray<FGuid> AttemptOrder;
    FVoxelTerrainWorkAccounting Work;
    TArray<double> Frames[uint8(EVoxelTerrainFrameMetric::Count)];
    int32 NextFrame[uint8(EVoxelTerrainFrameMetric::Count)] = {};
};

// 同一个执行身份在所有 continuation 中共享；只在实际使用或最后一个输出引用退休时结算一次。
class WHFRAMEWORK_API FVoxelTerrainWorkReceipt
{
public:
    FVoxelTerrainWorkReceipt(TSharedRef<FVoxelTerrainStatistics, ESPMode::ThreadSafe> InStatistics,
        TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease);
    ~FVoxelTerrainWorkReceipt();
    bool Complete(double InMilliseconds);
    bool IsRegistered() const { return bRegistered; }
    void Resolve(bool bInUsed);
    const FGuid& GetAttemptId() const { return AttemptId; }

private:
    TSharedRef<FVoxelTerrainStatistics, ESPMode::ThreadSafe> Statistics;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
    FGuid AttemptId = FGuid::NewGuid();
    FCriticalSection Mutex;
    bool bComplete = false;
    bool bRegistered = false;
    bool bResolved = false;
};
