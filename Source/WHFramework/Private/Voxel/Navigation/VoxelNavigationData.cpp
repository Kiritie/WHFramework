#include "Voxel/Navigation/VoxelNavigationData.h"

#include "AI/Navigation/NavigationElement.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "NavigationSystem.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Navigation/VoxelNavigationSystem.h"
#include "Voxel/Navigation/VoxelNavigationRepathControl.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

#if WITH_RECAST
#include "NavMesh/RecastNavMeshGenerator.h"
#include "NavMesh/RecastQueryFilter.h"
#include "NavMesh/NavMeshPath.h"
#include "Detour/DetourNavMesh.h"
#endif

struct FVoxelNavigationTileCapture
{
	struct FSourceInput
	{
		TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> Surface;
		TWeakObjectPtr<const UVoxelNavigationSurfaceComponent> Component;
		FTransform Transform;
	};
	TArray<FSourceInput> Surfaces;
	TArray<FNavTileRef> NativeTiles;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FGuid EvidenceOwnerId;
	FIntPoint Coordinate;
	uint64 WorldEpoch = 0;
	mutable FVoxelNavigationInvalidationSource Access;
};

class FVoxelNavigationNativeQueryScope
{
public:
	explicit FVoxelNavigationNativeQueryScope(const AVoxelNavigationData* InData)
		: Data(const_cast<AVoxelNavigationData*>(InData)), bEntered(Data.Get() != nullptr && Data->BeginTerrainQuery())
	{
	}

	~FVoxelNavigationNativeQueryScope()
	{
		if (bEntered) Data->FinishTerrainQuery();
	}

	bool IsEntered() const { return bEntered; }
	FVoxelNavigationNativeQueryScope(const FVoxelNavigationNativeQueryScope&) = delete;
	FVoxelNavigationNativeQueryScope& operator=(const FVoxelNavigationNativeQueryScope&) = delete;

private:
	TStrongObjectPtr<AVoxelNavigationData> Data;
	bool bEntered = false;
};

struct FVoxelNavigationAsyncQueryTicket
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FSharedConstNavQueryFilter OriginalFilter;
	TWeakPtr<const FNavigationQueryFilter, ESPMode::ThreadSafe> MarkedFilter;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> CompletionInputs;
	TSharedPtr<FVoxelNavigationRepathControl, ESPMode::ThreadSafe> Repath;
	FNavPathWeakPtr CompletionPath;
	FGuid BindingId;
	FGuid ProducerId;
	uint64 WorldEpoch = 0;
	uint32 QueryId = INVALID_NAVQUERYID;
	// Completion字段由本请求的worker写入，GT仅在整个Task完成后读取。
	bool bCompleted = false;
	bool bReadyAtCompletion = false;
};

#if WITH_RECAST
struct FVoxelNavigationQueryInputs : IVoxelNavigationInvalidationConsumer
{
	struct FSurfaceReader
	{
		FVoxelNavigationSurfaceReadLease Lease;
		FVoxelNavigationInvalidationObserver Observer;
		bool bPathInput = false;
	};
	struct FTileReader
	{
		TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe> Capture;
		FVoxelNavigationInvalidationObserver Observer;
		bool bPathInput = false;
	};
	TArray<FSurfaceReader> Readers;
	TMap<const FVoxelNavigationSurface*, int32> ReaderIndices;
	TMap<FIntVector, int32> ReaderSections;
	TArray<FTileReader> Tiles;
	TMap<const FVoxelNavigationTileCapture*, int32> TileIndices;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> PreviousAttempt;
	TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> Self;
	FGuid AttemptId;
	FCriticalSection PublicationLock;
	FNavPathWeakPtr Path;
	TAtomic<bool> bRevoked{false};
	// 实际过滤过程中遇到缺失或退役输入，不能把该次无路结果解释为最终不可达。
	bool bMissingInput = false;
	// Searching 的路径尚在 Worker 填充；Published 才允许 GT 修改引擎有效状态。
	enum class EState : uint8 { Searching, Published, Retired };
	EState State = EState::Searching;

	bool AcquireReader(FVoxelNavigationSurfaceReadLease& OutReader,
		TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface)
	{
		if (OutReader.Acquire(InSurface)) return true;
		if (!PreviousAttempt) return false;
		const int32* Index = PreviousAttempt->ReaderSections.Find(InSurface->InputVersion.Section);
		return Index && OutReader.AcquireContinuation(MoveTemp(InSurface), PreviousAttempt->Readers[*Index].Lease);
	}

	bool Retain(TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe> InCapture)
	{
		if (bRevoked.Load() || !InCapture || !InCapture->Access.IsCurrent()) return false;
		if (!TileIndices.Contains(InCapture.Get()))
		{
			const int32 Index = Tiles.AddDefaulted();
			Tiles[Index].Capture = InCapture;
			if (!Tiles[Index].Observer.Attach(InCapture->Access, Self))
			{
				bRevoked.Store(true);
				return false;
			}
			TileIndices.Add(InCapture.Get(), Index);
		}
		for (const auto& Input : InCapture->Surfaces)
		{
			if (const int32* Existing = ReaderIndices.Find(Input.Surface.Get()))
			{
				if (!Readers[*Existing].Lease.IsCurrent()) return false;
				continue;
			}
			const int32 Index = Readers.AddDefaulted();
			if (!AcquireReader(Readers[Index].Lease, Input.Surface) ||
				!Input.Surface->Access->Observe(Readers[Index].Observer, Self))
			{
				bRevoked.Store(true);
				return false;
			}
			ReaderIndices.Add(Input.Surface.Get(), Index);
			ReaderSections.Add(Input.Surface->InputVersion.Section, Index);
		}
		return !bRevoked.Load();
	}

	bool IsCurrent() const
	{
		if (bRevoked.Load() || Readers.IsEmpty() || Tiles.IsEmpty()) return false;
		for (const auto& Reader : Readers)
		{
			if (Reader.Lease.Get() && !Reader.Lease.IsCurrent()) return false;
		}
		for (const auto& Tile : Tiles)
		{
			if (Tile.Capture && !Tile.Capture->Access.IsCurrent()) return false;
		}
		return true;
	}

	bool KeepPathNode(const AVoxelNavigationData& InData, const NavNodeRef InNode)
	{
		if (!InNode) return false;
		const auto* Capture = InData.FindUsableNodeCapture(InNode);
		const int32* TileIndex = Capture ? TileIndices.Find(Capture) : nullptr;
		if (!TileIndex) return false;
		Tiles[*TileIndex].bPathInput = true;
		for (const auto& Input : Capture->Surfaces)
		{
			const int32* Index = ReaderIndices.Find(Input.Surface.Get());
			if (!Index) return false;
			Readers[*Index].bPathInput = true;
		}
		return true;
	}

	bool PublishPath(const AVoxelNavigationData& InData, const FNavPathSharedPtr& InPath)
	{
		if (!IsCurrent() || !InPath || (!InPath->IsReady() && InPath->GetPathPoints().Num() != 1)) return false;
		const auto* Mesh = InPath->CastPath<FNavMeshPath>();
		if (!Mesh) return false;
		bool bHasNativeNode = false;
		for (const auto Node : Mesh->PathCorridor)
		{
			if (!KeepPathNode(InData, Node)) return false;
			bHasNativeNode = true;
		}
		for (const auto& Point : InPath->GetPathPoints())
		{
			if (!Point.NodeRef) continue;
			if (!KeepPathNode(InData, Point.NodeRef)) return false;
			bHasNativeNode = true;
		}
		// 引擎 Start==End 不写节点，但前置投影已由本次过滤器捕获实际输入。
		if (bHasNativeNode)
		{
			for (auto& Reader : Readers)
			{
				if (!Reader.bPathInput)
				{
					Reader.Observer.Detach();
					Reader.Lease.Reset();
				}
			}
			for (auto& Tile : Tiles)
			{
				if (!Tile.bPathInput)
				{
					Tile.Observer.Detach();
					Tile.Capture.Reset();
				}
			}
		}
		// 搜索/收缩阶段不持有发布锁；GT撤销只标记意图，不等待整个走廊处理。
		FScopeLock Lock(&PublicationLock);
		if (!IsCurrent()) return false;
		Path = InPath;
		State = EState::Published;
		return true;
	}

	void SuspendForRepath()
	{
		FScopeLock Lock(&PublicationLock);
		State = EState::Retired;
		Path.Reset();
	}

	bool CanContinue(const FNavPathSharedPtr& InPath)
	{
		FScopeLock Lock(&PublicationLock);
		return State != EState::Searching && Path.Pin() == InPath;
	}

	void RestoreWaitingPath(const FNavPathSharedPtr& InPath)
	{
		FScopeLock Lock(&PublicationLock);
		Path = InPath;
		State = EState::Retired;
	}

