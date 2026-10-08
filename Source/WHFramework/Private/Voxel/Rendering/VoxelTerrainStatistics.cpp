#include "Voxel/Rendering/VoxelTerrainStatistics.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Misc/ScopeLock.h"

bool FVoxelTerrainStatistics::Initialize(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
    const FVoxelGenerationConsumerKey& InConsumer, const int32 InMaximumTargets, const int32 InMaximumAttempts)
{
    FScopeLock Lock(&Mutex);
    if (StorageLease || !Evidence.IsEmpty() || !Attempts.IsEmpty() || InMaximumTargets < 1 || InMaximumTargets > 1048576 ||
        InMaximumAttempts < 1 || InMaximumAttempts > 1048576) return false;
    const uint64 Bytes = VoxelBoundedAllocation::MapBytes<FVoxelTerrainMetricEvidence::FTarget, FVoxelTerrainMetricEvidence>(InMaximumTargets) +
        VoxelBoundedAllocation::MapBytes<FVoxelTerrainMetricEvidence::FTarget, int32>(InMaximumTargets) +
        VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainMetricEvidence::FTarget>(InMaximumTargets) +
        VoxelBoundedAllocation::MapBytes<FGuid, FAttempt>(InMaximumAttempts) +
        VoxelBoundedAllocation::ArrayBytes<FGuid>(InMaximumAttempts) + uint8(EVoxelTerrainFrameMetric::Count) * VoxelBoundedAllocation::ArrayBytes<double>(4096);
    if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident, Bytes, StorageLease)) return false;
    MaximumTargets = InMaximumTargets;
    MaximumAttempts = InMaximumAttempts;
    Evidence.Reserve(MaximumTargets);
    EvidenceIndices.Reserve(MaximumTargets);
    EvidenceOrder.Reserve(MaximumTargets);
    Attempts.Reserve(MaximumAttempts);
    AttemptOrder.Reserve(MaximumAttempts);
    for (auto& Samples : Frames) Samples.Reserve(4096);
    return true;
}

bool FVoxelTerrainStatistics::CanCompleteWork() const
{
    FScopeLock Lock(&Mutex);
    if (AttemptOrder.Num() < MaximumAttempts) return true;
    const auto* Old = Attempts.Find(AttemptOrder[NextAttempt]);
    return Old && Old->bResolved;
}

uint64 FVoxelTerrainStatistics::GetStorageReservationBytes() const
{
    FScopeLock Lock(&Mutex);
    return StorageLease && StorageLease->GetPool() == EVoxelTerrainResourcePool::Input ? StorageLease->Bytes() : 0;
}

int32 FVoxelTerrainStatistics::GetStorageReservationLeaseCount() const
{
    FScopeLock Lock(&Mutex);
    return StorageLease ? 1 : 0;
}

bool FVoxelTerrainStatistics::RegisterWorkLocked(const FGuid& InAttempt)
{
    if (!InAttempt.IsValid() || Attempts.Contains(InAttempt)) return false;
    if (AttemptOrder.Num() == MaximumAttempts)
    {
        // 未结算记录跨统计窗口保留；每次接纳只扫描固定数量，下一次从游标继续。
        bool bFound = false;
        for (int32 Scan = 0; Scan < FMath::Min(64, MaximumAttempts); ++Scan)
        {
            const int32 Index = NextAttempt;
            NextAttempt = (NextAttempt + 1) % MaximumAttempts;
            const auto* Old = Attempts.Find(AttemptOrder[Index]);
            if (!Old || !Old->bResolved) continue;
            Attempts.Remove(AttemptOrder[Index]);
            AttemptOrder[Index] = InAttempt;
            bFound = true;
            break;
        }
        if (!bFound) return false;
    }
    else AttemptOrder.Add(InAttempt);
    Attempts.Add(InAttempt, {});
    return true;
}

bool FVoxelTerrainStatistics::RegisterWork(const FGuid& InAttempt)
{
    FScopeLock Lock(&Mutex);
    return RegisterWorkLocked(InAttempt);
}

void FVoxelTerrainStatistics::AbandonWork(const FGuid& InAttempt)
{
    FScopeLock Lock(&Mutex);
    if (auto* Attempt = Attempts.Find(InAttempt); Attempt && !Attempt->bComplete) Attempt->bResolved = true;
}

