#include "Voxel/Tasks/VoxelTaskScheduler.h"
#include "Voxel/Rendering/VoxelProxyData.h"
#include "Voxel/Rendering/VoxelRenderPage.h"
#include "Voxel/Rendering/VoxelDetailView.h"
uint64 FVoxelTaskResult::ResultBytes() const
{
    uint64 N = Base.PayloadBytes() + Mesh.Bytes() + uint64(Collision.Boxes.Num()) * sizeof(FBox) + Bytes.Num();
    for (const auto& B : RemoteBases) N += B.PayloadBytes();
    auto OverlaySize = [](const FVoxelSectionOverlay& O)
    {
        uint64 Size = uint64(O.Blocks.Num()) * 16;
        for (const auto& E : O.Entities) Size += E.Value.Payload.Num() + 64;
        return Size;
    };
    for (const auto& O : RemoteOverlays) N += OverlaySize(O);
    N += OverlaySize(Overlay);
    if (Proxy) N += sizeof(FVoxelProxyReply);
    if (ViewGrid) N += sizeof(VoxelView::Grid);
    if (ViewPlan) N += ViewPlan->size() * sizeof(VoxelView::Key);
    if (ViewMesh) N += ViewMesh->Bytes();
    if (ViewPage) N += ViewPage->Bytes();
    if (Details) N += Details->Bytes();
    return N;
}
FVoxelTaskScheduler::FVoxelTaskScheduler()
{
    MaxRunning = FMath::Clamp(FPlatformMisc::NumberOfWorkerThreadsToSpawn() / 2, 1, 8);
}
FVoxelTaskScheduler::~FVoxelTaskScheduler() { StopAndJoin(); }
bool FVoxelTaskScheduler::Has(const FVoxelTaskStamp& S, EVoxelTaskKind K) const
{
    for (const auto& P : Pending) if (P.Kind == K && P.Stamp == S) return true;
    for (const auto& P : Running) if (P.Kind == K && P.Stamp == S && !P.Slot->Cancel.load()) return true;
    return false;
}
bool FVoxelTaskScheduler::Enqueue(FVoxelTaskRequest&& R)
{
    check(IsInGameThread());
    if (bStopped || !R.Execute || !FMath::IsFinite(R.Priority) || !R.ReservedBytes ||
        R.ReservedBytes > MaxReserved || R.InputBytes > MaxPendingInput) return false;
    if (Has(R.Stamp, R.Kind)) return true;
    // Derived tasks are keyed by their request token as well as level/peer at the owning presenter.
    // A bare section coordinate is NOT a valid deduplication key for them.
    if (!VoxelIsDerivedTask(R.Kind))
    {
        for (int32 I = Pending.Num() - 1; I >= 0; --I)
            if (Pending[I].Stamp.Key == R.Stamp.Key && Pending[I].Kind == R.Kind)
            {
                FVoxelTaskResult X; X.Stamp = Pending[I].Stamp; X.Kind = Pending[I].Kind; X.bCanceled = true;
                Canceled.Add(MoveTemp(X)); QueuedInput -= Pending[I].InputBytes; Pending.RemoveAtSwap(I);
            }
    }
    if (Pending.Num() >= 1024 || R.InputBytes > MaxPendingInput - QueuedInput) return false;
    if (!VoxelIsDerivedTask(R.Kind))
        for (auto& P : Running)
            if (P.Stamp.Key == R.Stamp.Key && P.Kind == R.Kind) P.Slot->Cancel.store(true);
    R.QueuedAt = FPlatformTime::Seconds(); QueuedInput += R.InputBytes; Pending.Add(MoveTemp(R)); return true;
}
void FVoxelTaskScheduler::Pump()
{
    while (!bStopped && Running.Num() < MaxRunning && !Pending.IsEmpty())
    {
        int32 Best = INDEX_NONE; double Score = DBL_MAX, Now = FPlatformTime::Seconds();
        for (int32 I = 0; I < Pending.Num(); ++I)
        {
            const auto& P = Pending[I]; if (P.ReservedBytes > MaxReserved - Reserved) continue;
            const double S = P.Priority - (Now - P.QueuedAt) * 100;
            if (S < Score) { Score = S; Best = I; }
        }
        if (Best == INDEX_NONE) break;
        FVoxelTaskRequest Q = MoveTemp(Pending[Best]); Pending.RemoveAtSwap(Best);
        QueuedInput -= Q.InputBytes; Reserved += Q.ReservedBytes;
        FRunning R; R.Stamp = Q.Stamp; R.Kind = Q.Kind; R.ReservedBytes = Q.ReservedBytes;
        R.Slot = MakeShared<FSlot, ESPMode::ThreadSafe>();
        auto Slot = R.Slot; const auto Stamp = R.Stamp; const auto Kind = R.Kind; const uint64 Max = R.ReservedBytes;
        R.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Slot, Stamp, Kind, Max, Fn = MoveTemp(Q.Execute)]() mutable
        {
            if (!Slot->Cancel.load()) Slot->Result = Fn(Slot->Cancel);
            Slot->Result.Stamp = Stamp; Slot->Result.Kind = Kind; Slot->Result.bCanceled = Slot->Cancel.load();
            if (Slot->Result.ResultBytes() > Max)
            {
                Slot->Result = FVoxelTaskResult(); Slot->Result.Stamp = Stamp; Slot->Result.Kind = Kind;
                Slot->Result.Error = TEXT("Task result memory budget exceeded");
            }
        });
        Running.Add(MoveTemp(R));
    }
}
void FVoxelTaskScheduler::Tick(TFunctionRef<void(FVoxelTaskResult&&)> Apply, double Ms)
{
    check(IsInGameThread()); if (bStopped) return;
    const double End = FPlatformTime::Seconds() + FMath::Max(0.0, Ms) / 1000;
    for (int32 I = 0; I < Running.Num() && FPlatformTime::Seconds() <= End;)
    {
        if (!Running[I].Task.IsCompleted()) { ++I; continue; }
        FRunning R = MoveTemp(Running[I]); Running.RemoveAtSwap(I); Reserved -= R.ReservedBytes;
        Apply(MoveTemp(R.Slot->Result));
    }
    while (!Canceled.IsEmpty() && FPlatformTime::Seconds() <= End)
    {
        auto C = MoveTemp(Canceled.Last()); Canceled.Pop(EAllowShrinking::No); Apply(MoveTemp(C));
    }
    Pump();
}
void FVoxelTaskScheduler::CancelSection(const FVoxelSectionKey& K)
{
    check(IsInGameThread());
    for (int32 I = Pending.Num() - 1; I >= 0; --I)
        if (!VoxelIsDerivedTask(Pending[I].Kind) && Pending[I].Stamp.Key == K)
        {
            FVoxelTaskResult C; C.Stamp = Pending[I].Stamp; C.Kind = Pending[I].Kind; C.bCanceled = true;
            Canceled.Add(MoveTemp(C)); QueuedInput -= Pending[I].InputBytes; Pending.RemoveAtSwap(I);
        }
    for (auto& R : Running)
        if (!VoxelIsDerivedTask(R.Kind) && R.Stamp.Key == K) R.Slot->Cancel.store(true);
}
void FVoxelTaskScheduler::StopAndJoin()
{
    if (bStopped && Running.IsEmpty()) return;
    bStopped = true; Pending.Reset(); Canceled.Reset(); QueuedInput = 0;
    for (auto& R : Running) R.Slot->Cancel.store(true);
    for (auto& R : Running) R.Task.Wait();
    Running.Reset(); Reserved = 0;
}