	bool CanRetire()
	{
		FScopeLock Lock(&PublicationLock);
		if (State == EState::Searching) return false;
		const auto Pinned = Path.Pin();
		// 失效只结束旧版本可用性；仍被使用者持有的路径继续拥有Nav语义需求。
		return !Pinned;
	}

	virtual void OnNavigationInputRevoked() override
	{
		RevokePath(true);
	}

	void RevokePath(const bool bInAllowRepath)
	{
		bRevoked.Store(true);
		FNavPathSharedPtr Pinned;
		{
			FScopeLock Lock(&PublicationLock);
			if (State != EState::Published) return;
			check(IsInGameThread());
			State = EState::Retired;
			Pinned = Path.Pin();
		}
		// 引擎路径Observer可能立即重新寻路；不得在用户回调期间持有本控制的发布锁。
		if (Pinned)
		{
			if (!Pinned->IsUpToDate()) return;
			// 即使调用者忽略普通导航更新，也不能保留已退役的真实原生引用。
			const bool bIgnored = Pinned->GetIgnoreInvalidation();
			const bool bAutoUpdate = Pinned->WillRecalculateOnInvalidation();
			if (!bInAllowRepath) Pinned->EnableRecalculationOnInvalidation(false);
			Pinned->SetIgnoreInvalidation(false);
			Pinned->Invalidate();
			Pinned->SetIgnoreInvalidation(bIgnored);
			if (!bInAllowRepath) Pinned->EnableRecalculationOnInvalidation(bAutoUpdate);
		}
	}
};

/** 保留调用者过滤与成本逻辑，只附加真实原生节点的当前体素输入约束。 */
class FVoxelNavigationGuardFilter final : public FRecastQueryFilter
{
public:
	FVoxelNavigationGuardFilter(const AVoxelNavigationData& InNavigation,
		FSharedConstNavQueryFilter InSource, TSharedRef<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> InInputs)
		: FRecastQueryFilter(*static_cast<const FRecastQueryFilter*>(InSource->GetImplementation())),
		Navigation(InNavigation), Source(MoveTemp(InSource)), Inputs(MoveTemp(InInputs))
	{
		SetIsVirtual(true);
	}

	virtual FVector GetAdjustedEndLocation(const FVector& InEnd) const override
	{
		return Source->GetAdjustedEndLocation(InEnd);
	}

	virtual float GetHeuristicScale() const override
	{
		return Source->GetHeuristicScale();
	}

protected:
	virtual bool passVirtualFilter(const dtPolyRef InRef, const dtMeshTile* InTile, const dtPoly* InPoly) const override
	{
		const auto* Capture = Navigation.FindUsableNodeCapture(NavNodeRef(InRef));
		if (!Capture)
		{
			Inputs->bMissingInput = true;
			return false;
		}
		const auto* Native = Navigation.GetRecastMesh();
		const auto* Entry = Native ? Navigation.CommittedTiles.Find(int32(Native->decodePolyIdTile(uint64(InRef)))) : nullptr;
		if (!Entry || !Inputs->Retain(Entry->Capture))
		{
			Inputs->bMissingInput = true;
			return false;
		}
		const auto* Original = static_cast<const FRecastQueryFilter*>(Source->GetImplementation());
		return Original->passFilter(InRef, InTile, InPoly) && Navigation.FindUsableNodeCapture(NavNodeRef(InRef));
	}

	virtual dtReal getVirtualCost(const dtReal* InStart, const dtReal* InEnd,
		const dtPolyRef InPrevious, const dtMeshTile* InPreviousTile, const dtPoly* InPreviousPoly,
		const dtPolyRef InCurrent, const dtMeshTile* InCurrentTile, const dtPoly* InCurrentPoly,
		const dtPolyRef InNext, const dtMeshTile* InNextTile, const dtPoly* InNextPoly) const override
	{
		const auto* Original = static_cast<const FRecastQueryFilter*>(Source->GetImplementation());
		return Original->getCost(InStart, InEnd, InPrevious, InPreviousTile, InPreviousPoly,
			InCurrent, InCurrentTile, InCurrentPoly, InNext, InNextTile, InNextPoly);
	}

private:
	const AVoxelNavigationData& Navigation;
	FSharedConstNavQueryFilter Source;
	TSharedRef<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Inputs;
};

struct FVoxelNavigationScopedFilter : FNavigationQueryFilter
{
	FVoxelNavigationScopedFilter(TSharedPtr<INavigationQueryFilterInterface, ESPMode::ThreadSafe> InImplementation,
		const uint32 InMaximumNodes)
	{
		QueryFilterImpl = MoveTemp(InImplementation);
		MaxSearchNodes = InMaximumNodes;
	}
};

/** 请求标记只持有冻结的调用者filter；真实输入约束仍由FindTerrainPath的guard施加。 */
class FVoxelNavigationAsyncFilter final : public FRecastQueryFilter
{
public:
	explicit FVoxelNavigationAsyncFilter(
		TSharedRef<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe> InTicket)
		: FRecastQueryFilter(*static_cast<const FRecastQueryFilter*>(InTicket->OriginalFilter->GetImplementation())),
		Ticket(MoveTemp(InTicket))
	{
		SetIsVirtual(true);
	}

	virtual FVector GetAdjustedEndLocation(const FVector& InEnd) const override
	{
		return Ticket->OriginalFilter->GetAdjustedEndLocation(InEnd);
	}

	virtual float GetHeuristicScale() const override
	{
		return Ticket->OriginalFilter->GetHeuristicScale();
	}

protected:
	virtual bool passVirtualFilter(const dtPolyRef InRef, const dtMeshTile* InTile, const dtPoly* InPoly) const override
	{
		const auto* Source = static_cast<const FRecastQueryFilter*>(Ticket->OriginalFilter->GetImplementation());
		return Source->passFilter(InRef, InTile, InPoly);
	}

	virtual dtReal getVirtualCost(const dtReal* InStart, const dtReal* InEnd,
		const dtPolyRef InPrevious, const dtMeshTile* InPreviousTile, const dtPoly* InPreviousPoly,
		const dtPolyRef InCurrent, const dtMeshTile* InCurrentTile, const dtPoly* InCurrentPoly,
		const dtPolyRef InNext, const dtMeshTile* InNextTile, const dtPoly* InNextPoly) const override
	{
		const auto* Source = static_cast<const FRecastQueryFilter*>(Ticket->OriginalFilter->GetImplementation());
		return Source->getCost(InStart, InEnd, InPrevious, InPreviousTile, InPreviousPoly,
			InCurrent, InCurrentTile, InCurrentPoly, InNext, InNextTile, InNextPoly);
	}

private:
	TSharedRef<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe> Ticket;
};

class FVoxelRecastTileGenerator final : public FRecastTileGenerator
{
public:
	FVoxelRecastTileGenerator(FRecastNavMeshGenerator& InParent, const FIntPoint& InCoordinate, const double InCreated)
		: FRecastTileGenerator(InParent, InCoordinate, InCreated),
		Navigation(const_cast<AVoxelNavigationData*>(CastChecked<AVoxelNavigationData>(InParent.GetOwner())))
	{
	}

	virtual ~FVoxelRecastTileGenerator() override
	{
		// AddGeneratedTileLayer 仅在 Detour 成功接管正文后调用 Release；失败或 Discard 保留正文。
		// 同一坐标在引擎 RunningDirtyTiles 内串行，析构发生在该次安装后、TilesUpdated 回调前。
		if (!IsInGameThread() || !Capture || !IsFullyRegenerated() || NavigationData.IsEmpty()) return;
		const auto* Native = Navigation->GetRecastMesh();
		if (!Native || Native->getMaxTiles() > Navigation->EvidenceCapacity) return;
		for (const FNavMeshTileData& Layer : NavigationData)
		{
			if (!Layer.NavData || Layer.NavData->GetRawNavData() || Layer.DataSize != 0) return;
		}
		Navigation->GetNavMeshTilesAt(GetTileX(), GetTileY(), Capture->NativeTiles);
		if (Capture->NativeTiles.Num() != NavigationData.Num()) return;
		Navigation->StageGeneratedTile(Capture.ToSharedRef());
	}

protected:
	virtual void GatherGeometry(const FRecastNavMeshGenerator& InParent, const bool bGeometryChanged) override
	{
		FRecastTileGenerator::GatherGeometry(InParent, bGeometryChanged);
		CaptureSources(InParent, bGeometryChanged);
	}