double FVoxelTerrainWorkAccounting::EffectiveRatio() const
{
    return CompletedMilliseconds > 0.0 ? UsedMilliseconds / CompletedMilliseconds : 0.0;
}

int32 FVoxelTerrainStatistics::DistanceBucket(const double InMeters)
{
    const double Limits[DistanceBucketCount - 1] = {8.0, 20.0, 64.0, 256.0, 400.0, 800.0, 2000.0, 4000.0};
    for (int32 Index = 0; Index < DistanceBucketCount - 1; ++Index)
    {
        if (InMeters < Limits[Index])
        {
            return Index;
        }
    }
    return DistanceBucketCount - 1;
}

void FVoxelTerrainStatistics::Accumulate(const FVoxelTerrainMetricEvidence& InEvidence, const int32 InSign)
{
    auto& Counter = Counters[uint8(InEvidence.Representation)][DistanceBucket(InEvidence.NearestDistanceMeters)];
    Counter.Wanted += InSign * int32(InEvidence.bWanted);
    Counter.DataReady += InSign * int32(InEvidence.bDataReady);
    Counter.MeshReady += InSign * int32(InEvidence.bMeshReady);
    Counter.Presented += InSign * int32(InEvidence.bPresented);
    Counter.CollisionReady += InSign * int32(InEvidence.bCollisionReady);
    Counter.LatestPresented += InSign * int32(InEvidence.bLatestPresented);
    Counter.WantedAtoms += InSign * InEvidence.OwnershipAtoms * int32(InEvidence.bWanted);
    Counter.PresentedAtoms += InSign * InEvidence.PresentedOwnershipAtoms;
    Counter.SampleStepTargets[FMath::FloorLog2(uint32(InEvidence.SampleStepCells))] += InSign;
    if (InSign > 0)
    {
        Counter.Vertices += InEvidence.Vertices;
        Counter.Triangles += InEvidence.Triangles;
        Counter.MeshBytes += InEvidence.MeshBytes;
    }
    else
    {
        Counter.Vertices -= InEvidence.Vertices;
        Counter.Triangles -= InEvidence.Triangles;
        Counter.MeshBytes -= InEvidence.MeshBytes;
    }
}

bool FVoxelTerrainStatistics::SetEvidence(const FVoxelTerrainMetricEvidence& InEvidence)
{
    FScopeLock Lock(&Mutex);
    if (!InEvidence.Target.IsValid() || InEvidence.Representation == EVoxelTerrainMetricRepresentation::None ||
        InEvidence.Representation >= EVoxelTerrainMetricRepresentation::Count ||
        !FMath::IsFinite(InEvidence.NearestDistanceMeters) || InEvidence.NearestDistanceMeters < 0.0 ||
        InEvidence.OwnershipAtoms < 1 || InEvidence.PresentedOwnershipAtoms < 0 ||
        InEvidence.SampleStepCells < 1 || InEvidence.SampleStepCells > (1 << 24) || !FMath::IsPowerOfTwo(InEvidence.SampleStepCells) ||
        InEvidence.PresentedOwnershipAtoms > InEvidence.OwnershipAtoms || InEvidence.Target.Representation != InEvidence.Representation)
    {
        return false;
    }
    const auto* Previous = Evidence.Find(InEvidence.Target);
    if (!Previous && Evidence.Num() >= MaximumTargets) return false;
    if (Previous)
    {
        Accumulate(*Previous, -1);
    }
    else EvidenceIndices.Add(InEvidence.Target, EvidenceOrder.Add(InEvidence.Target));
    Evidence.Add(InEvidence.Target, InEvidence);
    Accumulate(InEvidence, 1);
    return true;
}

void FVoxelTerrainStatistics::VisitEvidence(int32& InOutCursor,
    TFunctionRef<FVoxelTerrainMetricEvidence(const FVoxelTerrainMetricEvidence&)> InRefresh)
{
    FVoxelTerrainMetricEvidence Sample;
    {
        FScopeLock Lock(&Mutex);
        if (EvidenceOrder.IsEmpty()) { InOutCursor = 0; return; }
        InOutCursor %= EvidenceOrder.Num();
        Sample = Evidence.FindChecked(EvidenceOrder[InOutCursor++]);
    }
    auto Refreshed = InRefresh(Sample);
    if (!Refreshed.Target.IsValid()) RemoveEvidence(Sample.Target);
    else SetEvidence(Refreshed);
}

