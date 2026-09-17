#pragma once
#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Tasks/VoxelTaskTypes.h"
#include "Voxel/Rendering/Kernel/VoxelViewPlan.h"
#include <atomic>
struct FVoxelProxyReply;
struct FVoxelLodBuiltMesh;
struct FVoxelRenderPage;
struct FVoxelDetailPlan;
struct WHFRAMEWORK_API FVoxelTaskResult
{
    FVoxelTaskStamp Stamp;
    EVoxelTaskKind Kind = EVoxelTaskKind::Generate;
    bool bSuccess = false, bCanceled = false;
    FString Error;
    FVoxelSectionStorage Base;
    FVoxelSectionOverlay Overlay;
    FVoxelSectionMeshResult Mesh;
    FVoxelSectionCollisionResult Collision;
    TArray<uint8> Bytes;
    FGuid BatchId;
    TArray<FVoxelTaskStamp> RemoteStamps;
    TArray<FVoxelSectionOverlay> RemoteOverlays;
    TArray<FVoxelSectionStorage> RemoteBases;
    TSharedPtr<FVoxelProxyReply, ESPMode::ThreadSafe> Proxy;
    std::shared_ptr<const VoxelView::Grid> ViewGrid;
    std::shared_ptr<const std::vector<VoxelView::Key>> ViewPlan;
    TSharedPtr<FVoxelLodBuiltMesh, ESPMode::ThreadSafe> ViewMesh;
    TSharedPtr<FVoxelRenderPage, ESPMode::ThreadSafe> ViewPage;
    TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Details;
    uint64 ResultBytes() const;
};
struct WHFRAMEWORK_API FVoxelTaskRequest
{
    FVoxelTaskStamp Stamp;
    EVoxelTaskKind Kind = EVoxelTaskKind::Generate;
    double Priority = 0, QueuedAt = 0;
    uint64 ReservedBytes = 0, InputBytes = 0;
    TFunction<FVoxelTaskResult(const std::atomic_bool&)> Execute;
};
class WHFRAMEWORK_API FVoxelTaskScheduler
{
public:
    FVoxelTaskScheduler();
    ~FVoxelTaskScheduler();
    FVoxelTaskScheduler(const FVoxelTaskScheduler&) = delete;
    FVoxelTaskScheduler& operator=(const FVoxelTaskScheduler&) = delete;
    bool Enqueue(FVoxelTaskRequest&& Request);
    void Tick(TFunctionRef<void(FVoxelTaskResult&&)> Apply, double MaxApplyMilliseconds = 2);
    void CancelSection(const FVoxelSectionKey& Key);
    void StopAndJoin();
    bool Has(const FVoxelTaskStamp& Stamp, EVoxelTaskKind Kind) const;
    int32 ActiveCount() const { return Running.Num(); }
private:
    struct FSlot { std::atomic_bool Cancel{false}; FVoxelTaskResult Result; };
    struct FRunning
    {
        FVoxelTaskStamp Stamp;
        EVoxelTaskKind Kind;
        uint64 ReservedBytes = 0;
        TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot;
        UE::Tasks::FTask Task;
    };
    void Pump();
    TArray<FVoxelTaskRequest> Pending;
    TArray<FRunning> Running;
    TArray<FVoxelTaskResult> Canceled;
    bool bStopped = false;
    int32 MaxRunning = 2;
    uint64 Reserved = 0, QueuedInput = 0;
    static constexpr uint64 MaxReserved = 128ull * 1024 * 1024;
    static constexpr uint64 MaxPendingInput = 32ull * 1024 * 1024;
};