	virtual void PrepareGeometrySources(const FRecastNavMeshGenerator& InParent, const bool bGeometryChanged) override
	{
		FRecastTileGenerator::PrepareGeometrySources(InParent, bGeometryChanged);
		CaptureSources(InParent, bGeometryChanged);
	}

private:
	void CaptureSources(const FRecastNavMeshGenerator& InParent, const bool bGeometryChanged)
	{
		check(IsInGameThread());
		if (!bGeometryChanged || !Navigation->Resources || Navigation->EvidenceCapacity == 0) return;
		const auto* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(InParent.GetWorld());
		const auto* Octree = NavigationSystem ? NavigationSystem->GetNavOctree() : nullptr;
		if (!Octree) return;
		const auto Bounds = InParent.GrowBoundingBox(GetTileBB(), false);
		int32 Count = 0;
		bool bValid = true;
		const auto Visit = [&](const FNavigationOctreeElement& Element, const bool bStore)
		{
			const auto* Component = Cast<UVoxelNavigationSurfaceComponent>(Element.Data->SourceElement->GetWeakUObject().Get());
			const bool bShouldUse = InParent.GetOwner()->bUseVirtualGeometryFilteringAndDirtying ?
				InParent.ShouldGenerateGeometryForOctreeElement(Element, InParent.GetOwner()->GetConfig()) :
				Element.ShouldUseGeometry(InParent.GetOwner()->GetConfig());
			if (!Component || !bShouldUse ||
				(!Element.Data->HasGeometry() && !Element.Data->IsPendingLazyGeometryGathering())) return;
			const auto Surface = Component->CaptureExportedSurface(Element.Data->SourceElement.Get());
			if (!Surface || Element.Data->IsPendingLazyGeometryGathering())
			{
				bValid = false;
				return;
			}
			++Count;
			if (bStore) Capture->Surfaces.Add({Surface, Component, Component->GetComponentTransform()});
		};
		Octree->FindElementsWithBoundsTest(Bounds, [&](const FNavigationOctreeElement& Element) { Visit(Element, false); });
		if (!bValid || Count == 0) return;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationTileCapture>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelNavigationTileCapture::FSourceInput>(Count) +
			VoxelBoundedAllocation::ArrayBytes<FNavTileRef>(Navigation->EvidenceCapacity);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(Navigation->WorldEpoch, Navigation->EvidenceOwnerId, 1,
			FIntVector(GetTileX(), GetTileY(), 0), 1);
		if (!Navigation->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Bytes, Lease))
		{
			Navigation->HoldBlockedTile(FIntPoint(GetTileX(), GetTileY()), GetTileBB());
			return;
		}
		Capture = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationTileCapture>(Lease);
		Capture->MetadataLease = MoveTemp(Lease);
		Capture->EvidenceOwnerId = Navigation->EvidenceOwnerId;
		Capture->WorldEpoch = Navigation->WorldEpoch;
		Capture->Coordinate = FIntPoint(GetTileX(), GetTileY());
		Capture->Surfaces.Reserve(Count);
		Capture->NativeTiles.Reserve(Navigation->EvidenceCapacity);
		const int32 ExpectedCount = Count;
		Count = 0;
		Octree->FindElementsWithBoundsTest(Bounds, [&](const FNavigationOctreeElement& Element) { Visit(Element, true); });
		if (!bValid || Count != ExpectedCount)
		{
			Capture.Reset();
			return;
		}
		const uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationTileCapture>() +
			Capture->Surfaces.GetAllocatedSize() + Capture->NativeTiles.GetAllocatedSize();
		check(Actual <= Bytes);
		verify(Capture->MetadataLease->Resize(Actual));
	}

	AVoxelNavigationData* Navigation;
	TSharedPtr<FVoxelNavigationTileCapture, ESPMode::ThreadSafe> Capture;
};

class FVoxelRecastNavMeshGenerator final : public FRecastNavMeshGenerator
{
public:
	explicit FVoxelRecastNavMeshGenerator(ARecastNavMesh& InNavigation)
		: FRecastNavMeshGenerator(InNavigation)
	{
	}

	bool CanCancelWithoutWaiting() const
	{
		for (const auto& Element : RunningDirtyTiles)
		{
			if (Element.AsyncTask && !Element.AsyncTask->IsDone()) return false;
		}
		return true;
	}

	virtual bool RebuildAll() override
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		Navigation->bNativeRebuildPending = true;
		Navigation->bNativeCancelPending = false;
		if (!Navigation->TryBeginNativeWrite()) return false;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		if (!CanCancelWithoutWaiting()) return false;
		Navigation->bNativeRebuildPending = false;
		// Detour 重建可复用地址、slot 和初始 salt；同一世界也必须更换实际 Native 证据代。
		Navigation->InvalidateNativeEvidence();
		return FRecastNavMeshGenerator::RebuildAll();
	}

	virtual void TickAsyncBuild(float InDeltaSeconds) override
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		if (!Navigation->TryBeginNativeWrite()) return;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		if (!ApplyDeferredNativeChanges()) return;
		FRecastNavMeshGenerator::TickAsyncBuild(InDeltaSeconds);
	}

	virtual void EnsureBuildCompletion() override
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		if (!Navigation->TryBeginNativeWrite()) return;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		if (!ApplyDeferredNativeChanges()) return;
		FRecastNavMeshGenerator::EnsureBuildCompletion();
	}

	virtual void CancelBuild() override
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		Navigation->bNativeCancelPending = true;
		Navigation->bNativeRebuildPending = false;
		if (!Navigation->TryBeginNativeWrite()) return;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		if (!CanCancelWithoutWaiting()) return;
		Navigation->bNativeCancelPending = false;
		FRecastNavMeshGenerator::CancelBuild();
	}

	virtual void OnNavigationBoundsChanged() override
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		Navigation->bNativeBoundsPending = true;
		if (!Navigation->TryBeginNativeWrite()) return;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		Navigation->bNativeBoundsPending = false;
		FRecastNavMeshGenerator::OnNavigationBoundsChanged();
	}

	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& InAreas) override
	{
		// 已有Detour时此入口只修改GT generator dirty表，不安装或删除原生tile。
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		if (Navigation->GetRecastMesh())
		{
			FRecastNavMeshGenerator::RebuildDirtyAreas(InAreas);
			return;
		}
		if (!Navigation->TryBeginNativeWrite()) return;
		ON_SCOPE_EXIT { Navigation->FinishNativeWrite(); };
		FRecastNavMeshGenerator::RebuildDirtyAreas(InAreas);
	}

	virtual int32 GetNumRemaningBuildTasks() const override
	{
		const auto* Navigation = CastChecked<AVoxelNavigationData>(GetOwner());
		return FRecastNavMeshGenerator::GetNumRemaningBuildTasks() + (Navigation->HasDeferredNativeWrite() ? 1 : 0);
	}

protected:
	virtual TSharedRef<FRecastTileGenerator> CreateTileGenerator(const FIntPoint& InCoordinate,
		const TArray<FBox>& InDirtyAreas, const double InCreated) override
	{
		// 体素提交证明覆盖该 tile 的完整几何输入，不复用无输入版本的压缩层。
		const TArray<FBox> FullGeometry;
		return ConstructTileGeneratorImpl<FVoxelRecastTileGenerator>(InCoordinate, FullGeometry, InCreated);
	}

private:
	bool ApplyDeferredNativeChanges()
	{
		auto* Navigation = CastChecked<AVoxelNavigationData>(DestNavMesh);
		if (Navigation->bNativeCleanupPending) return false;
		if (Navigation->bTerrainClearPending) Navigation->ClearTerrainEvidence();
		if (Navigation->bNativeCancelPending)
		{
			if (!CanCancelWithoutWaiting()) return false;
			Navigation->bNativeCancelPending = false;
			FRecastNavMeshGenerator::CancelBuild();
		}
		if (Navigation->bNativeBoundsPending)
		{
			Navigation->bNativeBoundsPending = false;
			FRecastNavMeshGenerator::OnNavigationBoundsChanged();
		}
		if (Navigation->bNativeInvokersPending)
		{
			const auto* System = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Navigation->GetWorld());
			if (!System) return false;
			Navigation->bNativeInvokersPending = false;
			Navigation->UpdateActiveTiles(System->GetInvokerLocations());
		}
		if (Navigation->bNativeRebuildPending) RebuildAll();
		if (!Navigation->DeferredRemovals.IsEmpty())
		{
			Navigation->RemovalScratch.Reset();
			for (const FIntPoint& Tile : Navigation->DeferredRemovals) Navigation->RemovalScratch.Add(Tile);
			Navigation->DeferredRemovals.Reset();
			FRecastNavMeshGenerator::RemoveTiles(Navigation->RemovalScratch);
			Navigation->RemovalScratch.Reset();
		}
		return !Navigation->HasDeferredNativeWrite();
	}
};
#endif

AVoxelNavigationData::AVoxelNavigationData(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	RuntimeGeneration = ERuntimeGenerationType::Dynamic;
#if WITH_RECAST
	bDoFullyAsyncNavDataGathering = false;
#endif
	FindPathImplementation = FindTerrainPath;
	FindHierarchicalPathImplementation = FindTerrainPath;
	TestPathImplementation = TestTerrainPath;
	TestHierarchicalPathImplementation = TestTerrainHierarchicalPath;
}