void FVoxelTerrainStatistics::RemoveEvidence(const FVoxelTerrainMetricEvidence::FTarget& InTarget)
{
    FScopeLock Lock(&Mutex);
    if (const auto* Previous = Evidence.Find(InTarget))
    {
        Accumulate(*Previous, -1);
        Evidence.Remove(InTarget);
        int32 Index = INDEX_NONE;
        check(EvidenceIndices.RemoveAndCopyValue(InTarget, Index));
        EvidenceOrder.RemoveAtSwap(Index, 1, EAllowShrinking::No);
        if (EvidenceOrder.IsValidIndex(Index)) EvidenceIndices.FindChecked(EvidenceOrder[Index]) = Index;
    }
}

FVoxelTerrainReadinessCounters FVoxelTerrainStatistics::Get(const EVoxelTerrainMetricRepresentation InRepresentation,
    const int32 InDistanceBucket) const
{
    FScopeLock Lock(&Mutex);
    check(InRepresentation > EVoxelTerrainMetricRepresentation::None && InRepresentation < EVoxelTerrainMetricRepresentation::Count);
    check(InDistanceBucket >= 0 && InDistanceBucket < DistanceBucketCount);
    return Counters[uint8(InRepresentation)][InDistanceBucket];
}

bool FVoxelTerrainStatistics::CompleteWork(const FGuid& InAttempt, const double InExecuteMilliseconds)
{
    FScopeLock Lock(&Mutex);
    if (!InAttempt.IsValid() || !FMath::IsFinite(InExecuteMilliseconds) || InExecuteMilliseconds < 0.0)
    {
        return false;
    }
    if (!Attempts.Contains(InAttempt) && !RegisterWorkLocked(InAttempt)) return false;
    auto& Attempt = Attempts.FindChecked(InAttempt);
    if (Attempt.bComplete || Attempt.bResolved) return false;
    Attempt.Milliseconds = InExecuteMilliseconds;
    Attempt.bComplete = true;
    ++Work.Completed;
    Work.CompletedMilliseconds += InExecuteMilliseconds;
    ++Work.Unresolved;
    Work.UnresolvedMilliseconds += InExecuteMilliseconds;
    return true;
}

void FVoxelTerrainStatistics::ResolveWork(const FGuid& InAttempt, const bool bInUsed)
{
    FScopeLock Lock(&Mutex);
    auto* Attempt = Attempts.Find(InAttempt);
    if (!Attempt || !Attempt->bComplete || Attempt->bResolved)
    {
        return;
    }
    if (bInUsed)
    {
        ++Work.Used;
        Work.UsedMilliseconds += Attempt->Milliseconds;
    }
    else
    {
        ++Work.Discarded;
        Work.DiscardedMilliseconds += Attempt->Milliseconds;
    }
    --Work.Unresolved;
    Work.UnresolvedMilliseconds = FMath::Max(0.0, Work.UnresolvedMilliseconds - Attempt->Milliseconds);
    Attempt->bResolved = true;
}

void FVoxelTerrainStatistics::AddFrame(const double InFrameMilliseconds, const EVoxelTerrainFrameMetric InMetric)
{
    FScopeLock Lock(&Mutex);
    if (!FMath::IsFinite(InFrameMilliseconds) || InFrameMilliseconds < 0.0 || InMetric >= EVoxelTerrainFrameMetric::Count)
    {
        return;
    }
    constexpr int32 Capacity = 4096;
    auto& Samples = Frames[uint8(InMetric)];
    auto& Cursor = NextFrame[uint8(InMetric)];
    if (Samples.Num() < Capacity)
    {
        Samples.Add(InFrameMilliseconds);
        return;
    }
    Samples[Cursor] = InFrameMilliseconds;
    Cursor = (Cursor + 1) % Capacity;
}