bool AVoxelNavigationData::BeginTerrainQuery() const
{
	if (!bQueryAdmissionOpen.Load() || !NativeAccess.TryReadLock()) return false;
	if (!bQueryAdmissionOpen.Load())
	{
		NativeAccess.ReadUnlock();
		return false;
	}
	NativeQueryReaders++;
	return true;
}

void AVoxelNavigationData::FinishTerrainQuery() const
{
	const int32 Previous = NativeQueryReaders--;
	check(Previous > 0);
	NativeAccess.ReadUnlock();
}

bool AVoxelNavigationData::TryBeginNativeWrite()
{
	check(IsInGameThread());
	if (NativeWriteDepth > 0)
	{
		++NativeWriteDepth;
		return true;
	}
	if (!NativeAccess.TryWriteLock())
	{
		bQueryAdmissionOpen.Store(false);
		return false;
	}
	bQueryAdmissionOpen.Store(false);
	NativeWriteDepth = 1;
	return true;
}

void AVoxelNavigationData::FinishNativeWrite()
{
	check(IsInGameThread() && NativeWriteDepth > 0);
	if (--NativeWriteDepth != 0) return;
#if WITH_RECAST
	bQueryAdmissionOpen.Store(StorageLease && GetRecastMesh() && !bWorldCloseRequested && !HasDeferredNativeWrite());
#else
	bQueryAdmissionOpen.Store(false);
#endif
	NativeAccess.WriteUnlock();
}

bool AVoxelNavigationData::HasDeferredNativeWrite() const
{
	return bNativeRebuildPending || bNativeBoundsPending || bNativeCancelPending || bNativeCleanupPending ||
		bNativeInvokersPending || bTerrainClearPending || !DeferredRemovals.IsEmpty();
}

bool AVoxelNavigationData::TryCloseQueriesForWorldStop()
{
	check(IsInGameThread());
	if (auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
		System && !System->TryCloseTerrainQueries(*this))
	{
		return false;
	}
	// 引擎写入回调也不能重入销毁仍在Tick内的generator/所有者。
	if (NativeWriteDepth != 0 || !TryBeginNativeWrite()) return false;
	bWorldCloseRequested = true;
	RevokeAllPathInputs(false);
	PendingRepaths.Reset();
	PendingRepathPaths.Reset();
	RepathRequests.Reset();
	FinishNativeWrite();
	return true;
}

void AVoxelNavigationData::ResumeQueriesAfterWorldStopWait()
{
	check(IsInGameThread());
	bWorldCloseRequested = false;
	if (!TryBeginNativeWrite()) return;
	FinishNativeWrite();
}

FPathFindingResult AVoxelNavigationData::FindTerrainPath(const FNavAgentProperties& InAgent, const FPathFindingQuery& InQuery)
{
	return FindTerrainPathWithWait(InAgent, InQuery, nullptr);
}

FPathFindingResult AVoxelNavigationData::FindTerrainPathWithWait(const FNavAgentProperties& InAgent,
	const FPathFindingQuery& InQuery, bool* OutWaiting)
{
	if (OutWaiting) *OutWaiting = true;
	const auto* Data = Cast<AVoxelNavigationData>(InQuery.NavData.Get());
	const FVoxelNavigationNativeQueryScope Access(Data);
	if (!Access.IsEntered()) return ENavigationQueryResult::Error;
#if WITH_RECAST
	const auto AsyncTicket = Data->FindAsyncTicket(InQuery.QueryFilter);
	if (!OutWaiting && AsyncTicket && AsyncTicket->Repath) OutWaiting = &AsyncTicket->Repath->bWaiting;
	if (!InQuery.QueryFilter || !InQuery.QueryFilter->GetImplementation())
	{
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Error;
	}
	if (AsyncTicket && (AsyncTicket->WorldEpoch != Data->WorldEpoch || AsyncTicket->BindingId != Data->TerrainBindingId))
	{
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Fail;
	}
	const FSharedConstNavQueryFilter SourceFilter = AsyncTicket ? AsyncTicket->OriginalFilter : InQuery.QueryFilter;
	if (!SourceFilter || !SourceFilter->GetImplementation())
	{
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Error;
	}
	if (InQuery.PathInstanceToFill && InQuery.PathInstanceToFill->CastPath<FNavMeshPath>() &&
		InQuery.PathInstanceToFill->GetNavigationDataUsed() && InQuery.PathInstanceToFill->GetNavigationDataUsed() != Data)
	{
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Error;
	}
	const auto PreviousAttempt = Data->FindPathInputs(InQuery.PathInstanceToFill);
	if (AsyncTicket && AsyncTicket->Repath &&
		(AsyncTicket->Repath->QueryId != AsyncTicket->QueryId || AsyncTicket->Repath->Path.Pin() != InQuery.PathInstanceToFill ||
			AsyncTicket->Repath->ExpectedSemanticInputs != PreviousAttempt))
	{
		// 排队之后已有另一真实尝试接管；旧任务不能先Reset新路径再在回调中拒绝。
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Fail;
	}
	const auto Inputs = Data->CaptureQueryInputs();
	if (!Inputs) return ENavigationQueryResult::Error;
	Inputs->PreviousAttempt = PreviousAttempt;
	if (InQuery.PathInstanceToFill && InQuery.PathInstanceToFill->IsReady() && !Inputs->PreviousAttempt)
	{
		if (OutWaiting) *OutWaiting = false;
		return ENavigationQueryResult::Error;
	}
	const auto Implementation = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationGuardFilter>(Inputs->Lease,
		*Data, SourceFilter, Inputs.ToSharedRef());
	const auto Filter = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationScopedFilter>(Inputs->Lease,
		Implementation, SourceFilter->GetMaxSearchNodes());
	FPathFindingQuery Guarded(InQuery);
	Guarded.QueryFilter = Filter;
	if ((InQuery.StartLocation - Filter->GetAdjustedEndLocation(InQuery.EndLocation)).IsNearlyZero())
	{
		FNavLocation Projected;
		if (!Data->ProjectPoint(InQuery.StartLocation, Projected, Data->GetDefaultQueryExtent(),
			Filter, InQuery.Owner.Get())) return ENavigationQueryResult::Fail;
	}
	// 容量和前置投影失败保留原路径；实际填充开始时才结束旧尝试的写入权限。
	if (!Data->ReservePathInputSlot(InQuery.PathInstanceToFill, Inputs->PreviousAttempt)) return ENavigationQueryResult::Error;
	bool bPathInputsRegistered = false;
	ON_SCOPE_EXIT { Data->ReleasePathInputSlot(InQuery.PathInstanceToFill, bPathInputsRegistered); };
	FPathFindingResult Result = ARecastNavMesh::FindPath(InAgent, Guarded);
	if (Result.Path)
	{
		if (!Result.Path->GetNavigationDataUsed())
		{
			Result.Path->SetNavigationDataUsed(Data);
			const_cast<AVoxelNavigationData*>(Data)->RegisterActivePath(Result.Path);
		}
		// 范围过滤器包含本次原生读锁下的所有者引用，不允许逸出到下一次repath或调用者。
		FPathFindingQuery OriginalQuery(InQuery);
		OriginalQuery.QueryFilter = SourceFilter;
		Result.Path->SetQueryData(OriginalQuery);
		Result.Path->SetFilter(SourceFilter);
	}
	if (Result.Path && (Result.Path->IsReady() || (Result.IsSuccessful() && Result.Path->GetPathPoints().Num() == 1)))
	{
		bPathInputsRegistered = Data->RegisterPathInputs(Result.Path, Inputs.ToSharedRef());
		if (!bPathInputsRegistered || !Inputs->PublishPath(*Data, Result.Path))
		{
			Inputs->SuspendForRepath();
			Result.Path->ResetForRepath();
			Result.Result = ENavigationQueryResult::Fail;
			if (bPathInputsRegistered && Inputs->PreviousAttempt && Result.Path == InQuery.PathInstanceToFill)
			{
				Inputs->PreviousAttempt->RestoreWaitingPath(Result.Path);
				FScopeLock Lock(&Data->PathInputsLock);
				if (auto* Current = Data->PathInputs.Find(Result.Path.Get()); Current && *Current == Inputs)
				{
					*Current = Inputs->PreviousAttempt;
				}
			}
		}
	}
	else if (Result.IsSuccessful() && (!Result.Path || !Inputs->IsCurrent()))
	{
		Result.Result = ENavigationQueryResult::Fail;
	}
	if (!bPathInputsRegistered && Inputs->PreviousAttempt && InQuery.PathInstanceToFill)
	{
		Inputs->PreviousAttempt->RestoreWaitingPath(InQuery.PathInstanceToFill);
		bPathInputsRegistered = Data->RegisterPathInputs(InQuery.PathInstanceToFill, Inputs->PreviousAttempt.ToSharedRef());
	}
	Inputs->PreviousAttempt.Reset();
	if (AsyncTicket)
	{
		AsyncTicket->CompletionInputs = Inputs;
		AsyncTicket->CompletionPath = Result.Path;
		AsyncTicket->bReadyAtCompletion = Result.Path && Result.Path->IsReady();
		AsyncTicket->bCompleted = true;
		if (AsyncTicket->Repath)
		{
			AsyncTicket->Repath->ExpectedSemanticInputs = Data->FindPathInputs(
				Result.Path ? Result.Path : InQuery.PathInstanceToFill);
		}
	}
	if (OutWaiting)
	{
		*OutWaiting = !Result.IsSuccessful() && !Result.IsPartial() &&
			(Result.Result == ENavigationQueryResult::Error || Inputs->bMissingInput || !Inputs->IsCurrent());
	}
	return Result;
#else
	return ENavigationQueryResult::Error;
#endif
}

bool AVoxelNavigationData::IsPathInputAttemptCurrent(const FNavPathSharedPtr& InPath,
	const TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>& InInputs) const
{
#if WITH_RECAST
	return InInputs && FindPathInputs(InPath) == InInputs && InInputs->IsCurrent();
#else
	return false;
#endif
}

bool AVoxelNavigationData::TestTerrainPath(const FNavAgentProperties& InAgent, const FPathFindingQuery& InQuery, int32* OutVisited)
{
	if (OutVisited) *OutVisited = 0;
	const auto* Data = Cast<AVoxelNavigationData>(InQuery.NavData.Get());
	const FVoxelNavigationNativeQueryScope Access(Data);
	if (!Access.IsEntered()) return false;
#if WITH_RECAST
	if (!InQuery.QueryFilter || !InQuery.QueryFilter->GetImplementation()) return false;
	const auto Inputs = Data->CaptureQueryInputs();
	if (!Inputs) return false;
	const auto Implementation = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationGuardFilter>(Inputs->Lease,
		*Data, InQuery.QueryFilter, Inputs.ToSharedRef());
	const auto Filter = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationScopedFilter>(Inputs->Lease,
		Implementation, InQuery.QueryFilter->GetMaxSearchNodes());
	FPathFindingQuery Guarded(InQuery);
	Guarded.QueryFilter = Filter;
	if ((InQuery.StartLocation - Filter->GetAdjustedEndLocation(InQuery.EndLocation)).IsNearlyZero())
	{
		FNavLocation Projected;
		if (!Data->ProjectPoint(InQuery.StartLocation, Projected, Data->GetDefaultQueryExtent(),
			Filter, InQuery.Owner.Get())) return false;
	}
	return ARecastNavMesh::TestPath(InAgent, Guarded, OutVisited) && Inputs->IsCurrent();
#else
	return false;
#endif
}

bool AVoxelNavigationData::TestTerrainHierarchicalPath(const FNavAgentProperties& InAgent,
	const FPathFindingQuery& InQuery, int32* OutVisited)
{
	// 原生cluster快速检查不应用本次节点过滤器；使用引擎支持的非默认filter回落路径。
	return TestTerrainPath(InAgent, InQuery, OutVisited);
}

const FVoxelNavigationTileCapture* AVoxelNavigationData::FindUsableNodeCapture(const NavNodeRef InNode) const
{
#if WITH_RECAST
	const auto* Native = GetRecastMesh();
	const dtMeshTile* Tile = nullptr;
	const dtPoly* Polygon = nullptr;
	if (!Native || dtStatusFailed(Native->getTileAndPolyByRef(InNode, &Tile, &Polygon)) || !Tile || !Polygon) return nullptr;
	const auto Reference = FNavTileRef(Native->getTileRef(Tile));
	const auto* Entry = CommittedTiles.Find(int32(Native->decodePolyIdTile(uint64(Reference))));
	if (!Entry || Entry->Reference != Reference || !Entry->Capture || Entry->Capture->WorldEpoch != WorldEpoch ||
		Entry->Capture->EvidenceOwnerId != EvidenceOwnerId || !Entry->Capture->Access.IsCurrent() ||
		Entry->Capture->Surfaces.IsEmpty()) return nullptr;
	for (const auto& Input : Entry->Capture->Surfaces)
	{
		if (!Input.Surface || !Input.Surface->Access || !Input.Surface->Access->IsCurrent()) return nullptr;
	}
	return Entry->Capture.Get();
#else
	return nullptr;
#endif
}

TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> AVoxelNavigationData::CaptureQueryInputs() const
{
#if WITH_RECAST
	// 当前原生读锁保留目录。按其真实输入个数先授予，不在passFilter期间增长backing。
	int64 Count = 0;
	for (const auto& Entry : CommittedTiles)
	{
		if (Entry.Value.Capture) Count += Entry.Value.Capture->Surfaces.Num();
	}
	if (Count > MAX_int32 || !Resources) return nullptr;
	const int32 Capacity = int32(Count);
	const int32 TileCapacity = CommittedTiles.Num();
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationQueryInputs>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationGuardFilter>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationScopedFilter>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelNavigationQueryInputs::FSurfaceReader>(Capacity) +
		VoxelBoundedAllocation::MapBytes<const FVoxelNavigationSurface*, int32>(Capacity) +
		VoxelBoundedAllocation::MapBytes<FIntVector, int32>(Capacity) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelNavigationQueryInputs::FTileReader>(TileCapacity) +
		VoxelBoundedAllocation::MapBytes<const FVoxelNavigationTileCapture*, int32>(TileCapacity);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const FGuid Attempt = FGuid::NewGuid();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, Attempt, 1, {}, 1);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return nullptr;
	const auto Inputs = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationQueryInputs>(Lease);
	Inputs->Lease = MoveTemp(Lease);
	Inputs->AttemptId = Attempt;
	Inputs->Self = Inputs;
	Inputs->Readers.Reserve(Capacity);
	Inputs->ReaderIndices.Reserve(Capacity);
	Inputs->ReaderSections.Reserve(Capacity);
	Inputs->Tiles.Reserve(TileCapacity);
	Inputs->TileIndices.Reserve(TileCapacity);
	check((Inputs->Readers.GetAllocatedSize() + Inputs->ReaderIndices.GetAllocatedSize() + Inputs->ReaderSections.GetAllocatedSize() +
		Inputs->Tiles.GetAllocatedSize() + Inputs->TileIndices.GetAllocatedSize() <=
		VoxelBoundedAllocation::ArrayBytes<FVoxelNavigationQueryInputs::FSurfaceReader>(Capacity) +
		VoxelBoundedAllocation::MapBytes<const FVoxelNavigationSurface*, int32>(Capacity) +
		VoxelBoundedAllocation::MapBytes<FIntVector, int32>(Capacity) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelNavigationQueryInputs::FTileReader>(TileCapacity) +
		VoxelBoundedAllocation::MapBytes<const FVoxelNavigationTileCapture*, int32>(TileCapacity)));
	return Inputs;
#else
	return nullptr;
#endif
}

TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> AVoxelNavigationData::FindPathInputs(const FNavPathSharedPtr& InPath) const
{
#if WITH_RECAST
	if (!InPath) return nullptr;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Inputs;
	{
		FScopeLock Lock(&PathInputsLock);
		if (const auto* Entry = PathInputs.Find(InPath.Get())) Inputs = *Entry;
	}
	return Inputs && Inputs->CanContinue(InPath) ? Inputs : nullptr;
#else
	return nullptr;
#endif
}

TSharedPtr<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe> AVoxelNavigationData::FindAsyncTicket(
	const FSharedConstNavQueryFilter& InFilter) const
{
	FScopeLock Lock(&AsyncTicketsLock);
	const auto* Found = InFilter ? AsyncTickets.Find(InFilter.Get()) : nullptr;
	return Found ? *Found : nullptr;
}