FVoxelTerrainFrameQuantiles FVoxelTerrainStatistics::CaptureFrameQuantiles(const EVoxelTerrainFrameMetric InMetric) const
{
    FScopeLock Lock(&Mutex);
    FVoxelTerrainFrameQuantiles Result;
    if (InMetric >= EVoxelTerrainFrameMetric::Count) return Result;
    const auto& Samples = Frames[uint8(InMetric)];
    Result.Samples = Samples.Num();
    if (Samples.IsEmpty())
    {
        return Result;
    }
    TArray<double, TInlineAllocator<4096>> Sorted;
    Sorted.Append(Samples);
    Sorted.Sort();
    const auto Quantile = [&Sorted](const double Fraction)
    {
        return Sorted[FMath::Clamp(FMath::CeilToInt(Fraction * Sorted.Num()) - 1, 0, Sorted.Num() - 1)];
    };
    Result.P50 = Quantile(0.50);
    Result.P95 = Quantile(0.95);
    Result.P99 = Quantile(0.99);
    Result.Maximum = Sorted.Last();
    return Result;
}

FVoxelTerrainWorkAccounting FVoxelTerrainStatistics::GetWorkAccounting() const
{
    FScopeLock Lock(&Mutex);
    return Work;
}

FString FVoxelTerrainStatistics::ToCsv() const
{
    FScopeLock Lock(&Mutex);
    FString Csv = TEXT("representation,distance_bucket,wanted,data_ready,mesh_ready,presented,latest_presented,collision_ready,wanted_atoms,presented_atoms,min_step_cells,max_step_cells,latest_mesh_vertices,latest_mesh_triangles,latest_mesh_bytes\n");
    for (uint8 Representation = 1; Representation < uint8(EVoxelTerrainMetricRepresentation::Count); ++Representation)
    {
        for (int32 Bucket = 0; Bucket < DistanceBucketCount; ++Bucket)
        {
            const auto& Counter = Counters[Representation][Bucket];
            int32 MinimumStep = 0, MaximumStep = 0;
            for (int32 Step = 0; Step < UE_ARRAY_COUNT(Counter.SampleStepTargets); ++Step)
                if (Counter.SampleStepTargets[Step] > 0) { if (!MinimumStep) MinimumStep = 1 << Step; MaximumStep = 1 << Step; }
            Csv += FString::Printf(TEXT("%u,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%llu,%llu,%llu\n"), Representation, Bucket,
                Counter.Wanted, Counter.DataReady, Counter.MeshReady, Counter.Presented, Counter.LatestPresented,
                Counter.CollisionReady, Counter.WantedAtoms, Counter.PresentedAtoms, MinimumStep, MaximumStep,
                Counter.Vertices, Counter.Triangles, Counter.MeshBytes);
        }
    }
    return Csv;
}

void FVoxelTerrainStatistics::Reset()
{
    FScopeLock Lock(&Mutex);
    Evidence.Reset();
    EvidenceOrder.Reset();
    EvidenceIndices.Reset();
    Attempts.Reset();
    AttemptOrder.Reset();
    NextAttempt = 0;
    Work = {};
    for (auto& Samples : Frames) Samples.Reset();
    for (auto& Cursor : NextFrame) Cursor = 0;
    for (auto& Representation : Counters)
    {
        for (auto& Counter : Representation)
        {
            Counter = {};
        }
    }
}

FVoxelTerrainWorkReceipt::FVoxelTerrainWorkReceipt(TSharedRef<FVoxelTerrainStatistics, ESPMode::ThreadSafe> InStatistics,
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
    : Statistics(InStatistics), MetadataLease(MoveTemp(InLease))
{
    bRegistered = Statistics->RegisterWork(AttemptId);
}

FVoxelTerrainWorkReceipt::~FVoxelTerrainWorkReceipt()
{
    if (bComplete) Resolve(false);
    else if (bRegistered) Statistics->AbandonWork(AttemptId);
}

bool FVoxelTerrainWorkReceipt::Complete(const double InMilliseconds)
{
    FScopeLock Lock(&Mutex);
    if (!bRegistered || bComplete) return false;
    bComplete = Statistics->CompleteWork(AttemptId, InMilliseconds);
    return bComplete;
}

void FVoxelTerrainWorkReceipt::Resolve(const bool bInUsed)
{
    FScopeLock Lock(&Mutex);
    if (!bComplete || bResolved) return;
    Statistics->ResolveWork(AttemptId, bInUsed);
    bResolved = true;
}