bool AVoxelNavigationData::PrepareAsyncQuery(FAsyncPathFindingQuery& InQuery)
{
	check(IsInGameThread());
#if WITH_RECAST
	if (const auto Existing = FindAsyncTicket(InQuery.QueryFilter))
	{
		return Existing->QueryId == InQuery.QueryID && Existing->WorldEpoch == WorldEpoch &&
			Existing->BindingId == TerrainBindingId;
	}
	// 无效filter由实际worker报错，派发时不能通过Success核对；不能让一个坏请求卡住整个队列。
	if (!InQuery.QueryFilter || !InQuery.QueryFilter->GetImplementation()) return true;
	if (!StorageLease || !Resources || bWorldCloseRequested || InQuery.QueryID == INVALID_NAVQUERYID) return false;
	{
		FScopeLock Lock(&AsyncTicketsLock);
		if (AsyncTickets.Num() >= EvidenceCapacity) return false;
	}
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationAsyncQueryTicket>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationAsyncFilter>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationScopedFilter>();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const FGuid Producer = FGuid::NewGuid();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, Producer, 1, {}, 1);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return false;
	const auto Ticket = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationAsyncQueryTicket>(Lease);
	Ticket->Lease = Lease;
	Ticket->OriginalFilter = InQuery.QueryFilter;
	Ticket->ProducerId = Producer;
	Ticket->BindingId = TerrainBindingId;
	Ticket->WorldEpoch = WorldEpoch;
	Ticket->QueryId = InQuery.QueryID;
	if (const auto* ProducerId = PendingRepathPaths.Find(InQuery.PathInstanceToFill.Get()))
	{
		const auto* Repath = PendingRepaths.Find(*ProducerId);
		if (Repath && (*Repath)->QueryId == InQuery.QueryID &&
			InQuery.OnDoneDelegate.GetObjectForTimerManager() == Repath->Get()) Ticket->Repath = *Repath;
	}
	const auto Implementation = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationAsyncFilter>(Lease, Ticket.ToSharedRef());
	const FSharedConstNavQueryFilter Marked = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationScopedFilter>(Lease,
		Implementation, InQuery.QueryFilter->GetMaxSearchNodes());
	Ticket->MarkedFilter = Marked;
	{
		FScopeLock Lock(&AsyncTicketsLock);
		check(AsyncTickets.Num() < EvidenceCapacity && !AsyncTickets.Contains(Marked.Get()));
		AsyncTickets.Add(Marked.Get(), Ticket);
	}
	InQuery.QueryFilter = Marked;
	return true;
#else
	return false;
#endif
}

ENavigationQueryResult::Type AVoxelNavigationData::ValidateAsyncResult(const FAsyncPathFindingQuery& InQuery) const
{
	check(IsInGameThread());
	const auto Ticket = FindAsyncTicket(InQuery.QueryFilter);
	ON_SCOPE_EXIT
	{
#if WITH_RECAST
		// 成功搜索之后的输入撤销或尝试替换也必须让自动repath继续等待。
		if (Ticket && Ticket->Repath && (InQuery.Result.IsSuccessful() || InQuery.Result.IsPartial()))
		{
			Ticket->Repath->bWaiting = !Ticket->CompletionInputs || !Ticket->CompletionInputs->IsCurrent() ||
				FindPathInputs(InQuery.Result.Path) != Ticket->CompletionInputs;
		}
#endif
	};
	if (!Ticket || !Ticket->ProducerId.IsValid() || Ticket->QueryId != InQuery.QueryID ||
		Ticket->WorldEpoch != WorldEpoch || Ticket->BindingId != TerrainBindingId || bWorldCloseRequested)
	{
		return ENavigationQueryResult::Fail;
	}
	if (!InQuery.Result.IsSuccessful() && !InQuery.Result.IsPartial()) return InQuery.Result.Result;
#if WITH_RECAST
	const auto Inputs = Ticket->CompletionInputs;
	if (!Ticket->bCompleted || !Inputs || !Inputs->IsCurrent() ||
		Ticket->CompletionPath.Pin() != InQuery.Result.Path || !InQuery.Result.Path) return ENavigationQueryResult::Fail;
	if (FindPathInputs(InQuery.Result.Path) != Inputs) return ENavigationQueryResult::Fail;
	if (Ticket->bReadyAtCompletion)
	{
		// 比较完成时实际控制的身份，不能把自动repath后的同地址路径当作旧请求的结果。
		if (!InQuery.Result.Path->IsValid())
		{
			return ENavigationQueryResult::Fail;
		}
	}
	else if (InQuery.Result.Path->GetPathPoints().Num() != 1 || !InQuery.Result.Path->IsUpToDate())
	{
		return ENavigationQueryResult::Fail;
	}
	return InQuery.Result.Result;
#else
	return ENavigationQueryResult::Fail;
#endif
}

void AVoxelNavigationData::ReleaseAsyncQuery(const FAsyncPathFindingQuery& InQuery)
{
	check(IsInGameThread());
	FScopeLock Lock(&AsyncTicketsLock);
	const auto* Entry = InQuery.QueryFilter ? AsyncTickets.Find(InQuery.QueryFilter.Get()) : nullptr;
	if (Entry && (*Entry)->QueryId == InQuery.QueryID) AsyncTickets.Remove(InQuery.QueryFilter.Get());
}

void AVoxelNavigationData::RetireAsyncTickets(FVoxelTerrainFrameContext& InFrame)
{
	const double Deadline = InFrame.Deadline(0.1);
	for (int32 Visited = 0; Visited < 32; ++Visited)
	{
		const double Started = FPlatformTime::Seconds();
		if (Started >= Deadline || (InFrame.IsActive() &&
			!InFrame.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission, Started, AsyncTicketRetirementCostMilliseconds))) break;
		TSharedPtr<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe> Retired;
		{
			FScopeLock Lock(&AsyncTicketsLock);
			if (AsyncTickets.IsEmpty()) break;
			AsyncTicketCursor %= AsyncTickets.GetMaxIndex();
			const auto Id = FSetElementId::FromInteger(AsyncTicketCursor++);
			if (AsyncTickets.IsValidId(Id) && !AsyncTickets.Get(Id).Value->MarkedFilter.IsValid())
			{
				Retired = AsyncTickets.Get(Id).Value;
				AsyncTickets.Remove(AsyncTickets.Get(Id).Key);
			}
		}
		Retired.Reset();
		const double Finished = FPlatformTime::Seconds();
		AsyncTicketRetirementCostMilliseconds = FMath::Lerp(AsyncTicketRetirementCostMilliseconds,
			(Finished - Started) * 1000.0, 0.2);
		if (InFrame.IsActive()) InFrame.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
}

bool AVoxelNavigationData::ReservePathInputSlot(const FNavPathSharedPtr& InExisting,
	const TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>& InExpectedPrevious) const
{
#if WITH_RECAST
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Previous;
	{
		FScopeLock Lock(&PathInputsLock);
		if (InExisting && InFlightPaths.Contains(InExisting.Get())) return false;
		if (InExisting)
		{
			const auto* Current = PathInputs.Find(InExisting.Get());
			if ((Current && *Current != InExpectedPrevious) || (!Current && InExpectedPrevious)) return false;
		}
		const bool bReplacing = InExisting && PathInputs.Contains(InExisting.Get());
		if (PathInputs.Num() + ReservedPathInputSlots - int32(bReplacing) >= EvidenceCapacity) return false;
		if (bReplacing) PathInputs.RemoveAndCopyValue(InExisting.Get(), Previous);
		if (InExisting) InFlightPaths.Add(InExisting.Get());
		++ReservedPathInputSlots;
	}
	// 先结束旧尝试的撤销权限，再允许引擎在同一原始路径对象上 Reset/填充。
	if (Previous) Previous->SuspendForRepath();
	return true;
#else
	return false;
#endif
}

void AVoxelNavigationData::ReleasePathInputSlot(const FNavPathSharedPtr& InExisting, const bool bInRegistered) const
{
	FScopeLock Lock(&PathInputsLock);
	if (InExisting) InFlightPaths.Remove(InExisting.Get());
	if (!bInRegistered)
	{
		check(ReservedPathInputSlots > 0);
		--ReservedPathInputSlots;
	}
}

bool AVoxelNavigationData::RegisterPathInputs(const FNavPathSharedPtr& InPath,
	TSharedRef<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> InInputs) const
{
	FScopeLock Lock(&PathInputsLock);
	if (!InPath || PathInputs.Contains(InPath.Get())) return false;
	check(ReservedPathInputSlots > 0 && PathInputs.Num() < EvidenceCapacity);
	--ReservedPathInputSlots;
	PathInputs.Add(InPath.Get(), MoveTemp(InInputs));
	return true;
}

void AVoxelNavigationData::RevokeAllPathInputs(const bool bInAllowRepath, const bool bInRetainPathDemand)
{
	check(IsInGameThread());
#if WITH_RECAST
	if (bInRetainPathDemand)
	{
		int32 Index = 0;
		for (;;)
		{
			TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Inputs;
			{
				FScopeLock Lock(&PathInputsLock);
				if (Index >= PathInputs.GetMaxIndex()) break;
				const auto Id = FSetElementId::FromInteger(Index++);
				if (PathInputs.IsValidId(Id)) Inputs = PathInputs.Get(Id).Value;
			}
			if (Inputs) Inputs->RevokePath(bInAllowRepath);
		}
		return;
	}
	for (;;)
	{
		TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Inputs;
		{
			FScopeLock Lock(&PathInputsLock);
			check(ReservedPathInputSlots == 0 && InFlightPaths.IsEmpty());
			if (PathInputs.IsEmpty()) break;
			auto Entry = PathInputs.CreateIterator();
			Inputs = Entry.Value();
			Entry.RemoveCurrent();
		}
		Inputs->RevokePath(bInAllowRepath);
	}
#endif
}

void AVoxelNavigationData::RetirePathInputs(FVoxelTerrainFrameContext& InFrame)
{
	check(IsInGameThread());
	RetireAsyncTickets(InFrame);
#if WITH_RECAST
	const double Deadline = InFrame.Deadline(0.1);
	for (int32 Step = 0; Step < 32 && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		{
			FScopeLock Lock(&PathInputsLock);
			if (PathInputs.IsEmpty()) break;
		}
		if (InFrame.IsActive() && !InFrame.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission,
			Started, PathRetirementCostMilliseconds)) break;
		TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> Inputs;
		const FNavigationPath* Key = nullptr;
		{
			FScopeLock Lock(&PathInputsLock);
			if (!PathInputs.IsEmpty())
			{
				PathInputCursor %= PathInputs.GetMaxIndex();
				const auto Id = FSetElementId::FromInteger(PathInputCursor++);
				if (PathInputs.IsValidId(Id))
				{
					Key = PathInputs.Get(Id).Key;
					Inputs = PathInputs.Get(Id).Value;
				}
			}
		}
		if (Inputs && Inputs->CanRetire())
		{
			FScopeLock Lock(&PathInputsLock);
			if (const auto* Current = PathInputs.Find(Key); Current && *Current == Inputs) PathInputs.Remove(Key);
		}
		const double Finished = FPlatformTime::Seconds();
		PathRetirementCostMilliseconds = FMath::Lerp(PathRetirementCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrame.IsActive()) InFrame.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
#endif
}

bool AVoxelNavigationData::BindTerrain(UVoxelModule& InModule,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
{
	check(IsInGameThread());
	if (WorldEpoch != InModule.GetWorldEpoch() || TerrainModule.Get() != &InModule)
	{
		if (auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
			System && !System->TryCloseTerrainQueries(*this)) return false;
	}
	if (!TryBeginNativeWrite()) return false;
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	if (WorldEpoch != InModule.GetWorldEpoch() || TerrainModule.Get() != &InModule)
	{
		ClearTerrainEvidence();
		TerrainModule = &InModule;
		WorldEpoch = InModule.GetWorldEpoch();
		Resources = InResources;
		EvidenceOwnerId = FGuid::NewGuid();
		TerrainBindingId = FGuid::NewGuid();
		bWorldCloseRequested = false;
	}
	if (StorageLease) return Resources == InResources;
#if WITH_RECAST
	const auto* Native = GetRecastMesh();
	if (!bFixedTilePoolSize) return false;
	const int32 Count = Native ? Native->getMaxTiles() : TilePoolSize;
	if (Count <= 0 || Count > MAX_int32 / 2) return false;
	if (ActivePaths.Num() > Count || ObservedPaths.Num() > Count || RepathRequests.Num() > Count) return false;
	const uint64 OldPathArrays = ActivePaths.GetAllocatedSize() + ObservedPaths.GetAllocatedSize() + RepathRequests.GetAllocatedSize();
	const uint64 Bytes = VoxelBoundedAllocation::MapBytes<int32, FCommittedTile>(Count) +
		VoxelBoundedAllocation::MapBytes<FNavTileRef, TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe>>(Count * 2) +
		VoxelBoundedAllocation::SetBytes<FCaptureWait>(Count * 2) +
		VoxelBoundedAllocation::MapBytes<const FNavigationPath*, TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe>>(Count) +
		VoxelBoundedAllocation::SetBytes<const FNavigationPath*>(Count) +
		VoxelBoundedAllocation::MapBytes<const FNavigationQueryFilter*, TSharedPtr<FVoxelNavigationAsyncQueryTicket, ESPMode::ThreadSafe>>(Count) +
		VoxelBoundedAllocation::MapBytes<FGuid, TSharedPtr<FVoxelNavigationRepathControl, ESPMode::ThreadSafe>>(Count) +
		VoxelBoundedAllocation::MapBytes<const FNavigationPath*, FGuid>(Count) +
		OldPathArrays + VoxelBoundedAllocation::ArrayBytes<FNavPathWeakPtr>(Count) * 2 +
		VoxelBoundedAllocation::ArrayBytes<FNavPathRecalculationRequest>(Count) +
		VoxelBoundedAllocation::SetBytes<FIntPoint>(Count) +
		VoxelBoundedAllocation::ArrayBytes<FIntPoint>(Count) + VoxelBoundedAllocation::ArrayBytes<FNavTileRef>(Count);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, EvidenceOwnerId, 1, {}, 1);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Bytes, StorageLease)) return false;
	CommittedTiles.Reserve(Count);
	PendingTiles.Reserve(Count * 2);
	BlockedTiles.Reserve(Count * 2);
	PathInputs.Reserve(Count);
	InFlightPaths.Reserve(Count);
	AsyncTickets.Reserve(Count);
	PendingRepaths.Reserve(Count);
	PendingRepathPaths.Reserve(Count);
	ActivePaths.Reserve(Count);
	ObservedPaths.Reserve(Count);
	RepathRequests.Reserve(Count);
	DeferredRemovals.Reserve(Count);
	RemovalScratch.Reserve(Count);
	RemovalReferenceScratch.Reserve(Count);
	EvidenceCapacity = Count;
	const uint64 Actual = CommittedTiles.GetAllocatedSize() + PendingTiles.GetAllocatedSize() + BlockedTiles.GetAllocatedSize() +
		PathInputs.GetAllocatedSize() + InFlightPaths.GetAllocatedSize() + AsyncTickets.GetAllocatedSize() +
		PendingRepaths.GetAllocatedSize() + PendingRepathPaths.GetAllocatedSize() +
		ActivePaths.GetAllocatedSize() + ObservedPaths.GetAllocatedSize() + RepathRequests.GetAllocatedSize() +
		DeferredRemovals.GetAllocatedSize() + RemovalScratch.GetAllocatedSize() + RemovalReferenceScratch.GetAllocatedSize();
	check(Actual <= Bytes);
	verify(StorageLease->Resize(Actual));
	return true;
#else
	return false;
#endif
}

bool AVoxelNavigationData::IsCaptureCurrent(const FVoxelNavigationTileCapture& InCapture) const
{
	const auto* Module = TerrainModule.Get();
	if (!Module || WorldEpoch != Module->GetWorldEpoch() || InCapture.WorldEpoch != WorldEpoch ||
		InCapture.EvidenceOwnerId != EvidenceOwnerId) return false;
	for (const auto& Input : InCapture.Surfaces)
	{
		const auto* Component = Input.Component.Get();
		if (!Input.Surface || !Input.Surface->Access || !Input.Surface->Access->IsCurrent() || !Component ||
			!Component->IsRegistered() || &Component->GetSurface() != Input.Surface.Get() ||
			!Input.Transform.Equals(Component->GetComponentTransform(), 0.0) ||
			!Module->IsNavigationInputCurrent(Input.Surface->InputVersion)) return false;
	}
	return !InCapture.Surfaces.IsEmpty();
}

void AVoxelNavigationData::UnbindTerrain(const UVoxelModule& InModule)
{
	check(IsInGameThread());
	if (TerrainModule.Get() == &InModule) ClearTerrainEvidence();
}

bool AVoxelNavigationData::StageGeneratedTile(const TSharedRef<FVoxelNavigationTileCapture, ESPMode::ThreadSafe>& InCapture)
{
	check(IsInGameThread());
	if (!StorageLease || InCapture->WorldEpoch != WorldEpoch || InCapture->EvidenceOwnerId != EvidenceOwnerId ||
		InCapture->NativeTiles.IsEmpty() || PendingTiles.Num() + InCapture->NativeTiles.Num() > EvidenceCapacity * 2) return false;
	for (const auto Ref : InCapture->NativeTiles) PendingTiles.Add(Ref, InCapture);
	return true;
}

#if WITH_RECAST
void AVoxelNavigationData::OnNavMeshTilesUpdated(const TArray<FNavTileRef>& InChangedTiles)
{
	check(IsInGameThread());
	Super::OnNavMeshTilesUpdated(InChangedTiles);
	for (const auto Ref : InChangedTiles)
	{
#if WITH_RECAST
		const auto* Native = GetRecastMesh();
		const int32 Slot = Native ? int32(Native->decodePolyIdTile(uint64(Ref))) : INDEX_NONE;
		if (const auto* Previous = CommittedTiles.Find(Slot); Previous && Previous->Reference == Ref)
		{
			Previous->Capture->Access.Revoke();
			CommittedTiles.Remove(Slot);
		}
#endif
		TSharedPtr<const FVoxelNavigationTileCapture, ESPMode::ThreadSafe> Capture;
		if (PendingTiles.RemoveAndCopyValue(Ref, Capture))
		{
#if WITH_RECAST
			if (Native && Slot >= 0 && Slot < EvidenceCapacity && Native->getTileByRef(uint64(Ref)) &&
				IsCaptureCurrent(*Capture))
			{
				if (const auto* Previous = CommittedTiles.Find(Slot); Previous && Previous->Capture != Capture)
				{
					Previous->Capture->Access.Revoke();
				}
				Capture->Access.Publish();
				CommittedTiles.Add(Slot, {Ref, Capture});
				for (const auto& Input : Capture->Surfaces)
				{
					if (Input.Surface->WorkReceipt) Input.Surface->WorkReceipt->Resolve(true);
				}
			}
#endif
		}
	}
}
#endif

bool AVoxelNavigationData::HasCurrentTileForNode(const NavNodeRef InNode, const FVoxelNavigationInputVersion& InVersion) const
{
	check(IsInGameThread());
#if WITH_RECAST
	const auto* Native = GetRecastMesh();
	const dtMeshTile* Tile = nullptr;
	const dtPoly* Polygon = nullptr;
	if (!Native || dtStatusFailed(Native->getTileAndPolyByRef(InNode, &Tile, &Polygon)) || !Tile || !Polygon) return false;
	const auto Ref = FNavTileRef(Native->getTileRef(Tile));
	const auto* Entry = CommittedTiles.Find(int32(Native->decodePolyIdTile(uint64(Ref))));
	if (!Entry || Entry->Reference != Ref || !Entry->Capture || !Entry->Capture->Access.IsCurrent() ||
		!IsCaptureCurrent(*Entry->Capture)) return false;
	for (const auto& Input : Entry->Capture->Surfaces)
	{
		if (!Input.Surface->Indices.IsEmpty() && Input.Surface->InputVersion == InVersion) return true;
	}
#endif
	return false;
}

void AVoxelNavigationData::ClearTerrainEvidence()
{
	check(IsInGameThread());
	bTerrainClearPending = true;
	bQueryAdmissionOpen.Store(false);
	if (auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
		System && !System->TryCloseTerrainQueries(*this)) return;
	if (!TryBeginNativeWrite()) return;
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	RevokeAllPathInputs(false);
	for (const auto& Entry : CommittedTiles) Entry.Value.Capture->Access.Revoke();
	PendingRepaths.Empty();
	PendingRepathPaths.Empty();
	ActivePaths.Empty();
	ObservedPaths.Empty();
	RepathRequests.Empty();
	NextObservedPathsTickInSeconds = -1.f;
	CommittedTiles.Empty();
	PathInputs.Empty();
	InFlightPaths.Empty();
	{
		FScopeLock Lock(&AsyncTicketsLock);
		AsyncTickets.Empty();
	}
	PendingTiles.Empty();
	BlockedTiles.Empty();
	DeferredRemovals.Empty();
	RemovalScratch.Empty();
	RemovalReferenceScratch.Empty();
	StorageLease.Reset();
	Resources.Reset();
	TerrainModule.Reset();
	WorldEpoch = 0;
	EvidenceCapacity = 0;
	BlockedCursor = 0;
	PathInputCursor = 0;
	AsyncTicketCursor = 0;
	ActivePathCursor = 0;
	ObservedPathCursor = 0;
	ObservedPathsRemaining = 0;
	RepathCursor = 0;
	RepathReturnCursor = 0;
	PathMaintenancePhase = 0;
	LastPathObservationWorldTime = -1.0;
	PathMaintenanceFrame = MAX_uint64;
	TerrainBindingId.Invalidate();
	bNativeRebuildPending = false;
	bNativeBoundsPending = false;
	bNativeCancelPending = false;
	bNativeInvokersPending = false;
	bTerrainClearPending = false;
}

void AVoxelNavigationData::InvalidateNativeEvidence()
{
	check(IsInGameThread());
	RevokeAllPathInputs(true, true);
	for (const auto& Entry : CommittedTiles) Entry.Value.Capture->Access.Revoke();
	EvidenceOwnerId = FGuid::NewGuid();
	CommittedTiles.Reset();
	PendingTiles.Reset();
	BlockedTiles.Reset();
	BlockedCursor = 0;
}

void AVoxelNavigationData::HoldBlockedTile(const FIntPoint& InCoordinate, const FBox& InBounds)
{
	check(IsInGameThread());
	FCaptureWait Wait{InCoordinate, InBounds, Resources->Snapshot().ReleaseVersion};
	if (BlockedTiles.Contains(Wait) || BlockedTiles.Num() < EvidenceCapacity * 2)
	{
		BlockedTiles.Add(Wait);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Voxel navigation exceeded its fixed native tile capacity at %s"), *InCoordinate.ToString());
	}
}

void AVoxelNavigationData::RetryBlockedTiles(FVoxelTerrainFrameContext& InFrame)
{
	check(IsInGameThread());
	if (!Resources || BlockedTiles.IsEmpty()) return;
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation) return;
	const uint64 Release = Resources->Snapshot().ReleaseVersion;
	const double Deadline = InFrame.Deadline(0.1);
	for (int32 Step = 0; Step < 32 && !BlockedTiles.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (InFrame.IsActive() && !InFrame.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission,
			Started, RetryCostMilliseconds)) break;
		BlockedCursor %= BlockedTiles.GetMaxIndex();
		const auto Id = FSetElementId::FromInteger(BlockedCursor++);
		if (BlockedTiles.IsValidId(Id) && Release > BlockedTiles.Get(Id).ReleaseVersion)
		{
			Navigation->AddDirtyArea(BlockedTiles.Get(Id).Bounds, ENavigationDirtyFlag::Geometry);
			BlockedTiles.Remove(Id);
		}
		const double Finished = FPlatformTime::Seconds();
		RetryCostMilliseconds = FMath::Lerp(RetryCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (InFrame.IsActive()) InFrame.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
}

void AVoxelNavigationData::BeginDestroy()
{
	ClearTerrainEvidence();
	Super::BeginDestroy();
}

void AVoxelNavigationData::CleanUp()
{
	check(IsInGameThread());
	bNativeCleanupPending = true;
	bWorldCloseRequested = true;
	if (auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
		System && !System->TryCloseTerrainQueries(*this)) return;
	if (!CanCleanupNativeTerrain()) return;
	if (NativeWriteDepth != 0 || !TryBeginNativeWrite()) return;
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	bNativeCleanupPending = false;
	ClearTerrainEvidence();
	Super::CleanUp();
}

bool AVoxelNavigationData::IsReadyForFinishDestroy()
{
	if (!CanCleanupNativeTerrain()) return false;
	if (!Super::IsReadyForFinishDestroy() || NativeWriteDepth != 0 || !TryBeginNativeWrite()) return false;
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	if (bNativeCleanupPending)
	{
		bNativeCleanupPending = false;
		ClearTerrainEvidence();
		Super::CleanUp();
	}
	if (bTerrainClearPending) ClearTerrainEvidence();
	return Super::IsReadyForFinishDestroy();
}

bool AVoxelNavigationData::CanCleanupNativeTerrain() const
{
	check(IsInGameThread());
	if (NativeWriteDepth != 0 || NativeQueryReaders.Load() != 0) return false;
#if WITH_RECAST
	const auto* Generator = static_cast<const FVoxelRecastNavMeshGenerator*>(GetGenerator());
	if (Generator && !Generator->CanCancelWithoutWaiting()) return false;
#endif
	return true;
}

#if WITH_RECAST
void AVoxelNavigationData::UpdateActiveTiles(const TArray<FNavigationInvokerRaw>& InInvokers)
{
	if (!TryBeginNativeWrite())
	{
		// 生产入口由NavigationSystem调用；恢复时读取其权威当前位置，不复制移动轨迹。
		bNativeInvokersPending = true;
		return;
	}
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	bNativeInvokersPending = false;
	Super::UpdateActiveTiles(InInvokers);
}

void AVoxelNavigationData::RemoveTiles(const TArray<FIntPoint>& InTiles)
{
	if (!TryBeginNativeWrite())
	{
		// 当前Detour被实际读锁保留；只登记仍存在的原生坐标，数量不超过固定slot容量。
		for (const FIntPoint& Coordinate : InTiles)
		{
			RemovalReferenceScratch.Reset();
			GetNavMeshTilesAt(Coordinate.X, Coordinate.Y, RemovalReferenceScratch);
			if (!RemovalReferenceScratch.IsEmpty())
			{
				check(DeferredRemovals.Contains(Coordinate) || DeferredRemovals.Num() < EvidenceCapacity);
				DeferredRemovals.Add(Coordinate);
			}
		}
		RemovalReferenceScratch.Reset();
		return;
	}
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	Super::RemoveTiles(InTiles);
}
#endif

#if WITH_RECAST
FRecastNavMeshGenerator* AVoxelNavigationData::CreateGeneratorInstance()
{
	return new FVoxelRecastNavMeshGenerator(*this);
}
#endif
