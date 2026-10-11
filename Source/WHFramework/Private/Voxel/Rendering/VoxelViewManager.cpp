#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

#include "Algo/BinarySearch.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelProxyPageBuild.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"
#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"
#include "Voxel/Rendering/VoxelRemotePageBuild.h"
#include "Voxel/Rendering/VoxelVolumeInterfaceMesh.h"
#include "Voxel/Rendering/VoxelHeightfieldInterfaceMesh.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

struct FVoxelHeightfieldCoveragePlan final : FVoxelTaskCustomPayload
{
	uint64 Signature = 0;
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	virtual uint64 GetAllocatedBytes() const override { return Edges.GetAllocatedSize(); }
};

struct FVoxelVolumeCoveragePlan final : FVoxelTaskCustomPayload
{
	uint64 Signature = 0;
	TSet<FVoxelViewKey> Nodes;
	TArray<FVoxelVolumeTransitionFace> Faces;
	virtual uint64 GetAllocatedBytes() const override
	{
		return Nodes.GetAllocatedSize() + Faces.GetAllocatedSize();
	}
};

namespace
{
	struct FGrantedCoverageApply
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreviousLease;
		TFunction<void(FVoxelTaskResult&&)> Apply;
		void* Context = nullptr;
		void (*BeforeApply)(void*, const FVoxelTaskResult&) = nullptr;

		void Consume(FVoxelTaskResult&& InResult) const
		{
			if (BeforeApply) BeforeApply(Context, InResult);
			Apply(MoveTemp(InResult));
		}
	};

	struct FDeferredGrantedCoverageApply
	{
		TSharedPtr<FGrantedCoverageApply, ESPMode::ThreadSafe> Apply;
		TSharedPtr<FVoxelTaskResult, ESPMode::ThreadSafe> Result;
		void operator()() const { Apply->Consume(MoveTemp(*Result)); }
	};

	bool RetainRawPublication(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease,
		const FVoxelTerrainGeometry& Geometry, FVoxelTerrainRenderer& Renderer)
	{
		if (!Lease || Lease->Transfer(EVoxelTerrainResourcePool::Resident)) return true;
		FIntVector Min, Max;
		return Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Min, Max) &&
			Renderer.HasPresentedCoverage(FBox(FVector(Min), FVector(Max))) &&
			Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap);
	}
struct FRepresentationOverlayRead
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> FactResources;
	FVoxelGenerationConsumerKey FactConsumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TAtomic<bool> bWaitingFacts{false};
	TAtomic<uint64> FactCapacityVersion{0};
	uint64 BeforeFactBuild = 0;
	FVoxelFineOverlayRead Reader;
	TSharedPtr<FVoxelFineOverlayReadResult, ESPMode::ThreadSafe> Result;

	void Begin()
	{
		bWaitingFacts.Store(false);
		BeforeFactBuild = FactResources->ReleaseVersionExcludingOwner(FactConsumer);
	}

	bool IsBlocked() const
	{
		if (!bWaitingFacts.Load()) return false;
		uint64 Version = 0;
		return !FactResources->TryGetReleaseVersionExcludingOwner(FactConsumer, Version) || Version <= FactCapacityVersion.Load();
	}

	bool DeferFacts(FVoxelTaskResult& OutTask)
	{
		if (OutTask.bCanceled || !OutTask.Error.StartsWith(TEXT("CapacityBlocked"))) return false;
		FactCapacityVersion.Store(BeforeFactBuild);
		bWaitingFacts.Store(true);
		OutTask.bSuccess = OutTask.bContinue = true;
		OutTask.Error.Reset();
		return true;
	}

	bool Advance(FVoxelTaskResult& OutTask, const TAtomic<bool>& InCancel)
	{
		bWaitingFacts.Store(false);
		if (!Result)
		{
			bool bComplete = false;
			if (!Reader.Advance(1, InCancel, bComplete, OutTask.Error))
			{
				if (OutTask.Error.StartsWith(TEXT("CapacityBlocked")))
				{
					FactCapacityVersion.Store(BeforeFactBuild);
					bWaitingFacts.Store(true);
					OutTask.bSuccess = OutTask.bContinue = true;
					OutTask.Error.Reset();
				}
				return false;
			}
			if (!bComplete)
			{
				if (OutTask.Error.StartsWith(TEXT("CapacityBlocked")))
				{
					FactCapacityVersion.Store(BeforeFactBuild);
					bWaitingFacts.Store(true);
					OutTask.Error.Reset();
				}
				OutTask.bSuccess = true;
				OutTask.bContinue = true;
				return false;
			}
			auto Taken = MetadataLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineOverlayReadResult>(MetadataLease) :
				MakeShared<FVoxelFineOverlayReadResult, ESPMode::ThreadSafe>();
			if (!Reader.TakeResult(*Taken))
			{
				OutTask.Error = TEXT("表示构建无法取得完整Overlay读取结果");
				return false;
			}
			Result = MoveTemp(Taken);
		}
		OutTask.OverlayReadResult = Result;
		return true;
	}
};
struct FProxyMeshEstimate
{
	FVoxelProxyPageMesh Counter;
	TSharedPtr<FVoxelVoxelProxyData, ESPMode::ThreadSafe> Data;
};
	struct FHeightfieldMeshEstimate
	{
		FVoxelHeightfieldPageMesh Counter;
		TSharedPtr<FVoxelSurfaceTileData, ESPMode::ThreadSafe> Surface;
		TSharedPtr<FVoxelWaterSurfaceTileData, ESPMode::ThreadSafe> Water;
		TSharedPtr<FVoxelMacroTileData, ESPMode::ThreadSafe> Macro;
		bool bCompacted = false;
	};
	constexpr int32 ViewSectionSide = 16;

	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainEnvironmentData(
		const FVoxelGenerationPipeline& Generator, const FVoxelGenerationBounds& Bounds)
	{
		const int64 Width = static_cast<int64>(Bounds.Max.X) - Bounds.Min.X;
		const int64 Height = static_cast<int64>(Bounds.Max.Y) - Bounds.Min.Y;
		return Generator.RetainHydrologyForPlanning(
			FIntPoint(static_cast<int32>(static_cast<int64>(Bounds.Min.X) + Width / 2),
				static_cast<int32>(static_cast<int64>(Bounds.Min.Y) + Height / 2)),
			static_cast<int32>((FMath::Max(Width, Height) + 1) / 2));
	}

	struct FHeightfieldTransitionTaskPayload : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;

		virtual uint64 GetAllocatedBytes() const override
		{
			return sizeof(*this) + (Mesh ? Mesh->Bytes() : 0);
		}
	};

	struct FHeightfieldTransitionTileSource
	{
		TSharedPtr<const FVoxelSurfaceTileData, ESPMode::ThreadSafe> Surface;
		TSharedPtr<const FVoxelMacroTileData, ESPMode::ThreadSafe> Macro;

		FVoxelHeightfieldTileView View() const
		{
			return Surface ? FVoxelHeightfieldTransitionBuilder::MakeView(*Surface)
				: FVoxelHeightfieldTransitionBuilder::MakeView(*Macro);
		}
	};

	struct FHeightfieldTransitionBuild
	{
		TSharedPtr<FVoxelCoarseBuildStorage, ESPMode::ThreadSafe> Storage;
		TSharedPtr<const FVoxelFrozenHeightfieldInputs, ESPMode::ThreadSafe> Inputs;
		TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView> Views;
		FVoxelHeightfieldInterfaceMesh Counter, Builder;
		uint64 GeometryLimit = 0, MeshBytes = 0;
		double TextureStretch = 8.0;
		int32 Phase = 0;

		FVoxelTaskResult Advance(const FVoxelGenerationRuntimeConfig& Config, const FVoxelRegistrySnapshot& Registry,
			const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load()) { Result.bCanceled = true; return Result; }
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
			if (!(Phase < 2 ? Storage->BeginCount(Result, Working) : Storage->Begin(Result, Working))) return Result;
			bool bComplete = false;
			if (Phase == 0)
			{
				Views.Reserve(Inputs->Sources.Num());
				for (const auto& Source : Inputs->Sources)
				{
					if (!Source.Surface && !Source.Macro) { Result.Error = TEXT("Missing frozen heightfield side"); return Result; }
					Views.Add(Source.Key, Source.Surface ? FVoxelHeightfieldTransitionBuilder::MakeView(*Source.Surface) :
						FVoxelHeightfieldTransitionBuilder::MakeView(*Source.Macro));
				}
				Counter.Initialize(true, Storage->WorkingCapacityBytes, GeometryLimit); ++Phase;
			}
			else if (Phase == 1)
			{
				FVoxelSectionMeshResult Counted;
				if (!Counter.Advance(*Inputs, Views, Config, Registry, Config.Recipe->Settings.MinZ, TextureStretch,
					Counted, bComplete, Result.Error, Cancel)) return Result;
				if (bComplete)
				{
					MeshBytes = Counter.EstimatedBytes();
					Storage->GeometryCapacityBytes = MeshBytes + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
						VoxelBoundedAllocation::WorkerSharedBytes<FHeightfieldTransitionTaskPayload>();
					if (Storage->GeometryCapacityBytes > GeometryLimit)
					{ Result.Error = TEXT("Oversized: complete heightfield interface geometry"); return Result; }
					Builder.Initialize(false, Storage->WorkingCapacityBytes, GeometryLimit, MeshBytes); ++Phase;
				}
			}
			else
			{
				auto Payload = Storage->MakeResult<FHeightfieldTransitionTaskPayload>();
				Payload->Mesh = Storage->MakeResult<FVoxelSectionMeshResult>();
				if (!Builder.Advance(*Inputs, Views, Config, Registry, Config.Recipe->Settings.MinZ, TextureStretch,
					*Payload->Mesh, bComplete, Result.Error, Cancel)) return Result;
				if (bComplete)
				{
					Payload->Mesh->ResourceLease = Storage->GeometryLease;
					Payload->Mesh->FrozenHeightfieldInputs = Inputs;
					Result.bSuccess = true;
					if (!Storage->SettleCompleted(Result, Payload->Mesh.Get(),
						VoxelBoundedAllocation::WorkerSharedBytes<FHeightfieldTransitionTaskPayload>()))
					{
						return Result;
					}
					Result.CustomPayload = MoveTemp(Payload); Result.InputResourceLease = Storage->ResultInputLease;
					Result.OutputResourceLease = Storage->GeometryLease;
					Result.bSuccess = true; return Result;
				}
			}
			Result.bSuccess = Result.bContinue = true; return Result;
		}
	};

	FVoxelGenerationConsumerKey MakeCoarseGenerationConsumer(const FVoxelTerrainGeometry& Geometry, const FVoxelTaskStamp& Stamp)
	{
		FIntVector Min, Max;
		if (!Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Min, Max)) return {};
		return {Geometry.Key.IsColumn() ? EVoxelGenerationConsumerKind::Heightfield : EVoxelGenerationConsumerKind::VoxelProxy,
			Stamp.WorldEpoch, Stamp.OwnerId, Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Bounds, Min, Max, Stamp.Token};
	}

	TSharedPtr<FVoxelCoarseBuildStorage, ESPMode::ThreadSafe> CreateCoarseBuildStorage(
		FVoxelTaskScheduler& Scheduler, FVoxelTaskRequest& Request,
		const FVoxelTerrainGeometry& Geometry, const uint64 InControllerBytes,
		const int32 InShardArrays = 2, const int32 InInitialShardArrays = 0)
	{
		const FVoxelGenerationConsumerKey Consumer = MakeCoarseGenerationConsumer(Geometry, Request.Stamp);
		if (!Consumer.IsValid()) return nullptr;
		int32 InitialShardCount = 0;
		FString Error;
		if (InInitialShardArrays && !VoxelStreamingGeometry::CountShards(Geometry,
			Geometry.Key.IsColumn() ? 32 : 16, 4096, InitialShardCount, Error))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return nullptr;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>() + InControllerBytes +
			InShardArrays * VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096) +
			InInitialShardArrays * VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(InitialShardCount);
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Metadata))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::InputMemory, Bytes);
			return nullptr;
		}
		const uint64 ResultBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTaskResult>() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineOverlayReadResult>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultInput;
		const auto ResultAdmission = Scheduler.GetTerrainResources()->TryAcquire(Consumer,
			EVoxelTerrainResourcePool::Input, ResultBytes, ResultInput, 0, Metadata->Bytes(), 1);
		if (ResultAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				ResultAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? EVoxelTaskBlockReason::InputMemory :
				EVoxelTaskBlockReason::Oversized, ResultBytes);
			return nullptr;
		}
		auto Storage = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoarseBuildStorage>(Metadata,
			Scheduler.GetTerrainResources(), Consumer, Metadata, ResultInput);
		Request.GenerationConsumer = Consumer;
		Request.SetRequiredBindingInput(Request.GetRequiredBindingInputBytes() + Metadata->Bytes() + ResultInput->Bytes(),
			Request.GetRequiredBindingInputLeaseCount() + 2);
		{
			FString CallbackBindingError;
			if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[Storage] { return Storage->BlockReason(); }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				return nullptr;
			}
		}
		// 自有租约已占用唯一四账本；旧 Scheduler 计数只保留未迁移任务。
		Request.InputBytes = 0;
		Request.ReservedBytes = 1;
		return Storage;
	}

	bool AdmitCoarseCompletion(FVoxelTaskScheduler& Scheduler, FVoxelTaskRequest& Request,
		const TSharedPtr<FVoxelCoarseBuildStorage, ESPMode::ThreadSafe>& Storage)
	{
		const auto Admission = Storage->ReserveCompletion();
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? EVoxelTaskBlockReason::Memory :
				EVoxelTaskBlockReason::Oversized, Request.InputBytes, Storage->GeometryCapacityBytes);
			return false;
		}
		Request.SetRequiredBindingInput(Request.GetRequiredBindingInputBytes(),
			Request.GetRequiredBindingInputLeaseCount() + 1);
		return true;
	}

	TAutoConsoleVariable<int32> CVarVoxelDebugRepresentation(
		TEXT("wh.Voxel.DebugRepresentation"),
		1,
		TEXT("Logs voxel representation Wanted/Ready/Actor/Visible state. 0=off, 1=summary, 2=nearby keys."),
		ECVF_Cheat);

	void AppendNonEmptyBatches(
		FVoxelSectionMeshResult& InOutTarget,
		FVoxelSectionMeshResult&& InSource)
	{
		for (FVoxelRenderBatch& Batch : InSource.Batches)
		{
			if (!Batch.Mesh.Vertices.IsEmpty() && !Batch.Mesh.Triangles.IsEmpty())
			{
				InOutTarget.Batches.Add(MoveTemp(Batch));
			}
		}
	}

}

FVoxelViewManager::FVoxelViewManager(
	UVoxelModule& InModule,
	FVoxelTaskScheduler& InScheduler,
	const uint64 InWorldEpoch)
	: Module(InModule),
	  Scheduler(InScheduler),
	  WorldEpoch(InWorldEpoch),
	  TerrainFrameBudget(InModule.GetTerrainFrameContext().GetPublishBudget())
{
	const auto SelectionConsumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation, {}, 1);
	FineShardAdmissionSelection.BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
	FineShardPresentationSelection.BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
	for (int32 Stage = 0; Stage < 2; ++Stage)
	{
		AdmissionSelection[Stage].BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
		TransitionAdmissionSelection[Stage].BindResources(Scheduler.GetTerrainResources(), SelectionConsumer);
	}
	BindCoverageIndexResources();
	Publisher = MakeUnique<FVoxelViewPublisher>(Module, Scheduler, WorldEpoch);
	Publisher->SetGeometryValidator([this](const FVoxelTaskStamp& Stamp)
	{
		return IsRepresentationStampCurrent(Stamp);
	});
	Publisher->SetMeshValidator([this](const FVoxelSectionMeshResult& Mesh)
	{
		return (!Mesh.FrozenBoundaryInputs || AreBoundaryInputsCurrent(*Mesh.FrozenBoundaryInputs)) &&
			(!Mesh.FrozenHeightfieldInputs || AreHeightfieldInputsCurrent(*Mesh.FrozenHeightfieldInputs));
	});
	Publisher->SetCommitGate([this](const FVoxelPublishGroupKey& Key)
	{
		const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
		return !Bounds.IsValid || !ShouldDeferCoverage(FVoxelCoverageRect{
			FIntPoint(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()), FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize())),
			FIntPoint(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()), FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()))});
	});
}

FVoxelViewManager::~FVoxelViewManager()
{
	Reset();
}

void FVoxelViewManager::Tick(
	const uint64 InInterestRevision,
	const TConstArrayView<FVector> InObservers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewManager);
	const double TickStart = FPlatformTime::Seconds();
	bTerrainFrameBudgetActive = true;
	FrameTimings = {};
	LastServiceResourceAdmission = EVoxelTerrainResourceAdmission::None;
	AdvanceCoverageIndexRecovery(TerrainDeadline(0.03));
	{
		const double ApplyDeadline = TerrainDeadline(0.25);
		FDeferredCoverageCallback Callback;
		for (int32 Step = 0; Step < 32 && FPlatformTime::Seconds() < ApplyDeadline && DeferredCoverageCallbacks.Dequeue(Callback); ++Step)
		{
			const bool bBlocked = Callback.Bounds.IsValid() ? ShouldDeferCoverage(Callback.Bounds) : CoverageContinuation.IsValid();
			if (bBlocked) DeferredCoverageCallbacks.Enqueue(MoveTemp(Callback));
			else Callback.Apply();
		}
		for (int32 Step = 0; Step < 32 && !DeferredCoverageResults.IsEmpty() && FPlatformTime::Seconds() < ApplyDeadline; ++Step)
		{
			DeferredCoverageResultCursor %= DeferredCoverageResults.Num();
			const int32 Index = DeferredCoverageResultCursor++;
			if (ShouldDeferCoverage(DeferredCoverageResults[Index].Stamp)) continue;
			FVoxelTaskResult Result = MoveTemp(DeferredCoverageResults[Index]);
			DeferredCoverageResults.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			OnTask(MoveTemp(Result));
		}
	}
	Publisher->SetObservers(InObservers);
	const auto Run = [this](EVoxelPublishPhase Phase, auto&& Operation)
	{
		const double Start = FPlatformTime::Seconds();
		const uint8 Index = uint8(Phase);
		double& Estimate = ManagerPhaseEstimateMilliseconds[Index];
		const auto& Policy = Module.GetViewSettings().Budget;
		const double Floor = Policy.PublishOperationEstimateFloorMilliseconds;
		const double Ceiling = FMath::Min(double(Policy.PublishOperationEstimateCeilingMilliseconds), double(Policy.TerrainPublishMilliseconds) * 0.5);
		Estimate = FMath::Clamp(Estimate > 0.0 ? Estimate : Floor, Floor, Ceiling);
		const bool bCanYield = Phase == EVoxelPublishPhase::Admission || Phase == EVoxelPublishPhase::Prepare ||
			Phase == EVoxelPublishPhase::Retire;
		const double StartEstimate = bCanYield ? FMath::Min(Estimate, TerrainFrameBudget.RemainingMilliseconds(Start)) : Estimate;
		if (!TerrainFrameBudget.TryStart(Phase, Start, StartEstimate))
		{
			const uint64 Count = ++ManagerPhaseDeferrals[Index];
			if (Count == 60 || Count % 600 == 0)
				UE_LOG(LogTemp, Warning, TEXT("Voxel manager deferral: phase=%d consecutive=%llu estimateMs=%.3f"), Index, Count, Estimate);
			return false;
		}
		ManagerPhaseDeferrals[Index] = 0;
		Operation();
		const double End = FPlatformTime::Seconds();
		Estimate = FMath::Clamp(FMath::Lerp(Estimate, (End - Start) * 1000.0, 0.15), Floor, Ceiling);
		TerrainFrameBudget.Finish(Phase, Start, End);
		return true;
	};
	// 地形 GT 的发布、覆盖、准入、回收共用同一个期限；被推迟阶段保留队列/游标。
	const int32 FirstPhase = NextManagerPhase;
	NextManagerPhase = (FirstPhase + 1) % 5;
	for (int32 Attempt = 0; Attempt < 5 && TerrainFrameBudget.RemainingMilliseconds(FPlatformTime::Seconds()) > 0.0; ++Attempt)
	{
		const int32 Phase = (FirstPhase + Attempt) % 5;
		const bool bCurrentInterest = AppliedInterestSnapshot.IsValid();
		if (Phase == 0 &&
			(InterestWorkspace || CommittedWorkspaceEvents || !AppliedInterestSnapshot ||
			Module.GetDesiredInterestSnapshot() != Module.GetCurrentInterestSnapshot() ||
			AppliedInterestRevision != Module.GetDesiredInterestSnapshot()->PublicationRevision))
			AdvanceInterestInputs(InInterestRevision, InObservers);
		else if (Phase == 0 && bTaskPrioritiesDirty)
			Run(EVoxelPublishPhase::Priority, [this, InObservers]
			{
				PriorityObservers = TArray<FVector>(InObservers);
				const auto Latest = Module.GetCurrentInterestSnapshot();
				if (Latest && AppliedInterestSnapshot && Latest->PublicationRevision == AppliedInterestSnapshot->PublicationRevision &&
					!PendingInterestSnapshot)
					AppliedInterestSnapshot = Latest;
				RebuildAdmissions(InObservers);
				UpdateTaskPriorities();
				Publisher->InvalidatePriorities();
				bTaskPrioritiesDirty = false;
			});
		else if (Phase == 1 && bCurrentInterest)
		{
			Run(EVoxelPublishPhase::Commit, [this] { Publisher->GetTerrainRenderer().AdvanceFramePublication(); });
			Run(EVoxelPublishPhase::Retire, [this]
			{
				const double Deadline = TerrainDeadline(0.1);
				AdvanceNativeFineRetirement(Deadline);
				Publisher->GetTerrainRenderer().AdvanceRetirement(
					[this](const FIntVector& Section) { OnPresentationReady(Section); }, Deadline);
			});
			const bool bAllowCommit = true;
			Publisher->Tick(TerrainFrameBudget, bAllowCommit);
			if (bAllowCommit)
			{
				for (const auto& Key : Publisher->TakeRetryOwners())
				{
					const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module.BlockSize(), Module.GetViewSettings().Lod);
					if (Bounds.IsValid)
						MarkCoverageDirty(
							{FIntPoint(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()), FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize())),
								FIntPoint(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()), FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()))});
					if (Key.Representation == 0)
					{
						MarkFinePageDirty({Key.Coordinate, Key.Level});
						bFinePresentationDirty = true;
					}
				}
			}
		}
		else if (Phase == 2 && bCurrentInterest && (CoverageContinuation.IsValid() || bCoverageDirty))
			Run(EVoxelPublishPhase::Plan, [this]
			{
				const double Start = FPlatformTime::Seconds();
				if (!CoverageContinuation.IsValid())
				{
					bActiveCoverageFull = bCoverageDirtyFull;
					bCoverageDirtyFull = false;
					const int32 Count = bActiveCoverageFull ? 0 : FMath::Min(DirtyCoverageRects.Num(), 1);
					ActiveDirtyCoverageRects.Append(DirtyCoverageRects.GetData(), Count);
					DirtyCoverageRects.RemoveAt(0, Count, EAllowShrinking::No);
					bCoverageDirty = !DirtyCoverageRects.IsEmpty();
					CoverageInterestSnapshot = AppliedInterestSnapshot ? AppliedInterestSnapshot : Module.GetCurrentInterestSnapshot();
					CoverageInputRoot = AppliedInputRoot;
					CoverageStartedSeconds = Start;
					if (bActiveCoverageFull) ActiveCoveragePublishGroups = MoveTemp(OpenPublishGroups);
					CoverageContinuation = ResolveTransitionVisibilitySteps();
				}
				const double Deadline = TerrainDeadline(0.25);
				TGuardValue<bool> ReadScope(bCoverageReadScope, true);
				for (int32 Steps = 0; Steps < 4096 && !CoverageContinuation.IsComplete() && FPlatformTime::Seconds() < Deadline; ++Steps)
				{
					CoverageContinuation.Resume();
					++CoverageMicrosteps;
				}
				if (CoverageContinuation.IsComplete())
				{
					CoverageContinuation.Reset();
					CoverageInterestSnapshot.Reset();
					ReleaseInputRoot(CoverageInputRoot);
					ActiveDirtyCoverageRects.Reset();
					bActiveCoverageFull = true;
				}
				LastCoverageMilliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
				FrameTimings.CoverageMilliseconds += LastCoverageMilliseconds;
			});
		else if (Phase == 3 && bCurrentInterest)
		{
			const int32 FirstService = NextAdmissionPhase;
			NextAdmissionPhase = (FirstService + 1) % 5;
			for (int32 Service = 0; Service < 5 && TerrainFrameBudget.RemainingMilliseconds(FPlatformTime::Seconds()) > 0.0; ++Service)
			{
				switch ((FirstService + Service) % 5)
				{
				case 0:
					Run(EVoxelPublishPhase::Prepare, [this]
					{
						const double Deadline = TerrainDeadline(0.25);
						for (int32 Step = 0; Step < 32 && FPlatformTime::Seconds() < Deadline; ++Step)
						{
							PrepareFineShardPresentations(Deadline);
						}
					});
					break;
				case 1:
					Run(EVoxelPublishPhase::Admission, [this] { ProcessDataAdmissions(); });
					break;
				case 2:
					Run(EVoxelPublishPhase::Admission, [this] { ProcessFineShardAdmissions(); });
					break;
				case 3:
					Run(EVoxelPublishPhase::Admission, [this] { ProcessAdmissions(); });
					break;
				case 4:
					if (!bCoverageReadScope)
					{
						Run(EVoxelPublishPhase::Admission, [this] { PumpHeightfieldTransitions(); });
						Run(EVoxelPublishPhase::Admission, [this] { PumpVolumeTransitions(); });
					}
					break;
				}
			}
		}
		else if (Phase == 4 && bCurrentInterest &&
				 (RetirementContinuation.IsValid() || FPlatformTime::Seconds() >= NextRetireCheck))
			Run(EVoxelPublishPhase::Retire, [this]
			{
				CleanupRetiredRepresentations(FPlatformTime::Seconds());
				NextRetireCheck = FPlatformTime::Seconds();
			});
	}
	RefreshTerrainBuildStage();
	RefreshTerrainStatistics();
#if !UE_BUILD_SHIPPING
	if (TickStart >= NextInterestProgressLog)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Voxel publication input progress: desired=%llu applied=%llu pending=%llu phase=%d cursor=%d fine=%d admissions=%d geometry=%d"),
			InInterestRevision, AppliedInterestRevision, PendingInterestRevision, InterestInputPhase, InterestInputCursor, GetInputRoot().FineWanted.Num(), Admissions.Num(),
			GetViewInterest().Geometry.Num());
		NextInterestProgressLog = TickStart + 5.0;
		LogRepresentationState(InObservers);
		const auto& Accepted = GetViewInterest();
		UE_LOG(LogTemp, Display, TEXT("Voxel publication admission inputs: priorities=%d first=%d lanes=%d/%d/%d/%d admissionMs=%.6f"),
			AdmissionPriorities.Num(), AdmissionPriorities.IsEmpty() ? -1 : AdmissionPriorities[0],
			Accepted.AdmissionLanes[0].Num(), Accepted.AdmissionLanes[1].Num(), Accepted.AdmissionLanes[2].Num(), Accepted.AdmissionLanes[3].Num(),
			Module.GetViewSettings().Budget.AdmissionMilliseconds);
	}
#endif
	FrameTimings.PublishPrepareMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Prepare).Milliseconds;
	FrameTimings.PublishCommitMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Commit).Milliseconds;
	FrameTimings.PublishPlanMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Plan).Milliseconds;
	FrameTimings.PublishPriorityMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Priority).Milliseconds;
	FrameTimings.AdmissionMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Admission).Milliseconds;
	FrameTimings.RetireMilliseconds = TerrainFrameBudget.GetTiming(EVoxelPublishPhase::Retire).Milliseconds;
	FrameTimings.PublishMaximumOperationMilliseconds = 0.0;
	for (int32 Phase = 1; Phase < static_cast<int32>(EVoxelPublishPhase::Count); ++Phase)
		FrameTimings.PublishMaximumOperationMilliseconds = FMath::Max(FrameTimings.PublishMaximumOperationMilliseconds,
			TerrainFrameBudget.GetTiming(static_cast<EVoxelPublishPhase>(Phase)).MaximumOperationMilliseconds);
	FrameTimings.PublishTotalMilliseconds = TerrainFrameBudget.TotalMilliseconds(FPlatformTime::Seconds());
	FrameTimings.bPublishBudgetOverrun = TerrainFrameBudget.HasOverrun(FPlatformTime::Seconds());
	bTerrainFrameBudgetActive = false;
#if !UE_BUILD_SHIPPING
	if (FrameTimings.bPublishBudgetOverrun && FPlatformTime::Seconds() >= NextBudgetLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel terrain budget overrun: totalMs=%.3f maximumOperationMs=%.3f budgetMs=%.3f planMs=%.3f admissionMs=%.3f prepareMs=%.3f commitMs=%.3f retireMs=%.3f workers=%d invalidTiming=%d overloaded=%d"),
			FrameTimings.PublishTotalMilliseconds, FrameTimings.PublishMaximumOperationMilliseconds, Module.GetTerrainFrameContext().GetAllocation().TotalMilliseconds,
			FrameTimings.PublishPlanMilliseconds, FrameTimings.AdmissionMilliseconds, FrameTimings.PublishPrepareMilliseconds,
			FrameTimings.PublishCommitMilliseconds, FrameTimings.RetireMilliseconds,
			Module.GetTerrainFrameContext().GetAllocation().MaximumWorkers,
			Module.GetTerrainFrameContext().GetAllocation().bInvalidTiming,
			Module.GetTerrainFrameContext().GetAllocation().bBaseFrameOverloaded);
		NextBudgetLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
}

bool FVoxelViewManager::IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission, const bool bIncludePending) const
{
	if (InAdmission.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const FVoxelSection* Section = Module.GetRuntime()->FindSection(InAdmission.FineKey);
		return Section && Section->Status == EVoxelSectionStatus::DataReady;
	}
	if (IsAdmissionMeshReady(InAdmission)) return true;
	FVoxelTaskKey Key;
	Key.Stamp.WorldEpoch = WorldEpoch;
	EVoxelTaskKind MeshKind = EVoxelTaskKind::None;
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	switch (InAdmission.Kind)
	{
	case EVoxelViewAdmissionKind::VoxelProxy:
		Key.Kind = EVoxelTaskKind::GenerateVoxelProxy;
		MeshKind = EVoxelTaskKind::BuildVoxelProxy;
		Key.Stamp.ViewKey = InAdmission.ProxyKey;
		Key.Stamp.Revision = Hierarchy.GetVoxelProxyRevision(InAdmission.ProxyKey);
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.ProxyKey))
		{
			Key.Stamp = MakeRepresentationStamp(*Geometry, Key.Stamp.Revision);
		}
		else
		{
			return false;
		}
		break;
	case EVoxelViewAdmissionKind::Surface:
		Key.Kind = EVoxelTaskKind::GenerateSurface;
		MeshKind = EVoxelTaskKind::BuildSurface;
		Key.Stamp.SurfaceKey = InAdmission.SurfaceKey;
		Key.Stamp.Revision = Hierarchy.GetSurfaceRevision(InAdmission.SurfaceKey);
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.SurfaceKey))
		{
			Key.Stamp = MakeRepresentationStamp(*Geometry, Key.Stamp.Revision);
		}
		else
		{
			return false;
		}
		break;
	case EVoxelViewAdmissionKind::Macro:
		Key.Kind = EVoxelTaskKind::GenerateMacro;
		MeshKind = EVoxelTaskKind::BuildMacro;
		Key.Stamp.MacroKey = InAdmission.MacroKey;
		Key.Stamp.Revision = Hierarchy.GetMacroRevision(InAdmission.MacroKey);
		if (const auto* Geometry = GetViewInterest().FindGeometry(InAdmission.MacroKey))
		{
			Key.Stamp = MakeRepresentationStamp(*Geometry, Key.Stamp.Revision);
		}
		else
		{
			return false;
		}
		break;
	default:
		return false;
	}
	return PreparedData.Contains(Key) || HasDeferredRepresentation(Key.Stamp) || Scheduler.Has(Key.Stamp, MeshKind) ||
		(bIncludePending && Scheduler.Has(Key.Stamp, Key.Kind));
}

void FVoxelViewManager::ProcessDataAdmissions()
{
	ProcessServiceAdmissions(true);
}

void FVoxelViewManager::RecordServiceResourceAdmission(const EVoxelTerrainResourceAdmission InAdmission)
{
	if (LastServiceResourceAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
		LastServiceResourceAdmission == EVoxelTerrainResourceAdmission::InvalidRequest) return;
	if (InAdmission != EVoxelTerrainResourceAdmission::Granted || LastServiceResourceAdmission == EVoxelTerrainResourceAdmission::None)
		LastServiceResourceAdmission = InAdmission;
}

void FVoxelViewManager::EnumerateServiceResourceSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Visit = [&Resources, InVisit](const FVoxelTerrainResourceLease* Lease)
	{
		if (Lease && Lease->IsOwnedBy(*Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
	};
	for (int32 Stage = 0; Stage < 2; ++Stage)
	{
		Visit(AdmissionAgeLeases[Stage].Get());
		Visit(AdmissionSelection[Stage].GetResourceLease());
	}
	Visit(AppliedInputRoot ? AppliedInputRoot->ResourceLease.Get() : nullptr);
	const auto& Interest = GetViewInterest();
	for (const auto& Lease : Interest.CoarsePlanLeases) Visit(Lease.Get());
	Visit(Interest.DemandLease.Get());
	Visit(Interest.CompositionLease.Get());
	Visit(Interest.CopyLease.Get());
	Visit(Interest.FinalizationLease.Get());
	Visit(Interest.CoarseCompositionLease.Get());
}

bool FVoxelViewManager::ReserveServiceAdmissionAges(const int32 InStage, const int32 InCount)
{
	auto& Ages = AdmissionAges[InStage];
	auto& Lease = AdmissionAgeLeases[InStage];
	using FElement = TPair<FVoxelTaskKey, FAdmissionServiceAge>;
	if (InCount < 0)
	{
		RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable);
		return false;
	}
	if (VoxelGenerationAllocation::Private::SetCanReserve<FElement>(Ages, InCount)) return true;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = GetServiceControlConsumer();
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (bAdmissionAgeCapacityWaiting[InStage] && BeforeAdmission <= AdmissionAgeCapacityVersions[InStage])
	{
		RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityBlocked);
		return false;
	}
	const int64 CapacityWide = FMath::Max<int64>(8, FMath::Max<int64>(InCount, int64(AdmissionAgeCapacities[InStage]) * 2));
	if (CapacityWide > MAX_int32)
	{
		RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable);
		UE_LOG(LogTemp, Error, TEXT("CapacityUnachievable: terrain admission age count"));
		return false;
	}
	const int32 Capacity = int32(CapacityWide);
	const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(Ages);
	const uint64 NewBytes = VoxelGenerationAllocation::Private::SetReserveBytes<FElement>(Ages, Capacity);
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (NewBytes > MAX_uint64 - OldBytes ||
		!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Lease.Get(), Required, Error))
	{
		RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable);
		UE_LOG(LogTemp, Error, TEXT("CapacityUnachievable: terrain admission age peak %s"), *Error);
		return false;
	}
	const auto Admission = Lease ? Lease->TryResize(OldBytes + NewBytes, Required.Bytes[0], Required.LeaseCount) :
		Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, OldBytes + NewBytes, Lease, 0, Required.Bytes[0], Required.LeaseCount);
	RecordServiceResourceAdmission(Admission);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		bAdmissionAgeCapacityWaiting[InStage] = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		AdmissionAgeCapacityVersions[InStage] = BeforeAdmission;
		if (Admission != EVoxelTerrainResourceAdmission::CapacityBlocked)
			UE_LOG(LogTemp, Error, TEXT("Terrain admission age rejected: stage=%d admission=%d"), InStage, int32(Admission));
		return false;
	}
	Ages.Reserve(Capacity);
	const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Ages);
	check(ActualBytes <= NewBytes);
	const auto Settled = Lease->TryResize(ActualBytes);
	check(Settled == EVoxelTerrainResourceAdmission::Granted);
	AdmissionAgeCapacities[InStage] = Capacity;
	bAdmissionAgeCapacityWaiting[InStage] = false;
	return true;
}

bool FVoxelViewManager::IsServiceAdmissionIdentityCurrent(const FVoxelTaskKey& InIdentity) const
{
	if (InIdentity.Stamp.WorldEpoch != WorldEpoch) return false;
	FVoxelViewAdmission Admission;
	const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	uint64 Revision = 0;
	const FVoxelTerrainGeometry* Geometry = nullptr;
	switch (InIdentity.Kind)
	{
	case EVoxelTaskKind::BuildFineMesh:
	{
		Admission.Kind = EVoxelViewAdmissionKind::Fine;
		Admission.FineKey = InIdentity.Stamp.Section;
		const auto* Section = Module.GetRuntime()->FindSection(Admission.FineKey);
		if (!GetInputRoot().FineWanted.Contains(Admission.FineKey) || !Section || Section->Status != EVoxelSectionStatus::DataReady ||
			Section->Stamp.Token != InIdentity.Stamp.Token || Section->CommittedRevision != InIdentity.Stamp.Revision) return false;
		break;
	}
	case EVoxelTaskKind::GenerateVoxelProxy:
	case EVoxelTaskKind::BuildVoxelProxy:
		Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
		Admission.ProxyKey = InIdentity.Stamp.ViewKey;
		if (!GetInputRoot().ProxyWanted.Contains(Admission.ProxyKey)) return false;
		Geometry = GetViewInterest().FindGeometry(Admission.ProxyKey);
		Revision = Hierarchy.GetVoxelProxyRevision(Admission.ProxyKey);
		break;
	case EVoxelTaskKind::GenerateSurface:
	case EVoxelTaskKind::BuildSurface:
		Admission.Kind = EVoxelViewAdmissionKind::Surface;
		Admission.SurfaceKey = InIdentity.Stamp.SurfaceKey;
		if (!GetInputRoot().SurfaceWanted.Contains(Admission.SurfaceKey)) return false;
		Geometry = GetViewInterest().FindGeometry(Admission.SurfaceKey);
		Revision = Hierarchy.GetSurfaceRevision(Admission.SurfaceKey);
		break;
	case EVoxelTaskKind::GenerateMacro:
	case EVoxelTaskKind::BuildMacro:
		Admission.Kind = EVoxelViewAdmissionKind::Macro;
		Admission.MacroKey = InIdentity.Stamp.MacroKey;
		if (!GetInputRoot().MacroWanted.Contains(Admission.MacroKey)) return false;
		Geometry = GetViewInterest().FindGeometry(Admission.MacroKey);
		Revision = Hierarchy.GetMacroRevision(Admission.MacroKey);
		break;
	default:
		return false;
	}
	if (Admission.Kind != EVoxelViewAdmissionKind::Fine &&
		(!Geometry || !IsRepresentationStampCurrent(InIdentity.Stamp) || *Geometry != InIdentity.Stamp.GeometryIdentity ||
			(Module.IsAuthority() ? Revision != InIdentity.Stamp.Revision : Revision > InIdentity.Stamp.Revision))) return false;
	const bool bDataOnly = InIdentity.Kind == EVoxelTaskKind::GenerateVoxelProxy ||
		InIdentity.Kind == EVoxelTaskKind::GenerateSurface || InIdentity.Kind == EVoxelTaskKind::GenerateMacro;
	return !IsAdmissionTerminalFailure(Admission) && !(bDataOnly ? IsAdmissionDataReady(Admission, true) : IsAdmissionMeshReady(Admission));
}

void FVoxelViewManager::PruneServiceAdmissionAges(const int32 InStage, const double InDeadline)
{
	auto& Ages = AdmissionAges[InStage];
	int32& Cursor = AdmissionAgeCleanupCursors[InStage];
	for (int32 Step = 0; Step < 32 && !Ages.IsEmpty() && FPlatformTime::Seconds() < InDeadline; ++Step)
	{
		Cursor %= Ages.GetMaxIndex();
		const FSetElementId Id = FSetElementId::FromInteger(Cursor++);
		if (!Ages.IsValidId(Id)) continue;
		const FVoxelTaskKey Identity = Ages.Get(Id).Key;
		if (!IsServiceAdmissionIdentityCurrent(Identity)) Ages.Remove(Identity);
	}
	if (Ages.IsEmpty())
	{
		const bool bRetiredStorage = AdmissionAgeLeases[InStage].IsValid();
		Ages.Empty();
		AdmissionAgeLeases[InStage].Reset();
		AdmissionAgeCapacities[InStage] = 0;
		Cursor = 0;
		if (bRetiredStorage)
		{
			bAdmissionAgeCapacityWaiting[InStage] = false;
			bServiceSelectionCapacityWaiting[InStage] = false;
		}
	}
}

void FVoxelViewManager::ProcessServiceAdmissions(const bool bInDataOnly)
{
	const auto& Interest = GetViewInterest();
	const auto& Policy = Module.GetViewSettings();
	const int32 Stage = bInDataOnly ? 0 : 1;
	const int32 Limits[] = {bInDataOnly ? 0 : Policy.Budget.FineAdmissionPerFrame,
		bInDataOnly ? Policy.Budget.DataAdmissionPerFrame : Policy.Budget.VoxelProxyAdmissionPerFrame,
		bInDataOnly ? Policy.Budget.DataAdmissionPerFrame : Policy.Budget.SurfaceAdmissionPerFrame,
		bInDataOnly ? Policy.Budget.DataAdmissionPerFrame : Policy.Budget.MacroAdmissionPerFrame};
	int32 Submitted[4] = {};
	const double Deadline = TerrainDeadline(Policy.Budget.AdmissionMilliseconds);
	const double StartedScanning = FPlatformTime::Seconds();
	if (StartedScanning >= Deadline) return;
	const double ScanDeadline = StartedScanning + (Deadline - StartedScanning) * 0.5;
	PruneServiceAdmissionAges(Stage, ScanDeadline);
	const auto Resources = Scheduler.GetTerrainResources();
	FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			static_cast<const FVoxelViewManager*>(Context)->EnumerateServiceResourceSources(Visit);
		});
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<128>> Candidates;
	TArray<FVoxelTaskKey, TInlineAllocator<128>> Identities;
	TArray<int32, TInlineAllocator<128>> CandidateAdmissions;
	const int32 MinimumKind = bInDataOnly ? 1 : 0;
	const int32 KindCount = 4 - MinimumKind;
	const int32 FirstKind = MinimumKind + ServiceAdmissionKinds[Stage];
	ServiceAdmissionKinds[Stage] = (ServiceAdmissionKinds[Stage] + 1) % KindCount;
	for (int32 Offset = 0; Offset < KindCount && FPlatformTime::Seconds() < ScanDeadline; ++Offset)
	{
		const int32 Kind = MinimumKind + (FirstKind - MinimumKind + Offset) % KindCount;
		const auto& Lane = Interest.AdmissionLanes[Kind];
		int32& Scan = ServiceAdmissionScans[Stage][Kind];
		const int32 ScanCount = FMath::Min(32, Lane.Num());
		for (int32 Attempt = 0; Attempt < ScanCount && FPlatformTime::Seconds() < ScanDeadline; ++Attempt)
		{
			Scan %= Lane.Num();
			const int32 Index = Lane[Scan++];
			const auto& Admission = Admissions[Index];
			FVoxelTaskKey Identity;
			const FVoxelTerrainGeometry* Geometry = nullptr;
			const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
			uint64 Revision = 0;
			switch (Admission.Kind)
			{
			case EVoxelViewAdmissionKind::Fine:
			{
				const FVoxelSection* Section = Module.GetRuntime()->FindSection(Admission.FineKey);
				if (!Section || Section->Status != EVoxelSectionStatus::DataReady) continue;
				Identity.Kind = EVoxelTaskKind::BuildFineMesh;
				Identity.Stamp.WorldEpoch = WorldEpoch;
				Identity.Stamp.Section = Admission.FineKey;
				Identity.Stamp.Token = Section->Stamp.Token;
				Identity.Stamp.Revision = Section->CommittedRevision;
				break;
			}
			case EVoxelViewAdmissionKind::VoxelProxy:
				Identity.Kind = bInDataOnly ? EVoxelTaskKind::GenerateVoxelProxy : EVoxelTaskKind::BuildVoxelProxy;
				Geometry = Interest.FindGeometry(Admission.ProxyKey);
				Revision = Hierarchy.GetVoxelProxyRevision(Admission.ProxyKey);
				break;
			case EVoxelViewAdmissionKind::Surface:
				Identity.Kind = bInDataOnly ? EVoxelTaskKind::GenerateSurface : EVoxelTaskKind::BuildSurface;
				Geometry = Interest.FindGeometry(Admission.SurfaceKey);
				Revision = Hierarchy.GetSurfaceRevision(Admission.SurfaceKey);
				break;
			case EVoxelViewAdmissionKind::Macro:
				Identity.Kind = bInDataOnly ? EVoxelTaskKind::GenerateMacro : EVoxelTaskKind::BuildMacro;
				Geometry = Interest.FindGeometry(Admission.MacroKey);
				Revision = Hierarchy.GetMacroRevision(Admission.MacroKey);
				break;
			default:
				continue;
			}
			if (Geometry) Identity.Stamp = MakeRepresentationStamp(*Geometry, Revision);
			else if (Admission.Kind != EVoxelViewAdmissionKind::Fine) continue;
			const bool bReady = bInDataOnly ? IsAdmissionDataReady(Admission, true) : IsAdmissionMeshReady(Admission);
			if (bReady || IsAdmissionTerminalFailure(Admission))
			{
				AdmissionAges[Stage].Remove(Identity);
				continue;
			}
			if (Scheduler.Has(Identity.Stamp, Identity.Kind) || Limits[Kind] <= 0) continue;
			if (!bInDataOnly && Geometry && Module.IsAuthority())
			{
				const EVoxelTaskKind DataKind = Admission.Kind == EVoxelViewAdmissionKind::VoxelProxy ? EVoxelTaskKind::GenerateVoxelProxy :
					Admission.Kind == EVoxelViewAdmissionKind::Surface ? EVoxelTaskKind::GenerateSurface : EVoxelTaskKind::GenerateMacro;
				if (!PreparedData.Contains({DataKind, Identity.Stamp})) continue;
			}
			EVoxelWorkClass WorkClass = EVoxelWorkClass::Visible;
			FVoxelTerrainServicePriority Priority;
			if (!ResolveTaskServicePriority(Identity.Kind, Identity.Stamp, WorkClass, Priority)) continue;
			FAdmissionServiceAge* Age = AdmissionAges[Stage].Find(Identity);
			if (!Age)
			{
				if (NextAdmissionSequence == MAX_uint64 || AdmissionAges[Stage].Num() == MAX_int32)
				{
					RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable);
					UE_LOG(LogTemp, Error, TEXT("CapacityUnachievable: terrain admission age identity"));
					return;
				}
				if (!ReserveServiceAdmissionAges(Stage, AdmissionAges[Stage].Num() + 1))
				{
					if (LastServiceResourceAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked) continue;
					return;
				}
				Age = &AdmissionAges[Stage].Add(Identity, {FPlatformTime::Seconds(), ++NextAdmissionSequence});
			}
			FVoxelTerrainServiceCandidate Candidate;
			Candidate.Index = Candidates.Num();
			Candidate.Priority = Priority;
			Candidate.FirstQueuedSeconds = Age->FirstQueuedSeconds;
			Candidate.Sequence = Age->Sequence;
			Candidate.EstimatedMicroseconds = AdmissionCostEstimates[Stage][Kind] > 0.0 ?
				AdmissionCostEstimates[Stage][Kind] : 25.0;
			Candidate.bRunnable = true;
			Candidates.Add(Candidate);
			Identities.Add(Identity);
			CandidateAdmissions.Add(Index);
		}
	}
	if (Candidates.IsEmpty() || FPlatformTime::Seconds() >= Deadline) return;
	const uint64 BeforeSelection = Resources->ReleaseVersionExcludingOwner(GetServiceControlConsumer());
	if (bServiceSelectionCapacityWaiting[Stage] && !AdmissionSelection[Stage].GetResourceLease() &&
		BeforeSelection <= ServiceSelectionCapacityVersions[Stage])
	{
		RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::CapacityBlocked);
		return;
	}
	FString SelectionError;
	if (!AdmissionSelection[Stage].Prepare(SelectionError))
	{
		const auto Admission = AdmissionSelection[Stage].GetLastResourceAdmission();
		RecordServiceResourceAdmission(Admission);
		bServiceSelectionCapacityWaiting[Stage] = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		ServiceSelectionCapacityVersions[Stage] = BeforeSelection;
		if (!bServiceSelectionCapacityWaiting[Stage]) UE_LOG(LogTemp, Error, TEXT("%s"), *SelectionError);
		return;
	}
	bServiceSelectionCapacityWaiting[Stage] = false;
	RecordServiceResourceAdmission(EVoxelTerrainResourceAdmission::Granted);
	for (int32 Attempt = 0; Attempt < Candidates.Num() && FPlatformTime::Seconds() < Deadline; ++Attempt)
	{
		auto& Frame = Module.GetTerrainFrameContext();
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.MaximumWaitSeconds = Policy.Fine.MaximumBackgroundWaitSeconds;
		Capacity.bAllowOverdueOpportunity = Frame.IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Selected = AdmissionSelection[Stage].Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Selected == INDEX_NONE) break;
		if (FPlatformTime::Seconds() >= Deadline)
		{
			verify(AdmissionSelection[Stage].RefundBeforeStart(Charge.Serial));
			break;
		}
		if (Charge.bOverdueOpportunity) Frame.ConsumeOverdueOpportunity();
		Candidates[Selected].bRunnable = false;
		const auto& Identity = Identities[Selected];
		const FVoxelViewAdmission* Admission = &Admissions[CandidateAdmissions[Selected]];
		const int32 Kind = int32(Admission->Kind);
		const double Started = FPlatformTime::Seconds();
		bool bSubmitted = false;
		if (!bInDataOnly) bSubmitted = TrySubmitAdmission(*Admission);
		else switch (Admission->Kind)
		{
		case EVoxelViewAdmissionKind::VoxelProxy: bSubmitted = RequestVoxelProxy(Admission->ProxyKey, Admission->Priority, true); break;
		case EVoxelViewAdmissionKind::Surface: bSubmitted = RequestSurface(Admission->SurfaceKey, Admission->Priority, true); break;
		case EVoxelViewAdmissionKind::Macro: bSubmitted = RequestMacro(Admission->MacroKey, Admission->Priority, true); break;
		default: break;
		}
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
		verify(AdmissionSelection[Stage].Settle(Charge.Serial, Actual));
		double& Estimate = AdmissionCostEstimates[Stage][Kind];
		Estimate = Estimate > 0.0 ? FMath::Lerp(Estimate, Actual, 0.15) : Actual;
		Submitted[Kind] += bSubmitted ? 1 : 0;
		if (bSubmitted) AdmissionAges[Stage].Remove(Identity);
		if (Submitted[Kind] >= Limits[Kind])
		{
			for (auto& Candidate : Candidates)
			{
				if (Identities[Candidate.Index].Kind == Identity.Kind) Candidate.bRunnable = false;
			}
		}
	}
}


FVoxelTaskStamp FVoxelViewManager::MakeRepresentationStamp(const FVoxelTerrainGeometry& InGeometry, const uint64 InRevision) const
{
	check(IsInGameThread());
	if (const auto* Existing = RepresentationAttempts.Find(InGeometry.Key);
		Existing && Existing->Stamp.WorldEpoch == WorldEpoch && Existing->Stamp.OwnerId == TaskOwnerId &&
		Existing->Stamp.OwnerIncarnation == TaskOwnerIncarnation &&
		Existing->Stamp.bHasGeometryIdentity && Existing->Stamp.GeometryIdentity == InGeometry)
	{
		if (Existing->Stamp.Revision == InRevision) return Existing->Stamp;
		if (!Module.IsAuthority() && Existing->Stamp.Revision > InRevision)
		{
			const EVoxelTaskKind Kind = InGeometry.Key.Representation == EVoxelTerrainRepresentation::VoxelProxy ?
				EVoxelTaskKind::GenerateVoxelProxy : InGeometry.Key.Representation == EVoxelTerrainRepresentation::Surface ?
				EVoxelTaskKind::GenerateSurface : EVoxelTaskKind::GenerateMacro;
			const auto* Pending = PreparedData.Find({Kind, Existing->Stamp});
			if (Pending && *Pending && ((*Pending)->PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::None ||
				(*Pending)->PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked))
				return Existing->Stamp;
		}
	}
	check(NextRepresentationAttempt != MAX_uint64);
	FVoxelTaskStamp Stamp;
	Stamp.OwnerId = TaskOwnerId;
	Stamp.OwnerIncarnation = TaskOwnerIncarnation;
	Stamp.WorldEpoch = WorldEpoch;
	Stamp.Token = ++NextRepresentationAttempt;
	Stamp.Revision = InRevision;
	Stamp.bHasGeometryIdentity = true;
	Stamp.GeometryIdentity = InGeometry;
	switch (InGeometry.Key.Representation)
	{
	case EVoxelTerrainRepresentation::VoxelProxy:
		Stamp.ViewKey = {InGeometry.Key.Coordinate, InGeometry.Key.Level};
		break;
	case EVoxelTerrainRepresentation::Surface:
		Stamp.SurfaceKey = {FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		break;
	case EVoxelTerrainRepresentation::Macro:
		Stamp.MacroKey = {FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		break;
	default:
		checkNoEntry();
	}
	RepresentationAttempts.Add(InGeometry.Key, {Stamp, EVoxelTaskKind::None});
	return Stamp;
}

bool FVoxelViewManager::IsRepresentationStampCurrent(const FVoxelTaskStamp& InStamp) const
{
	if (!InStamp.bHasGeometryIdentity || InStamp.WorldEpoch != WorldEpoch || InStamp.OwnerId != TaskOwnerId ||
		InStamp.OwnerIncarnation != TaskOwnerIncarnation)
	{
		return false;
	}
	const auto* Current = RepresentationAttempts.Find(InStamp.GeometryIdentity.Key);
	return Current && Current->Stamp == InStamp;
}

bool FVoxelViewManager::HasDeferredRepresentation(const FVoxelTaskStamp& InStamp) const
{
	if (!InStamp.bHasGeometryIdentity) return false;
	const auto* Current = RepresentationAttempts.Find(InStamp.GeometryIdentity.Key);
	return Current && Current->Stamp == InStamp && Current->DeferredKind != EVoxelTaskKind::None;
}

bool FVoxelViewManager::IsPreparedDataCurrent(const FVoxelTaskKey& InKey) const
{
	if (!IsRepresentationStampCurrent(InKey.Stamp)) return false;
	const FVoxelChangeHierarchy& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	const auto RevisionCurrent = [this, &InKey](const uint64 Known)
	{
		// 客户端的新回复在 Worker 验证后才推进已知版本；排队期间允许尚未接纳的新版本。
		return Module.IsAuthority() ? Known == InKey.Stamp.Revision : Known <= InKey.Stamp.Revision;
	};
	switch (InKey.Kind)
	{
	case EVoxelTaskKind::GenerateVoxelProxy:
		return GetInputRoot().ProxyWanted.Contains(InKey.Stamp.ViewKey) && RevisionCurrent(Hierarchy.GetVoxelProxyRevision(InKey.Stamp.ViewKey)) &&
			   GetViewInterest().FindGeometry(InKey.Stamp.ViewKey) &&
			   *GetViewInterest().FindGeometry(InKey.Stamp.ViewKey) == InKey.Stamp.GeometryIdentity;
	case EVoxelTaskKind::GenerateSurface:
		return GetInputRoot().SurfaceWanted.Contains(InKey.Stamp.SurfaceKey) && RevisionCurrent(Hierarchy.GetSurfaceRevision(InKey.Stamp.SurfaceKey)) &&
			   GetViewInterest().FindGeometry(InKey.Stamp.SurfaceKey) &&
			   *GetViewInterest().FindGeometry(InKey.Stamp.SurfaceKey) == InKey.Stamp.GeometryIdentity;
	case EVoxelTaskKind::GenerateMacro:
		return GetInputRoot().MacroWanted.Contains(InKey.Stamp.MacroKey) && RevisionCurrent(Hierarchy.GetMacroRevision(InKey.Stamp.MacroKey)) &&
			   GetViewInterest().FindGeometry(InKey.Stamp.MacroKey) &&
			   *GetViewInterest().FindGeometry(InKey.Stamp.MacroKey) == InKey.Stamp.GeometryIdentity;
	default:
		return false;
	}
}

void FVoxelViewManager::PrunePreparedData()
{
	for (auto It = PreparedEnvironmentRetentions.CreateIterator(); It; ++It)
		if (!IsPreparedDataCurrent(It.Key())) It.RemoveCurrent();
	for (auto It = PreparedData.CreateIterator(); It; ++It)
	{
		if (!IsPreparedDataCurrent(It.Key()))
		{
			PreparedDataBytes -= It.Value()->ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
			It.RemoveCurrent();
		}
	}
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return (Kind == EVoxelTaskKind::GenerateVoxelProxy || Kind == EVoxelTaskKind::GenerateSurface ||
			Kind == EVoxelTaskKind::GenerateMacro) && !IsPreparedDataCurrent({ Kind, Stamp });
	});
}

TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> FVoxelViewManager::RetainPreparedEnvironment(
	const FVoxelTaskKey& InKey, const FVoxelGenerationPipeline& InGenerator,
	const FVoxelGenerationBounds& InBounds)
{
	auto& Retention = PreparedEnvironmentRetentions.FindOrAdd(InKey);
	if (!Retention) Retention = RetainEnvironmentData(InGenerator, InBounds);
	return Retention;
}

bool FVoxelViewManager::EnqueuePreparedData(FVoxelTaskRequest&& InRequest)
{
	if (Scheduler.Has(InRequest.Stamp, InRequest.Kind) || HasDeferredRepresentation(InRequest.Stamp)) return false;
	const uint64 Limit = (InRequest.WorkClass == EVoxelWorkClass::Prefetch ? 128ull : 256ull) * 1024ull * 1024ull;
	const uint64 Reservation = InRequest.ReservedBytes + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
	if (PreparedDataBytes + PendingDataBytes + Reservation > Limit)
	{
		Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
			Reservation > Limit ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::Memory,
			InRequest.InputBytes, Reservation);
		return false;
	}
	{
		FString CallbackBindingError;
		if (!InRequest.BindApply(Scheduler.GetTerrainResources(), (InRequest.GenerationConsumer.IsValid() ? InRequest.GenerationConsumer : Scheduler.GetControlConsumer()),
			[this, Reservation](FVoxelTaskResult&& Result)
	{
		PendingDataBytes -= Reservation;
		const FVoxelTaskKey Key { Result.Kind, Result.Stamp };
		if (Result.bCanceled || Result.Error.StartsWith(TEXT("DependencyNotReady")) || !IsPreparedDataCurrent(Key)) return;
		if (Result.OverlayReadResult && !Module.IsOverlayReadCurrent(*Result.OverlayReadResult))
		{
			++TerrainWorkMetrics.Stale;
			return;
		}
		if (!Result.bSuccess)
		{
			Scheduler.ObserveDemand(Result.Kind, Result.Stamp, EVoxelTaskBlockReason::TerminalFailure);
			UE_LOG(LogTemp, Warning, TEXT("Voxel data preparation failed: kind=%d error=%s"),
				static_cast<int32>(Result.Kind), *Result.Error);
			return;
		}
		const auto& Interest = GetViewInterest();
		const auto* Geometry = Result.VoxelProxy ? Interest.FindGeometry(Result.VoxelProxy->Key) :
			Result.Surface ? Interest.FindGeometry(Result.Surface->Key) :
			Result.Macro ? Interest.FindGeometry(Result.Macro->Key) : nullptr;
		if (!Geometry || (Result.VoxelProxy && *Geometry != Result.VoxelProxy->Geometry) ||
			(Result.Surface && *Geometry != Result.Surface->Geometry) || (Result.Macro && *Geometry != Result.Macro->Geometry)) return;
		RemovePreparedData(Key);
		Result.PreparedRevisionAdmission = Module.IsAuthority() ? EVoxelTerrainResourceAdmission::Granted : EVoxelTerrainResourceAdmission::None;
		TSharedPtr<FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
		if (Result.InputResourceLease)
		{
			const auto Metadata = Result.InputResourceLease;
			Prepared = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTaskResult>(Metadata, MoveTemp(Result));
		}
#if WITH_DEV_AUTOMATION_TESTS
		else Prepared = MakeShared<FVoxelTaskResult, ESPMode::ThreadSafe>(MoveTemp(Result));
#endif
		if (!Prepared)
		{
			Scheduler.ObserveDemand(Key.Kind, Key.Stamp, EVoxelTaskBlockReason::TerminalFailure);
			return;
		}
		PreparedDataBytes += Prepared->ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
		PreparedData.Add(Key, Prepared);
		if (!TryAcceptPreparedRevision(Key, *Prepared)) return;
		if (!ShouldDeferCoverage(VoxelProxyCoverageRect(Prepared->Stamp.ViewKey)) && Prepared->bKnownEmptyMesh && Prepared->VoxelProxy && !VoxelProxyActors.Contains(Prepared->Stamp.ViewKey))
		{
			if (!Prepared->InputResourceLease) return;
			FVoxelTaskResult Empty;
			Empty.Kind = Prepared->Kind;
			Empty.Stamp = Prepared->Stamp;
			Empty.OverlayReadResult = Prepared->OverlayReadResult;
			Empty.VoxelProxy = Prepared->VoxelProxy;
			Empty.VoxelProxyMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(Prepared->InputResourceLease);
			Empty.VoxelProxyMesh->ResourceLease = Prepared->InputResourceLease;
			Empty.VoxelProxyMesh->FrozenVoxelProxy = Prepared->VoxelProxy;
			Empty.VoxelProxyMesh->ValidateAndCache();
			Empty.VoxelProxyMesh->InputWorkReceipt = Prepared->WorkReceipt;
			if (PublishVoxelProxy(Empty))
			{
				++SkippedProxyMeshes;
				MarkCoverageDirty(VoxelProxyCoverageRect(Prepared->Stamp.ViewKey));
				RemovePreparedData(Key);
				return;
			}
		}
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}
	const FVoxelTaskKey RetentionKey{InRequest.Kind, InRequest.Stamp};
	if (!EnqueueViewTask(MoveTemp(InRequest)))
		return false;
	PreparedEnvironmentRetentions.Remove(RetentionKey);
	PendingDataBytes += Reservation;
	return true;
}

void FVoxelViewManager::RemovePreparedData(const FVoxelTaskKey& InKey)
{
	if (const auto* Data = PreparedData.Find(InKey))
	{
		PreparedDataBytes -= (*Data)->ResultBytes() + sizeof(FVoxelTaskResult) + sizeof(FVoxelTaskKey);
		PreparedData.Remove(InKey);
	}
}

bool FVoxelViewManager::TryAcceptPreparedRevision(const FVoxelTaskKey& InKey, FVoxelTaskResult& InOutResult)
{
	if (!IsPreparedDataCurrent(InKey)) return false;
	if (InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::Granted) return true;
	if (InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
		InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::InvalidRequest) return false;
	auto& Runtime = *Module.GetRuntime();
	auto& Hierarchy = Runtime.GetChangeHierarchy();
	const auto Resources = Runtime.GetTerrainResources();
	const auto Consumer = Hierarchy.GetControlConsumer();
	if (Consumer.IsValid() && Consumer.WorldEpoch != InKey.Stamp.WorldEpoch) return false;
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(Consumer, BeforeAdmission)) return false;
	if (InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
		BeforeAdmission <= InOutResult.PreparedRevisionCapacityVersion) return false;
	FString Error;
	bool bAccepted = false;
	FVoxelGenerationRequiredSourceScope ResultSource(&Resources.Get(), InOutResult.InputResourceLease.Get());
	switch (InKey.Kind)
	{
	case EVoxelTaskKind::GenerateVoxelProxy:
		bAccepted = Hierarchy.GetVoxelProxyRevision(InKey.Stamp.ViewKey) == InKey.Stamp.Revision ||
			Hierarchy.SetVoxelProxyRevision(InKey.Stamp.ViewKey, InKey.Stamp.Revision, Error);
		break;
	case EVoxelTaskKind::GenerateSurface:
		bAccepted = Hierarchy.GetSurfaceRevision(InKey.Stamp.SurfaceKey) == InKey.Stamp.Revision ||
			Hierarchy.SetSurfaceRevision(InKey.Stamp.SurfaceKey, InKey.Stamp.Revision, Error);
		break;
	case EVoxelTaskKind::GenerateMacro:
		bAccepted = Hierarchy.GetMacroRevision(InKey.Stamp.MacroKey) == InKey.Stamp.Revision ||
			Hierarchy.SetMacroRevision(InKey.Stamp.MacroKey, InKey.Stamp.Revision, Error);
		break;
	default:
		InOutResult.PreparedRevisionAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	InOutResult.PreparedRevisionAdmission = bAccepted ? EVoxelTerrainResourceAdmission::Granted : Hierarchy.GetLastResourceAdmission();
	InOutResult.PreparedRevisionCapacityVersion = BeforeAdmission;
	if (!bAccepted)
	{
		Scheduler.ObserveDemand(InKey.Kind, InKey.Stamp,
			InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? EVoxelTaskBlockReason::InputMemory :
			InOutResult.PreparedRevisionAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? EVoxelTaskBlockReason::Oversized :
			EVoxelTaskBlockReason::TerminalFailure);
	}
	return bAccepted;
}

TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> FVoxelViewManager::FindPreparedData(const FVoxelTaskKey& InKey)
{
	const auto* Data = PreparedData.Find(InKey);
	if (!Data || !*Data || !TryAcceptPreparedRevision(InKey, **Data)) return nullptr;
	return *Data;
}

int32 FVoxelViewManager::VoxelProxyTerrainStage(const FVoxelViewKey& InKey) const
{
	// Fine 交接依赖的粗体素属于同一阶段，避免 Fine 等待被自身阶段门禁挡住的依赖。
	return GetViewInterest().TerrainPlan.FineDependencies.Contains(InKey) ? 0 : 1;
}

int32 FVoxelViewManager::AdmissionTerrainStage(const FVoxelViewAdmission& InAdmission) const
{
	return InAdmission.Kind == EVoxelViewAdmissionKind::VoxelProxy
		? VoxelProxyTerrainStage(InAdmission.ProxyKey) : static_cast<int32>(InAdmission.Kind);
}

EVoxelWorkClass FVoxelViewManager::VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const
{
	const auto Context = DesiredVolumeContexts.FindRef(InOwner);
	if (Context && Context->Patches.ContainsByPredicate(
					   [this](const FVoxelBoundaryTransitionPatch& Patch)
					   {
						   const auto* Demand = Patch.Face.Neighbor.Level == 0 ? GetViewInterest().Exact.Find(Patch.Face.Neighbor.Coordinate) : nullptr;
						   return Demand && (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalFine ||
												Demand->bMovementCriticalCollision || Demand->bMovementCriticalData);
					   }))
		return EVoxelWorkClass::Critical;
	return VoxelProxyTerrainStage(InOwner) == 0 || (Context && Context->Patches.ContainsByPredicate([](const FVoxelBoundaryTransitionPatch& Patch)
	{
		return Patch.Face.Neighbor.Level == 0;
	})) ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;
}

bool FVoxelViewManager::ResolveTaskServicePriority(const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
	EVoxelWorkClass& InOutWorkClass, FVoxelTerrainServicePriority& OutPriority) const
{
	const auto& Latest = Module.GetCurrentInterest();
	const FVoxelTerrainServicePriority* Priority = nullptr;
	FVoxelTerrainSpatialKey Spatial;
	switch (InKind)
	{
	case EVoxelTaskKind::BuildFineShard:
		if (InStamp.bHasGeometryIdentity)
		{
			const auto& Geometry = InStamp.GeometryIdentity;
			if (Geometry.Key.Level == 0)
			{
				if (const auto* Demand = Latest.Exact.Find(Geometry.Key.Coordinate); Demand && Demand->bFineRender)
				{
					Priority = &Demand->ServicePriority;
					InOutWorkClass = FVoxelTerrainWorkSelection::IsNear(Priority->Band) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
				}
			}
			else
			{
				const int32 Side = int32(Geometry.Key.SideCells());
				if (const auto* Demand = Latest.FineShardDemands.Find({Geometry.Key.Coordinate * Side, Side}))
				{
					Priority = &Demand->ServicePriority;
					InOutWorkClass = FVoxelTerrainWorkSelection::IsNear(Priority->Band) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
				}
			}
		}
		break;
	case EVoxelTaskKind::BuildFineMesh:
		if (InStamp.Section == FIntVector(MIN_int32, 1, 0))
		{
			const FVoxelFinePageKey Page{InStamp.ViewKey.Coordinate, InStamp.ViewKey.Level};
			Priority = Latest.FinePageServicePriorities.Find(Page);
			InOutWorkClass = Latest.CriticalFinePages.Contains(Page) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
		}
		else if (const auto* Demand = Latest.Exact.Find(InStamp.Section))
		{
			Priority = &Demand->ServicePriority;
			InOutWorkClass = FVoxelTerrainWorkSelection::IsNear(Priority->Band) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
		}
		break;
	case EVoxelTaskKind::BuildVoxelProxy:
	case EVoxelTaskKind::GenerateVoxelProxy:
	case EVoxelTaskKind::BuildVolumeTransition:
		Spatial = {EVoxelTerrainRepresentation::VoxelProxy, InStamp.ViewKey.Coordinate, InStamp.ViewKey.Level};
		Priority = Latest.GeometryServicePriorities.Find(Spatial);
		InOutWorkClass = InKind == EVoxelTaskKind::GenerateVoxelProxy ? EVoxelWorkClass::Prefetch : EVoxelWorkClass::Boundary;
		break;
	case EVoxelTaskKind::BuildSurface:
	case EVoxelTaskKind::GenerateSurface:
		Spatial = {EVoxelTerrainRepresentation::Surface, FIntVector(InStamp.SurfaceKey.Coordinate.X, InStamp.SurfaceKey.Coordinate.Y, 0), InStamp.SurfaceKey.Level};
		Priority = Latest.GeometryServicePriorities.Find(Spatial);
		InOutWorkClass = InKind == EVoxelTaskKind::GenerateSurface ? EVoxelWorkClass::Prefetch : EVoxelWorkClass::Exploration;
		break;
	case EVoxelTaskKind::BuildMacro:
	case EVoxelTaskKind::GenerateMacro:
		Spatial = {EVoxelTerrainRepresentation::Macro, FIntVector(InStamp.MacroKey.Coordinate.X, InStamp.MacroKey.Coordinate.Y, 0), InStamp.MacroKey.Level};
		Priority = Latest.GeometryServicePriorities.Find(Spatial);
		InOutWorkClass = InKind == EVoxelTaskKind::GenerateMacro ? EVoxelWorkClass::Prefetch : EVoxelWorkClass::Background;
		break;
	case EVoxelTaskKind::BuildViewTransition:
	case EVoxelTaskKind::BuildViewCoverage:
		if (InStamp.Section.Z != -1)
		{
			Spatial = {static_cast<EVoxelHeightfieldRepresentation>(InStamp.Section.Z) == EVoxelHeightfieldRepresentation::Surface
				? EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro,
				FIntVector(InStamp.Section.X, InStamp.Section.Y, 0), InStamp.ViewKey.Level};
			Priority = Latest.GeometryServicePriorities.Find(Spatial);
		}
		break;
	default:
		return false;
	}
	if (!Priority)
	{
		return false;
	}
	OutPriority = *Priority;
	if (FVoxelTerrainWorkSelection::IsNear(OutPriority.Band))
	{
		InOutWorkClass = EVoxelWorkClass::Critical;
	}
	return true;
}

void FVoxelViewManager::UpdateTaskPriorities()
{
	Scheduler.UpdateServicePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
	{
		ResolveTaskServicePriority(Kind, Stamp, WorkClass, Priority);
	});

	Scheduler.UpdateTerrainStages(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp, int32& Stage)
		{
			switch (Kind)
			{
			case EVoxelTaskKind::BuildVoxelProxy:
				Stage = IsCriticalDependency({EVoxelTerrainRepresentation::VoxelProxy, Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level})
							? 0
							: VoxelProxyTerrainStage(Stamp.ViewKey);
				break;
			case EVoxelTaskKind::BuildSurface:
				Stage = IsCriticalDependency({EVoxelTerrainRepresentation::Surface, FIntVector(Stamp.SurfaceKey.Coordinate.X, Stamp.SurfaceKey.Coordinate.Y, 0),
							Stamp.SurfaceKey.Level})
							? 0
							: 2;
				break;
			case EVoxelTaskKind::BuildMacro:
				Stage = IsCriticalDependency(
							{EVoxelTerrainRepresentation::Macro, FIntVector(Stamp.MacroKey.Coordinate.X, Stamp.MacroKey.Coordinate.Y, 0), Stamp.MacroKey.Level})
							? 0
							: 3;
				break;
			default:
				break;
			}
		});
}

void FVoxelViewManager::ProcessAdmissions()
{
	ProcessServiceAdmissions(false);
}

double FVoxelViewManager::MinimumObserverDistanceCells(const FVector& InWorldCenter) const
{
	double BestSquared = TNumericLimits<double>::Max();
	for (const FVector& Observer : PriorityObservers)
	{
		BestSquared = FMath::Min(BestSquared, FVector::DistSquared(InWorldCenter, Observer));
	}
	return PriorityObservers.IsEmpty() ? 0.0 : FMath::Sqrt(BestSquared) / FMath::Max(1.0, Module.BlockSize());
}

FVector FVoxelViewManager::SurfaceWorldCenter(const FVoxelSurfaceTileKey& InKey) const
{
	const FVoxelCoverageRect Bounds = SurfaceCoverageRect(InKey);
	return FVector(FVector2D(Bounds.Min + Bounds.Max) * (0.5 * Module.BlockSize()),
		PriorityObservers.IsEmpty() ? 0.0 : PriorityObservers[0].Z);
}

double FVoxelViewManager::MinimumObserverDistanceCells(const FVoxelGenerationBounds& InCellBounds) const
{
	const FBox Bounds(FVector(InCellBounds.Min), FVector(InCellBounds.Max));
	double BestSquared = MAX_dbl;
	for (const FVector& Observer : PriorityObservers)
	{
		BestSquared = FMath::Min(BestSquared, Bounds.ComputeSquaredDistanceToPoint(Observer / FMath::Max(1.0, Module.BlockSize())));
	}
	return PriorityObservers.IsEmpty() ? 0.0 : FMath::Sqrt(BestSquared);
}

FVector FVoxelViewManager::MacroWorldCenter(const FVoxelMacroTileKey& InKey) const
{
	const FVoxelCoverageRect Bounds = MacroCoverageRect(InKey);
	return FVector(FVector2D(Bounds.Min + Bounds.Max) * (0.5 * Module.BlockSize()),
		PriorityObservers.IsEmpty() ? 0.0 : PriorityObservers[0].Z);
}

void FVoxelViewManager::RebuildAdmissions(TConstArrayView<FVector> InObservers)
{
	Admissions = GetViewInterest().Admissions;
	AdmissionPriorities = GetViewInterest().AdmissionPriorities;
}

void FVoxelViewManager::TrackReadyTerrainNode(FVoxelViewKey InKey)
{
	if (bReadyTerrainBranchesDirty) return;
	while (InKey.Level < 24)
	{
		bool bAlreadyPresent = false;
		ReadyTerrainBranches.Add(InKey, &bAlreadyPresent);
		if (bAlreadyPresent) break;
		InKey = InKey.GetParent();
	}
}

void FVoxelViewManager::RebuildReadyTerrainBranches()
{
	ReadyTerrainBranches.Reset();
	bReadyTerrainBranchesDirty = false;
	for (const FIntVector& Key : FineReady) TrackReadyTerrainNode({ Key, 0 });
	for (const FVoxelViewKey& Key : VoxelProxyReady) TrackReadyTerrainNode(Key);
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelViewManager::SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions)

{
	InOutAdmissions.Sort([](const FVoxelViewAdmission& A, const FVoxelViewAdmission& B)
	{
		if (A.Priority != B.Priority) return A.Priority < B.Priority;
		return A.DistanceCells != B.DistanceCells ? A.DistanceCells < B.DistanceCells :
			static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
	});
}

double FVoxelViewManager::ResolveAdmissionFrontier(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
	const double InBandWidthCells)
{
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (!InIsReady(Admission))
		{
			return Admission.DistanceCells + InBandWidthCells;
		}
	}
	return TNumericLimits<double>::Max();
}

#endif

bool FVoxelViewManager::IsAdmissionMeshReady(const FVoxelViewAdmission& A) const
{
	switch (A.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		return IsFineGeometryPrepared(A.FineKey);
	case EVoxelViewAdmissionKind::VoxelProxy:
		{
		const auto Data = VoxelProxyData.FindRef(A.ProxyKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.ProxyKey);
		return VoxelProxyReady.Contains(A.ProxyKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	case EVoxelViewAdmissionKind::Surface:
		{
		const auto Data = SurfaceData.FindRef(A.SurfaceKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.SurfaceKey);
		return SurfaceReady.Contains(A.SurfaceKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	case EVoxelViewAdmissionKind::Macro:
		{
		const auto Data = MacroData.FindRef(A.MacroKey);
		const auto* Geometry = GetViewInterest().FindGeometry(A.MacroKey);
		return MacroReady.Contains(A.MacroKey) && Data && Geometry && Data->Geometry == *Geometry;
		}
	default:
		return true;
	}
}

bool FVoxelViewManager::IsAdmissionPresented(const FVoxelViewAdmission& InAdmission) const
{
	if (!IsAdmissionMeshReady(InAdmission)) return false;
	AActor* Actor = nullptr;
	switch (InAdmission.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		if (!VisibleTerrainNodes.Contains({InAdmission.FineKey, 0})) return false;
		return IsFinePresented(InAdmission.FineKey);
	case EVoxelViewAdmissionKind::VoxelProxy:
		if (!VisibleTerrainNodes.Contains(InAdmission.ProxyKey)) return false;
		Actor = VoxelProxyActors.FindRef(InAdmission.ProxyKey);
		break;
	case EVoxelViewAdmissionKind::Surface:
		Actor = SurfaceActors.FindRef(InAdmission.SurfaceKey);
		break;
	case EVoxelViewAdmissionKind::Macro:
		Actor = MacroActors.FindRef(InAdmission.MacroKey);
		break;
	default:
		return true;
	}
	return Actor && Publisher->IsCommitted(Actor);
}

bool FVoxelViewManager::IsTerrainGeometryPresented(const FVoxelTerrainGeometry& InGeometry) const
{
	const auto* Current = GetViewInterest().Geometry.Find(InGeometry.Key);
	if (!Current || *Current != InGeometry) return false;
	FVoxelViewAdmission Admission;
	switch (InGeometry.Key.Representation)
	{
	case EVoxelTerrainRepresentation::VoxelProxy:
		Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
		Admission.ProxyKey = {InGeometry.Key.Coordinate, InGeometry.Key.Level};
		break;
	case EVoxelTerrainRepresentation::Surface:
		Admission.Kind = EVoxelViewAdmissionKind::Surface;
		Admission.SurfaceKey = {FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		break;
	case EVoxelTerrainRepresentation::Macro:
		Admission.Kind = EVoxelViewAdmissionKind::Macro;
		Admission.MacroKey = {FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		break;
	default:
		return false;
	}
	return IsAdmissionPresented(Admission);
}

void FVoxelViewManager::RefreshTerrainBuildStage()
{
	if (!bTerrainBuildStageDirty) return;
	bTerrainBuildStageDirty = false;
	int32 Stage = 3;
	const auto Presented = CapturePresentedSnapshot();
	for (const FVoxelViewAdmission& Admission : GetViewInterest().Admissions)
	{
		const int32 Kind = static_cast<uint8>(Admission.Kind);
		// Fine 保持可玩呈现门禁；粗地形按网格就绪推进构建，呈现交接由发布分组保证。
		// 等待所有 Proxy 呈现会阻止其退役所需的 Surface 构建，形成跨表示循环依赖。
		bool bReady = IsAdmissionMeshReady(Admission);
		if (Admission.Kind == EVoxelViewAdmissionKind::Fine)
		{
			bReady = IsAdmissionPresented(Admission);
			if (!bReady && IsFineGeometryPrepared(Admission.FineKey) && Presented && Presented->GetWorldEpoch() == WorldEpoch)
			{
				const auto* Evidence = Presented->Find(Admission.FineKey);
				bReady = Evidence && Evidence->Content.SampleStepCells == 1 && Evidence->GeometryLease &&
					(Evidence->Owner.Kind == EVoxelFineOwnerKind::F0 || Evidence->Owner.Kind == EVoxelFineOwnerKind::F1Plus);
			}
		}
		if (Kind < Stage && !bReady) Stage = Kind;
		if (Stage == 0) break;
	}
	if (Stage != Scheduler.GetObservedTerrainStage())
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel terrain mesh stage: %d -> %d interest=%llu"),
			Scheduler.GetObservedTerrainStage(), Stage, Module.GetInterestRevision());
		Scheduler.SetObservedTerrainStage(Stage);
	}
}



bool FVoxelViewManager::IsAdmissionTerminalFailure(const FVoxelViewAdmission& A) const
{
	if (A.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const auto* Section = Module.GetRuntime()->FindSection(A.FineKey);
		const auto* Slot = FineShardSlots.Find({A.FineKey * 16, 16});
		return (Section && Section->Status == EVoxelSectionStatus::Failed) || (Slot && !Slot->Build.GetFailure().IsEmpty());
	}
	FVoxelTaskStamp Stamp;
	Stamp.WorldEpoch = WorldEpoch;
	const auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	EVoxelTaskKind DataKind, MeshKind;
	const FVoxelTerrainGeometry* Geometry = nullptr;
	if (A.Kind == EVoxelViewAdmissionKind::VoxelProxy)
	{
		Stamp.ViewKey = A.ProxyKey;
		Stamp.Revision = Hierarchy.GetVoxelProxyRevision(A.ProxyKey);
		Geometry = GetViewInterest().FindGeometry(A.ProxyKey);
		DataKind = EVoxelTaskKind::GenerateVoxelProxy; MeshKind = EVoxelTaskKind::BuildVoxelProxy;
	}
	else if (A.Kind == EVoxelViewAdmissionKind::Surface)
	{
		Stamp.SurfaceKey = A.SurfaceKey;
		Stamp.Revision = Hierarchy.GetSurfaceRevision(A.SurfaceKey);
		Geometry = GetViewInterest().FindGeometry(A.SurfaceKey);
		DataKind = EVoxelTaskKind::GenerateSurface; MeshKind = EVoxelTaskKind::BuildSurface;
	}
	else
	{
		Stamp.MacroKey = A.MacroKey;
		Stamp.Revision = Hierarchy.GetMacroRevision(A.MacroKey);
		Geometry = GetViewInterest().FindGeometry(A.MacroKey);
		DataKind = EVoxelTaskKind::GenerateMacro; MeshKind = EVoxelTaskKind::BuildMacro;
	}
	if (!Geometry) return false; Stamp = MakeRepresentationStamp(*Geometry, Stamp.Revision);
	return Scheduler.IsTerminal({DataKind, Stamp}) || Scheduler.IsTerminal({MeshKind, Stamp});
}

void FVoxelViewManager::RefreshTaskPriorities()
{
	if (GetViewInterest().PublicationInputs)
	{
		bTaskPrioritiesDirty = true;
		bTerrainBuildStageDirty = true;
		return;
	}
	// Standalone fixtures may build a Fine plan without an accepted publication snapshot.
	// Source movement can change presentation bands without changing logical demand.
	RebuildFinePagePlan();
	bTerrainBuildStageDirty = true;
	RefreshTerrainBuildStage();
	RebuildAdmissions(PriorityObservers);
	UpdateTaskPriorities();
}

#if WITH_DEV_AUTOMATION_TESTS
TMap<int32, uint8> FVoxelViewManager::ResolveAdmissionLanes(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady)
{
	TMap<int32, uint8> Lanes;
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (InIsReady(Admission)) continue;
		Lanes.FindOrAdd(Admission.Priority) |= 1 << static_cast<uint8>(Admission.Kind);
	}
	return Lanes;
}

int32 FVoxelViewManager::ResolveActiveAdmissionKind(
	const TConstArrayView<FVoxelViewAdmission> InAdmissions,
	const TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
	int32* OutPriority)
{
	int32 ActiveKind = 4;
	int32 ActivePriority = MAX_int32;
	for (const FVoxelViewAdmission& Admission : InAdmissions)
	{
		if (!InIsReady(Admission))
		{
			if (Admission.Priority < ActivePriority)
			{
				ActivePriority = Admission.Priority;
				ActiveKind = static_cast<int32>(Admission.Kind);
			}
			else if (Admission.Priority == ActivePriority)
			{
				ActiveKind = FMath::Min(ActiveKind, static_cast<int32>(Admission.Kind));
			}
		}
	}
	if (OutPriority) *OutPriority = ActivePriority;
	return ActiveKind;
}

#endif

bool FVoxelViewManager::TrySubmitAdmission(const FVoxelViewAdmission& A)
{
	switch (A.Kind)
	{
	case EVoxelViewAdmissionKind::Fine:
		if (const FVoxelSection* Section = Module.GetRuntime()->FindSection(A.FineKey);
			Section && Section->Status == EVoxelSectionStatus::DataReady)
		{
			const uint64* Revision = FineRevisions.Find(A.FineKey);
			if (IsFineGeometryPrepared(A.FineKey) && Revision && *Revision == Section->CommittedRevision)
			{
				return false;
			}
			return RequestFine(A.FineKey, Section->CommittedRevision);
		}
		return false;
	case EVoxelViewAdmissionKind::VoxelProxy:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestVoxelProxy(A.ProxyKey, A.Priority);
	case EVoxelViewAdmissionKind::Surface:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestSurface(A.SurfaceKey, A.Priority);
	case EVoxelViewAdmissionKind::Macro:
		if (IsAdmissionMeshReady(A)) return false;
		return RequestMacro(A.MacroKey, A.Priority);
	default: return false;
	}
}

void FVoxelViewManager::CancelStaleViewTasks()
{
	PrunePreparedData();
	Scheduler.CancelMatching([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		switch (Kind)
		{
		case EVoxelTaskKind::BuildFineMesh:
			if (Stamp.Section.X == MIN_int32 && Stamp.Section.Y == 1) return !GetInputRoot().FinePagePlan.Pages.Contains({Stamp.ViewKey.Coordinate, Stamp.ViewKey.Level});
			return !GetInputRoot().FineWanted.Contains(Stamp.Section);
		case EVoxelTaskKind::BuildVoxelProxy:
			return !GetInputRoot().ProxyWanted.Contains(Stamp.ViewKey) || !GetViewInterest().FindGeometry(Stamp.ViewKey) ||
				   (!Stamp.bHasGeometryIdentity || *GetViewInterest().FindGeometry(Stamp.ViewKey) != Stamp.GeometryIdentity);
		case EVoxelTaskKind::BuildSurface:
		case EVoxelTaskKind::BuildWater:
			return !GetInputRoot().SurfaceWanted.Contains(Stamp.SurfaceKey) || !GetViewInterest().FindGeometry(Stamp.SurfaceKey) ||
				   (!Stamp.bHasGeometryIdentity || *GetViewInterest().FindGeometry(Stamp.SurfaceKey) != Stamp.GeometryIdentity);
		case EVoxelTaskKind::BuildMacro:
			return !GetInputRoot().MacroWanted.Contains(Stamp.MacroKey) || !GetViewInterest().FindGeometry(Stamp.MacroKey) ||
				   (!Stamp.bHasGeometryIdentity || *GetViewInterest().FindGeometry(Stamp.MacroKey) != Stamp.GeometryIdentity);
		default: return false;
		}
	});
}

void FVoxelViewManager::LogRepresentationState(const TConstArrayView<FVector> InObservers)
{
	const auto Statistics = Scheduler.GetTerrainStatistics();
	const auto Presented = CapturePresentedSnapshot();
	const auto GeometryResources = Scheduler.GetTerrainResources()->Snapshot();
	const auto FactResources = Scheduler.GetRuntimeResources()->Snapshot();
	if (Statistics)
	{
		const auto Work = Statistics->GetWorkAccounting();
		UE_LOG(LogTemp, Display, TEXT("Voxel work accounting: epoch=%llu completed=%llu used=%llu discarded=%llu unresolved=%llu executeMs=%.3f usedMs=%.3f discardedMs=%.3f unresolvedMs=%.3f effectiveRatio=%.4f"),
			WorldEpoch, Work.Completed, Work.Used, Work.Discarded, Work.Unresolved, Work.CompletedMilliseconds,
			Work.UsedMilliseconds, Work.DiscardedMilliseconds, Work.UnresolvedMilliseconds, Work.EffectiveRatio());
	}
	UE_LOG(LogTemp, Display, TEXT("Voxel actual RT coverage: serial=%llu F0Atoms=%d F1PlusAtoms=%d ProxyKeys=%d SurfaceKeys=%d MacroKeys=%d metricsInputRevision=%llu phase=%d cursor=%d"),
		Presented ? Presented->GetSerial() : 0, Presented ? Presented->GetOwnerAtomCount(EVoxelFineOwnerKind::F0) : 0,
		Presented ? Presented->GetOwnerAtomCount(EVoxelFineOwnerKind::F1Plus) : 0,
		Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Proxy) : 0,
		Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Surface) : 0,
		Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Macro) : 0,
		StatisticsInputRevision, StatisticsInputPhase, StatisticsInputCursor);
	UE_LOG(LogTemp, Display, TEXT("Voxel resource grants: Geometry Input=%llu Working=%llu Staged=%llu Resident=%llu Overlap=%llu Facts Input=%llu Working=%llu Staged=%llu Resident=%llu Overlap=%llu"),
		GeometryResources.Used[int32(EVoxelTerrainResourcePool::Input)], GeometryResources.Used[int32(EVoxelTerrainResourcePool::Working)], GeometryResources.Used[int32(EVoxelTerrainResourcePool::Staged)], GeometryResources.Used[int32(EVoxelTerrainResourcePool::Resident)], GeometryResources.Used[int32(EVoxelTerrainResourcePool::ResidentOverlap)],
		FactResources.Used[int32(EVoxelTerrainResourcePool::Input)], FactResources.Used[int32(EVoxelTerrainResourcePool::Working)], FactResources.Used[int32(EVoxelTerrainResourcePool::Staged)], FactResources.Used[int32(EVoxelTerrainResourcePool::Resident)], FactResources.Used[int32(EVoxelTerrainResourcePool::ResidentOverlap)]);
}
void FVoxelViewManager::UpdateWantedTimestamps(
	const double InNow)
{
	auto Refresh = [InNow](auto& Times, const auto& Wanted)
	{
		for (auto& Pair : Times)
		{
			if (Wanted.Contains(Pair.Key)) Pair.Value = InNow;
		}
	};
	Refresh(FineLastWanted, GetInputRoot().FineWanted);
	Refresh(VoxelProxyLastWanted, GetInputRoot().ProxyWanted);
	Refresh(SurfaceLastWanted, GetInputRoot().SurfaceWanted);
	Refresh(MacroLastWanted, GetInputRoot().MacroWanted);
}

FVoxelCoverageBox FVoxelViewManager::FineCoverageBox(const FIntVector& InKey) const
{
	const FIntVector Min = InKey * ViewSectionSide;
	return { Min, Min + FIntVector(ViewSectionSide) };
}

FVoxelCoverageBox FVoxelViewManager::VoxelProxyCoverageBox(const FVoxelViewKey& InKey) const
{
	const FVoxelGenerationBounds Bounds = InKey.GetBounds();
	return { Bounds.Min, Bounds.Max };
}

FVoxelCoverageRect FVoxelViewManager::FineCoverageRect(const FIntVector& InKey) const
{
	return FineCoverageBox(InKey).ProjectXY();
}

FVoxelCoverageRect FVoxelViewManager::VoxelProxyCoverageRect(const FVoxelViewKey& InKey) const
{
	return VoxelProxyCoverageBox(InKey).ProjectXY();
}

FVoxelCoverageRect FVoxelViewManager::SurfaceCoverageRect(const FVoxelSurfaceTileKey& InKey) const
{
	const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

FVoxelCoverageRect FVoxelViewManager::MacroCoverageRect(const FVoxelMacroTileKey& InKey) const
{
	const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	return { Min, Min + FIntPoint(Side, Side) };
}

void FVoxelViewManager::GatherReadyWantedFineBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(FineReady.Num());
	for (const FIntVector& Key : FineReady)
	{
		if (GetInputRoot().FineWanted.Contains(Key) && VisibleTerrainNodes.Contains({Key, 0}) &&
			IsFinePresented(Key)) OutCoverage.Add(FineCoverageBox(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedProxyBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(VoxelProxyReady.Num());
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (GetInputRoot().ProxyWanted.Contains(Key) && VisibleTerrainNodes.Contains(Key) &&
			(!Actor || (!Actor->IsHidden() && Publisher->IsCommitted(Actor)))) OutCoverage.Add(VoxelProxyCoverageBox(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedSurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(SurfaceReady.Num());
	for (const FVoxelSurfaceTileKey& Key : SurfaceReady)
	{
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (GetInputRoot().SurfaceWanted.Contains(Key) && Actor && !Actor->IsHidden() &&
			Publisher->IsCommitted(Actor)) OutCoverage.Add(SurfaceCoverageRect(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedMacroRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(MacroReady.Num());
	for (const FVoxelMacroTileKey& Key : MacroReady)
	{
		AActor* Actor = MacroActors.FindRef(Key);
		if (GetInputRoot().MacroWanted.Contains(Key) && Actor && !Actor->IsHidden() &&
			Publisher->IsCommitted(Actor)) OutCoverage.Add(MacroCoverageRect(Key));
	}
}

void FVoxelViewManager::GatherReadyWantedProxySurfaceRects(
	const FVoxelCoverageRect& InTarget,
	TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	for (const FVoxelViewKey& Key : VoxelProxyReady)
	{
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (!GetInputRoot().ProxyWanted.Contains(Key) || !VisibleTerrainNodes.Contains(Key) ||
			!Actor || Actor->IsHidden() || !Publisher->IsCommitted(Actor) || !VoxelProxyCoverageRect(Key).Intersects(InTarget))
		{
			continue;
		}
		if (const auto Coverage = ProxySurfaceCoverage.FindRef(Key))
		{
			for (const FVoxelCoverageRect& Rect : Coverage->TopSurfaceCoverage)
			{
				if (Rect.Intersects(InTarget))
				{
					OutCoverage.Add(Rect);
				}
			}
		}
	}
}

void FVoxelViewManager::UpdateProxySurfaceCoverage(TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> InData)
{
	bHeightfieldTopologyDirty = true;
	check(InData && InData->CapturedSurfaceRows == InData->GridSide);
	const auto Key = InData->Key;
	ProxySurfaceCoverage.Add(Key, MoveTemp(InData));
}

void FVoxelViewManager::GatherCurrentRenderDomainRects(TArray<FVoxelCoverageRect>& OutCoverage) const
{
	OutCoverage.Reset();
	OutCoverage.Reserve(GetInputRoot().FineWanted.Num() + GetInputRoot().ProxyWanted.Num() + GetInputRoot().SurfaceWanted.Num() + GetInputRoot().MacroWanted.Num());
	for (const FIntVector& Key : GetInputRoot().FineWanted) OutCoverage.Add(FineCoverageRect(Key));
	for (const FVoxelViewKey& Key : GetInputRoot().ProxyWanted) OutCoverage.Add(VoxelProxyCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : GetInputRoot().SurfaceWanted) OutCoverage.Add(SurfaceCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : GetInputRoot().MacroWanted) OutCoverage.Add(MacroCoverageRect(Key));
}

bool FVoxelViewManager::IsFineReplacementReady(const FIntVector& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsFineReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsProxyReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsSurfaceReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsMacroReplacementReady(const FVoxelMacroTileKey& InKey) const
{
	FCoveragePassSnapshot Snapshot;
	BuildCoveragePassSnapshot(Snapshot);
	return IsMacroReplacementReady(InKey, Snapshot);
}

bool FVoxelViewManager::IsFineInsideRenderDomain(const FIntVector& InKey) const
{
	const auto Bounds = FineCoverageBox(InKey);
	return CoverageIndex.AnyRecord(Bounds.Min, Bounds.Max, [](const FVoxelCoverageRecord& Record) { return Record.bWanted; });
}

bool FVoxelViewManager::IsProxyInsideRenderDomain(const FVoxelViewKey& InKey) const
{
	const auto Bounds = VoxelProxyCoverageBox(InKey);
	return CoverageIndex.AnyRecord(Bounds.Min, Bounds.Max, [](const FVoxelCoverageRecord& Record) { return Record.bWanted; });
}

bool FVoxelViewManager::IsSurfaceInsideRenderDomain(const FVoxelSurfaceTileKey& InKey) const
{
	const auto Bounds = FVoxelCoverageBox{FIntVector(SurfaceCoverageRect(InKey).Min.X, SurfaceCoverageRect(InKey).Min.Y, MIN_int32 + 1), FIntVector(SurfaceCoverageRect(InKey).Max.X, SurfaceCoverageRect(InKey).Max.Y, MAX_int32)};
	return CoverageIndex.AnyRecord(Bounds.Min, Bounds.Max, [](const FVoxelCoverageRecord& Record) { return Record.bWanted; });
}

bool FVoxelViewManager::IsMacroInsideRenderDomain(const FVoxelMacroTileKey& InKey) const
{
	const auto Bounds = FVoxelCoverageBox{FIntVector(MacroCoverageRect(InKey).Min.X, MacroCoverageRect(InKey).Min.Y, MIN_int32 + 1), FIntVector(MacroCoverageRect(InKey).Max.X, MacroCoverageRect(InKey).Max.Y, MAX_int32)};
	return CoverageIndex.AnyRecord(Bounds.Min, Bounds.Max, [](const FVoxelCoverageRecord& Record) { return Record.bWanted; });
}

void FVoxelViewManager::MarkCoverageDirty()
{
	bCoverageDirty = true;
	bCoverageDirtyFull = true;
	DirtyCoverageRects.Reset();
}

void FVoxelViewManager::MarkCoverageDirty(const FVoxelCoverageRect& InBounds)
{
	if (!InBounds.IsValid())
	{
		MarkCoverageDirty();
		return;
	}
	bCoverageDirty = true;
	if (bCoverageDirtyFull) return;
	RefreshIndexedCoverage(InBounds);
	MergeCoverageRect(DirtyCoverageRects, InBounds);
}

void FVoxelViewManager::MergeCoverageRect(TArray<FVoxelCoverageRect>& InOutRects, const FVoxelCoverageRect& InBounds)
{
	FVoxelCoverageRect Merged{InBounds.Min - FIntPoint(1, 1),
		InBounds.Max + FIntPoint(1, 1)};
	for (int32 Index = 0; Index < InOutRects.Num();)
	{
		const FVoxelCoverageRect& Existing = InOutRects[Index];
		if (!Merged.Intersects(Existing))
		{
			++Index;
			continue;
		}
		Merged.Min.X = FMath::Min(Merged.Min.X, Existing.Min.X);
		Merged.Min.Y = FMath::Min(Merged.Min.Y, Existing.Min.Y);
		Merged.Max.X = FMath::Max(Merged.Max.X, Existing.Max.X);
		Merged.Max.Y = FMath::Max(Merged.Max.Y, Existing.Max.Y);
		InOutRects.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		Index = 0;
	}
	InOutRects.Add(Merged);
}

void FVoxelViewManager::ExpandActiveCoverage(const FVoxelCoverageRect& InBounds)
{
	if (bActiveCoverageFull) return;
	if (!InBounds.IsValid())
	{
		bActiveCoverageFull = true;
		return;
	}
	if (CoverageAffects(InBounds)) return;
	MergeCoverageRect(ActiveDirtyCoverageRects, InBounds);
}

void FVoxelViewManager::ApplyInterestDelta(const FVoxelInterestDelta& InDelta)
{
	if (GetViewInterest().PublicationInputs)
	{
		// Index, dirty scopes and retirement events are applied by bounded input cursors.
		QueuedInterestSnapshot = Module.GetDesiredInterestSnapshot();
		bTerrainBuildStageDirty = true;
		bHeightfieldTopologyDirty |= !InDelta.AddedSurface.IsEmpty() || !InDelta.RemovedSurface.IsEmpty() ||
			!InDelta.AddedMacro.IsEmpty() || !InDelta.RemovedMacro.IsEmpty();
		return;
	}
	for (const auto& Key : InDelta.RemovedFine) QueueRetirement({4, Key, 0});
	for (const auto& Key : InDelta.RemovedProxy) QueueRetirement({1, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.RemovedSurface) QueueRetirement({2, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.RemovedMacro) QueueRetirement({3, Key.Coordinate, Key.Level});
	for (const auto& Key : InDelta.ChangedGeometry)
	{
		if (const auto* Geometry = GetViewInterest().Geometry.Find(Key))
		{
			FString Error;
			if (!Module.GetRuntime()->GetChangeHierarchy().RegisterGeometry(*Geometry, Error))
			{
				QueuedInterestSnapshot = Module.GetDesiredInterestSnapshot();
				return;
			}
			FIntVector Min, Max;
			if (Key.TryBounds(Geometry->WorldMinZ, Geometry->WorldMaxZ, Min, Max))
				MarkCoverageDirty({FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)});
			bHeightfieldTopologyDirty |= Key.IsColumn();
		}
	}
	bTerrainBuildStageDirty = true;
	RefreshTerrainBuildStage();
	bHeightfieldTopologyDirty |= !InDelta.AddedSurface.IsEmpty() || !InDelta.RemovedSurface.IsEmpty() || !InDelta.AddedMacro.IsEmpty() || !InDelta.RemovedMacro.IsEmpty();
	if (bCoverageDirtyFull || !InDelta.bVisualPartitionChanged) return;
	for (const FIntVector& Key : InDelta.AddedFine) MarkCoverageDirty(FineCoverageRect(Key));
	for (const FIntVector& Key : InDelta.RemovedFine) MarkCoverageDirty(FineCoverageRect(Key));
	for (const FVoxelViewKey& Key : InDelta.AddedProxy) MarkCoverageDirty(VoxelProxyCoverageRect(Key));
	for (const FVoxelViewKey& Key : InDelta.RemovedProxy) MarkCoverageDirty(VoxelProxyCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : InDelta.AddedSurface) MarkCoverageDirty(SurfaceCoverageRect(Key));
	for (const FVoxelSurfaceTileKey& Key : InDelta.RemovedSurface) MarkCoverageDirty(SurfaceCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : InDelta.AddedMacro) MarkCoverageDirty(MacroCoverageRect(Key));
	for (const FVoxelMacroTileKey& Key : InDelta.RemovedMacro) MarkCoverageDirty(MacroCoverageRect(Key));
}

bool FVoxelViewManager::CoverageAffects(const FVoxelCoverageRect& InBounds) const
{
	return bActiveCoverageFull ||
		VoxelCoverage::IntersectsAny2D(InBounds, ActiveDirtyCoverageRects);
}

void FVoxelViewManager::RetryActiveCoverage()
{
	bCoverageRetryRequested = true;
	if (bActiveCoverageFull || ActiveDirtyCoverageRects.IsEmpty())
	{
		MarkCoverageDirty();
		return;
	}
	bCoverageDirty = true;
	if (bCoverageDirtyFull) return;
	DirtyCoverageRects.Append(ActiveDirtyCoverageRects);
}

FGuid FVoxelViewManager::BeginPublishGroup(const FVoxelPublishGroupKey& InKey)
{
	auto& Groups = bCoverageReadScope ? ActiveCoveragePublishGroups : OpenPublishGroups;
	if (const FGuid* Existing = Groups.Find(InKey))
		return *Existing;
	if (bCoverageReadScope)
		if (const FGuid* Existing = OpenPublishGroups.Find(InKey))
		{
			const FGuid Group = *Existing;
			OpenPublishGroups.Remove(InKey);
			Groups.Add(InKey, Group);
			return Group;
		}
	const FGuid Group = Publisher->BeginGroup(InKey);
	Groups.Add(InKey, Group);
	return Group;
}

bool FVoxelViewManager::ShouldDeferCoverage(const FVoxelCoverageRect& InBounds) const
{
	if (!CoverageContinuation.IsValid() || bCoverageReadScope) return false;
	const FVoxelCoverageRect Halo{InBounds.Min - FIntPoint(16), InBounds.Max + FIntPoint(16)};
	return CoverageAffects(Halo);
}

bool FVoxelViewManager::ShouldDeferCoverage(const FVoxelTaskStamp& InStamp) const
{
	if (!CoverageContinuation.IsValid() || bCoverageReadScope) return false;
	if (InStamp.bHasGeometryIdentity)
	{
		FIntVector Min, Max;
		if (!InStamp.GeometryIdentity.Key.TryBounds(InStamp.GeometryIdentity.WorldMinZ, InStamp.GeometryIdentity.WorldMaxZ, Min, Max)) return true;
		return ShouldDeferCoverage(FVoxelCoverageRect{FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)});
	}
	return ShouldDeferCoverage(FineCoverageRect(InStamp.Section));
}

FVoxelCoverageRect FVoxelViewManager::CoverageBounds(const FVoxelTaskStamp& InStamp) const
{
	if (!InStamp.bHasGeometryIdentity) return FineCoverageRect(InStamp.Section);
	FIntVector Min, Max;
	if (!InStamp.GeometryIdentity.Key.TryBounds(InStamp.GeometryIdentity.WorldMinZ, InStamp.GeometryIdentity.WorldMaxZ, Min, Max)) return {};
	return {FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y)};
}

void FVoxelViewManager::ResolveTransitionVisibility()
{
	TGuardValue<bool> ReadScope(bCoverageReadScope, true);
	auto Steps = ResolveTransitionVisibilitySteps();
	while (!Steps.IsComplete())
		Steps.Resume();
}

bool FVoxelViewManager::CommitTerrainOwnership(const FVoxelPublishGroupKey& InKey, const TConstArrayView<FVoxelViewKey> InTarget)
{
	if (InKey.Representation == 1)
		if (auto* Metrics = GeometryMetrics.Find({EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level}))
			Metrics->bPresented = !InTarget.IsEmpty();
	// 每代提交替换此 owner 当前的完整节点集，包括前代在快照形成后才提交的节点。
	TArray<FVoxelViewKey>& Current = VisibleTerrainNodesByGroup.FindOrAdd(InKey);
	bool bChanged = false;
	for (const FVoxelViewKey& Node : Current)
		if (!InTarget.Contains(Node))
			bChanged |= VisibleTerrainNodes.Remove(Node) > 0;
	for (const FVoxelViewKey& Node : InTarget)
		if (!VisibleTerrainNodes.Contains(Node))
		{
			VisibleTerrainNodes.Add(Node);
			bChanged = true;
		}
	for (const auto& Node : Current)
		IndexCoverage(Node);
	for (const auto& Node : InTarget)
		IndexCoverage(Node);
	Current = TArray<FVoxelViewKey>(InTarget);
	if (Current.IsEmpty())
		VisibleTerrainNodesByGroup.Remove(InKey);
	return bChanged;
}

void FVoxelViewManager::RebuildHeightfieldTransitions(const FCoveragePassSnapshot& InSnapshot)
{
	auto Work = RebuildHeightfieldTransitionSteps(InSnapshot);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
}

void FVoxelViewManager::PumpHeightfieldTransitions()
{
	if (UnsubmittedTransitionOwners.IsEmpty())
		return;
	const auto Config = Module.GetGenerationConfig();
	const auto Registry = Module.GetRegistry();
	if (!Config || !Config->Recipe || !Registry)
		return;
	const double Deadline = TerrainDeadline(Module.GetViewSettings().Budget.AdmissionMilliseconds);
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<64>> Candidates;
	TArray<FVoxelHeightfieldNodeKey, TInlineAllocator<64>> Owners;
	const int32 ScanCount = FMath::Min(64, UnsubmittedTransitionOwners.Num());
	for (int32 Scan = 0; Scan < ScanCount && FPlatformTime::Seconds() < Deadline; ++Scan)
	{
		TransitionAdmissionCursor[0] %= UnsubmittedTransitionOwners.Num();
		const auto Owner = UnsubmittedTransitionOwners[TransitionAdmissionCursor[0]++];
		const auto Rect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ?
			SurfaceCoverageRect({Owner.Coordinate, Owner.Level}) : MacroCoverageRect({Owner.Coordinate, Owner.Level});
		if (ShouldDeferCoverage(Rect)) continue;
		const auto Inputs = DesiredHeightfieldInputs.FindRef(Owner);
		FVoxelTerrainServiceCandidate Candidate;
		const FVoxelTerrainSpatialKey Key{Owner.Representation == EVoxelHeightfieldRepresentation::Surface ?
			EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro,
			FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, 0), Owner.Level};
		if (const auto* Priority = GetViewInterest().GeometryServicePriorities.Find(Key)) Candidate.Priority = *Priority;
		else Candidate.Priority.DistanceCells = MinimumObserverDistanceCells(
			Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceWorldCenter({Owner.Coordinate, Owner.Level}) :
			MacroWorldCenter({Owner.Coordinate, Owner.Level}));
		Candidate.Index = Owners.Add(Owner);
		Candidate.FirstQueuedSeconds = Inputs ? Inputs->FirstQueuedSeconds : 0.0;
		Candidate.Sequence = Inputs ? Inputs->InputSerial : 0;
		Candidate.EstimatedMicroseconds = TransitionAdmissionCostMicroseconds[0];
		Candidate.bRunnable = true;
		Candidates.Add(Candidate);
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	for (int32 Submitted = 0; Submitted < 4 && FPlatformTime::Seconds() < Deadline;)
	{
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Chosen = TransitionAdmissionSelection[0].Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Chosen == INDEX_NONE) break;
		Candidates[Chosen].bRunnable = false;
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		ON_SCOPE_EXIT
		{
			const double Actual = (FPlatformTime::Seconds() - Started) * 1000000.0;
			TransitionAdmissionSelection[0].Settle(Charge.Serial, Actual);
			TransitionAdmissionCostMicroseconds[0] = FMath::Lerp(TransitionAdmissionCostMicroseconds[0], FMath::Max(1.0, Actual), 0.15);
		};
		const FVoxelHeightfieldNodeKey Owner = Owners[Chosen];
		const uint64* Signature = DesiredTransitionSignatures.Find(Owner);
		const TArray<FVoxelHeightfieldTransitionEdge>* Edges = DesiredTransitionEdges.Find(Owner);
		const auto Inputs = DesiredHeightfieldInputs.FindRef(Owner);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Owner);
		const uint64* Pending = PendingTransitionSignatures.Find(Owner);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Owner);
		if (!Signature || !Edges || !Inputs || !AreHeightfieldInputsCurrent(*Inputs) ||
			Edges->ContainsByPredicate([](const auto& Edge) { return Edge.bPending; }) ||
			(Built && *Built == *Signature) || (Pending && *Pending == *Signature) || (Prepared && *Prepared == *Signature))
		{
			UnsubmittedTransitionOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewTransition;
		Request.TerrainStage = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3;
		Request.WorkClass = EVoxelWorkClass::Boundary;
		Request.FirstRunnableSeconds = Inputs->FirstQueuedSeconds;
		Request.ServiceBand = Candidates[Chosen].Priority.Band;
		Request.SourceStableKey = Candidates[Chosen].Priority.SourceStableKey;
		Request.ForwardScore = Candidates[Chosen].Priority.ForwardScore;
		if (Owner.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			if (const auto* Priority = GetViewInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
				Request.SourcePriority = *Priority;
		}
		else if (const auto* Priority = GetViewInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
			Request.SourcePriority = *Priority;
		Request.DistanceScore = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
									? MinimumObserverDistanceCells(SurfaceWorldCenter({Owner.Coordinate, Owner.Level}))
									: MinimumObserverDistanceCells(MacroWorldCenter({Owner.Coordinate, Owner.Level}));
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Token = *Signature;
		Request.Stamp.Section = FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, static_cast<int32>(Owner.Representation));
		Request.Stamp.ViewKey.Level = Owner.Level;
		const FVoxelTerrainSpatialKey SpatialKey{Owner.Representation == EVoxelHeightfieldRepresentation::Surface ?
			EVoxelTerrainRepresentation::Surface : EVoxelTerrainRepresentation::Macro,
			FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, 0), Owner.Level};
		const auto* Geometry = GetViewInterest().Geometry.Find(SpatialKey);
		if (!Geometry) { UnsubmittedTransitionOwners.RemoveSingleSwap(Owner, EAllowShrinking::No); continue; }
		Request.Stamp.OwnerId = TaskOwnerId;
		Request.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		Request.Stamp.bHasGeometryIdentity = true;
		Request.Stamp.GeometryIdentity = *Geometry;
		Request.Stamp.GeometryRevision = GetTypeHash(*Geometry);
		Request.bResumable = true;
		const auto Storage = CreateCoarseBuildStorage(Scheduler, Request, *Geometry,
			VoxelBoundedAllocation::WorkerSharedBytes<FHeightfieldTransitionBuild>() +
			VoxelBoundedAllocation::MapBytes<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView>(Inputs->Sources.Num()), 0);
		if (!Storage) continue;
		Storage->WorkingCapacityBytes = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
		const auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FHeightfieldTransitionBuild>(Storage->MetadataLease);
		Worker->Storage = Storage; Worker->Inputs = Inputs;
		Worker->GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
		Worker->TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells;
		{
			FString CallbackBindingError;
			if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[Worker, Config, Registry](const TAtomic<bool>& Cancel) { return Worker->Advance(*Config, *Registry, Cancel); }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		FVoxelCoverageLocalSnapshot Guard;
		const auto Rect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																						   : MacroCoverageRect({Owner.Coordinate, Owner.Level});
		HeightfieldCoverageIndex.Capture(FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, Module.GetManifest().Settings.MinZ),
			FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, Module.GetManifest().Settings.MaxZ), Guard, false);
		if (Guard.bCountOnly) continue;
		{
			FString CallbackBindingError;
			if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[this, Owner, Inputs, TaskSignature = *Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (!AreHeightfieldInputsCurrent(*Inputs) || !HeightfieldCoverageIndex.IsCurrent(Guard))
			{
				Result.bSuccess = false;
				Result.bCanceled = true;
			}
			ApplyHeightfieldTransition(Owner, TaskSignature, MoveTemp(Result));
		}, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		if (Scheduler.IsTerminal({Request.Kind, Request.Stamp}))
		{
			UnsubmittedTransitionOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
			continue;
		}
		if (!EnqueueViewTask(MoveTemp(Request)))
			continue;
		PendingTransitionSignatures.Add(Owner, *Signature);
		UnsubmittedTransitionOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
		++Submitted;
	}
}

void FVoxelViewManager::ApplyHeightfieldTransition(const FVoxelHeightfieldNodeKey& InOwner, const uint64 InSignature, FVoxelTaskResult&& InResult)
{
	if (const uint64* Pending = PendingTransitionSignatures.Find(InOwner); Pending && *Pending == InSignature)
	{
		PendingTransitionSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredTransitionSignatures.Find(InOwner);
	if (InResult.Stamp.WorldEpoch != WorldEpoch || !Desired || *Desired != InSignature)
		return;
	if (InResult.bCanceled)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.CustomPayload)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heightfield transition build failed: %s"), *InResult.Error);
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildViewTransition, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		return;
	}
	const auto Payload = StaticCastSharedPtr<const FHeightfieldTransitionTaskPayload>(InResult.CustomPayload);
	if (!Payload->Mesh)
	{
		UnsubmittedTransitionOwners.AddUnique(InOwner);
		return;
	}
	PreparedTransitionSignatures.Add(InOwner, InSignature);
	Payload->Mesh->WorkReceipt = InResult.WorkReceipt;
	PreparedTransitionMeshes.Add(InOwner, Payload->Mesh);
	MarkCoverageDirty(InOwner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({InOwner.Coordinate, InOwner.Level})
																						 : MacroCoverageRect({InOwner.Coordinate, InOwner.Level}));
}

bool FVoxelViewManager::VolumeHandoffAffectsHeightfield(const FVoxelHeightfieldNodeKey& InKey) const
{
	FVoxelHeightfieldTileView View;
	if (InKey.Representation == EVoxelHeightfieldRepresentation::Surface)
	{
		const auto Data = SurfaceData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data)
			return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	else
	{
		const auto Data = MacroData.FindRef({InKey.Coordinate, InKey.Level});
		if (!Data)
			return false;
		View = FVoxelHeightfieldTransitionBuilder::MakeView(*Data);
	}
	int32 MinimumHeight = MAX_int32;
	int32 MaximumHeight = MIN_int32;
	for (const int32 Height : View.Ground)
	{
		if (Height == MIN_int32)
			continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	for (const int32 Height : View.Water)
	{
		if (Height == MIN_int32)
			continue;
		MinimumHeight = FMath::Min(MinimumHeight, Height);
		MaximumHeight = FMath::Max(MaximumHeight, Height);
	}
	if (MinimumHeight > MaximumHeight)
		return false;
	const FIntPoint Maximum = View.Origin + FIntPoint(View.CellSide() * View.Step);
	const FVoxelCoverageBox Bounds{FIntVector(View.Origin.X, View.Origin.Y, MinimumHeight - 1), FIntVector(Maximum.X, Maximum.Y, MaximumHeight + 2)};
	return VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes);
}

void FVoxelViewManager::RebuildVolumeTransitions(const TSet<FVoxelViewKey>& InTargetNodes)
{
	auto Work = RebuildVolumeTransitionSteps(InTargetNodes);
	while (!Work.IsComplete())
	{
		Work.Resume();
	}
}

TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FVoxelViewManager::FindFineBoundaryGeometry(const FIntVector& InSection) const
{
	for (const int32 Side : {16, 32, 64})
	{
		FIntVector Min;
		if (!FVoxelFineSpatial::TrySnap(InSection * 16, Side, Min)) continue;
		if (const auto* Slot = FineShardSlots.Find({Min, Side}); Slot && Slot->Build.IsMeshReady())
			if (const auto Geometry = Slot->Build.GetPreparedGeometry()) return Geometry;
	}
	const auto Presented = CapturePresentedSnapshot();
	const auto* Evidence = Presented ? Presented->Find(InSection) : nullptr;
	if (Evidence && (Evidence->Owner.Kind == EVoxelFineOwnerKind::F0 || Evidence->Owner.Kind == EVoxelFineOwnerKind::F1Plus))
		return StaticCastSharedPtr<const FVoxelFineShardGeometry>(Evidence->GeometryLease);
	return nullptr;
}

bool FVoxelViewManager::AreBoundaryInputsCurrent(const FVoxelBoundaryTransitionContext& InContext) const
{
	if (InContext.WorldEpoch != WorldEpoch || InContext.GeneratorIdentity != Module.GetManifest().BaseSampleHash ||
		InContext.RegistryRevision != Module.GetRegistry()->Hash || InContext.ShapeRevision != Module.GetShapes()->GetContentHash()) return false;
	if (!InContext.OwnerGeometry || VoxelProxyData.FindRef(InContext.Owner) != InContext.OwnerGeometry) return false;
	const auto Desired = DesiredVolumeContexts.FindRef(InContext.Owner);
	if (!Desired || Desired->InputSerial != InContext.InputSerial || !Desired->SameInputs(InContext)) return false;
	for (const auto& Patch : InContext.Patches)
	{
		if (Patch.Neighbor.FineGeometry)
		{
			if (FindFineBoundaryGeometry(Patch.Face.Neighbor.Coordinate) != Patch.Neighbor.FineGeometry ||
				!Module.IsOverlayVersionCurrent(Patch.Neighbor.FineGeometry->OverlayVersion)) return false;
		}
		else if (!Patch.Neighbor.ProxyGeometry || VoxelProxyData.FindRef(Patch.Face.Neighbor) != Patch.Neighbor.ProxyGeometry) return false;
	}
	for (const auto& Source : InContext.SuppressionSources)
		if (!Source || VoxelProxyData.FindRef(Source->Key) != Source) return false;
	return true;
}

bool FVoxelViewManager::AreHeightfieldInputsCurrent(const FVoxelFrozenHeightfieldInputs& InInputs) const
{
	if (InInputs.WorldEpoch != WorldEpoch || InInputs.GeneratorIdentity != Module.GetManifest().BaseSampleHash ||
		InInputs.RegistryRevision != Module.GetRegistry()->Hash || InInputs.ShapeRevision != Module.GetShapes()->GetContentHash()) return false;
	const auto Desired = DesiredHeightfieldInputs.FindRef(InInputs.Owner);
	if (!Desired || Desired->InputSerial != InInputs.InputSerial || !Desired->SameInputs(InInputs)) return false;
	for (const auto& Source : InInputs.Sources)
	{
		if (Source.Surface)
		{
			if (SurfaceData.FindRef({Source.Key.Coordinate, Source.Key.Level}) != Source.Surface) return false;
		}
		else if (!Source.Macro || MacroData.FindRef({Source.Key.Coordinate, Source.Key.Level}) != Source.Macro) return false;
	}
	return true;
}

void FVoxelViewManager::PumpVolumeTransitions()
{
	if (UnsubmittedVolumeOwners.IsEmpty())
		return;
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	if (!Registry || !Shapes)
		return;
	const double Deadline = TerrainDeadline(Module.GetViewSettings().Budget.AdmissionMilliseconds);
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<64>> Candidates;
	TArray<FVoxelViewKey, TInlineAllocator<64>> Owners;
	const int32 ScanCount = FMath::Min(64, UnsubmittedVolumeOwners.Num());
	for (int32 Scan = 0; Scan < ScanCount && FPlatformTime::Seconds() < Deadline; ++Scan)
	{
		TransitionAdmissionCursor[1] %= UnsubmittedVolumeOwners.Num();
		const auto Owner = UnsubmittedVolumeOwners[TransitionAdmissionCursor[1]++];
		if (ShouldDeferCoverage(VoxelProxyCoverageRect(Owner))) continue;
		const auto Context = DesiredVolumeContexts.FindRef(Owner);
		FVoxelTerrainServiceCandidate Candidate;
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::VoxelProxy, Owner.Coordinate, Owner.Level};
		if (const auto* Priority = GetViewInterest().GeometryServicePriorities.Find(Key)) Candidate.Priority = *Priority;
		else Candidate.Priority.DistanceCells = MinimumObserverDistanceCells(Owner.GetBounds());
		Candidate.Index = Owners.Add(Owner);
		Candidate.FirstQueuedSeconds = Context ? Context->FirstQueuedSeconds : 0.0;
		Candidate.Sequence = Context ? Context->InputSerial : 0;
		Candidate.EstimatedMicroseconds = TransitionAdmissionCostMicroseconds[1];
		Candidate.bRunnable = true;
		Candidates.Add(Candidate);
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	for (int32 Submitted = 0; Submitted < 4 && FPlatformTime::Seconds() < Deadline;)
	{
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Chosen = TransitionAdmissionSelection[1].Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Chosen == INDEX_NONE) break;
		Candidates[Chosen].bRunnable = false;
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		ON_SCOPE_EXIT
		{
			const double Actual = (FPlatformTime::Seconds() - Started) * 1000000.0;
			TransitionAdmissionSelection[1].Settle(Charge.Serial, Actual);
			TransitionAdmissionCostMicroseconds[1] = FMath::Lerp(TransitionAdmissionCostMicroseconds[1], FMath::Max(1.0, Actual), 0.15);
		};
		const FVoxelViewKey Owner = Owners[Chosen];
		const uint64* Signature = DesiredVolumeSignatures.Find(Owner);
		const auto Data = VoxelProxyData.FindRef(Owner);
		const auto Context = DesiredVolumeContexts.FindRef(Owner);
		if (!Signature || !Data || !Context || !AreBoundaryInputsCurrent(*Context) || VolumeTransitionSignatures.FindRef(Owner) == *Signature ||
			PendingVolumeSignatures.FindRef(Owner) == *Signature || PreparedVolumeSignatures.FindRef(Owner) == *Signature)
		{
			UnsubmittedVolumeOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
			continue;
		}
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildVolumeTransition;
		Request.WorkClass = VolumeTransitionWorkClass(Owner);
		Request.FirstRunnableSeconds = Context->FirstQueuedSeconds;
		Request.ServiceBand = Candidates[Chosen].Priority.Band;
		Request.SourceStableKey = Candidates[Chosen].Priority.SourceStableKey;
		Request.ForwardScore = Candidates[Chosen].Priority.ForwardScore;
		if (const auto* Priority = GetViewInterest().VoxelProxyPriorities.Find(Owner)) Request.SourcePriority = *Priority;
		Request.DistanceScore = MinimumObserverDistanceCells(Owner.GetBounds());
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.ViewKey = Owner;
		Request.Stamp.OwnerId = TaskOwnerId;
		Request.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		Request.Stamp.Revision = Data->Revision;
		Request.Stamp.GeometryRevision = GetTypeHash(Data->Geometry);
		Request.Stamp.bHasGeometryIdentity = true;
		Request.Stamp.GeometryIdentity = Data->Geometry;
		Request.Stamp.Token = *Signature;
		Request.bResumable = true;
		const auto Storage = CreateCoarseBuildStorage(Scheduler, Request, Data->Geometry,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVolumeTransitionBuild>());
		if (!Storage) continue;
		Storage->WorkingCapacityBytes = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
		const auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FVoxelVolumeTransitionBuild>(Storage->MetadataLease);
		Worker->Storage = Storage; Worker->Data = Data; Worker->Inputs = Context;
		Worker->GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
		{
			FString CallbackBindingError;
			if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[Worker, Registry, Shapes](const TAtomic<bool>& Cancel) { return Worker->Advance(*Registry, *Shapes, Cancel); }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		FVoxelCoverageLocalSnapshot Guard;
		const auto Bounds = Owner.GetBounds();
		CoverageIndex.Capture(Bounds.Min - FIntVector(1), Bounds.Max + FIntVector(1), Guard, false);
		if (Guard.bCountOnly) continue;
		{
			FString CallbackBindingError;
			if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[this, Owner, Data, Context, TaskSignature = *Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (!AreBoundaryInputsCurrent(*Context) || VoxelProxyData.FindRef(Owner) != Data || !CoverageIndex.IsCurrent(Guard))
			{
				Result.bSuccess = false;
				Result.bCanceled = true;
			}
			ApplyVolumeTransition(Owner, TaskSignature, MoveTemp(Result));
		}, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		if (Scheduler.IsTerminal({Request.Kind, Request.Stamp}))
		{
			UnsubmittedVolumeOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
			continue;
		}
		if (!EnqueueViewTask(MoveTemp(Request)))
			continue;
		PendingVolumeSignatures.Add(Owner, *Signature);
		UnsubmittedVolumeOwners.RemoveSingleSwap(Owner, EAllowShrinking::No);
		++Submitted;
	}
}

void FVoxelViewManager::ApplyVolumeTransition(const FVoxelViewKey& InOwner, const uint64 InSignature, FVoxelTaskResult&& InResult)
{
	if (PendingVolumeSignatures.FindRef(InOwner) == InSignature)
	{
		PendingVolumeSignatures.Remove(InOwner);
	}
	const uint64* Desired = DesiredVolumeSignatures.Find(InOwner);
	const auto* Geometry = GetViewInterest().FindGeometry(InOwner);
	if (!Geometry || GetTypeHash(*Geometry) != InResult.Stamp.GeometryRevision || InResult.Stamp.WorldEpoch != WorldEpoch || !Desired ||
		*Desired != InSignature)
		return;
	if (InResult.bCanceled)
	{
		UnsubmittedVolumeOwners.AddUnique(InOwner);
		return;
	}
	if (!InResult.bSuccess || !InResult.VoxelProxyMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Volume transition build failed: %s"), *InResult.Error);
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildVolumeTransition, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		MarkCoverageDirty(VoxelProxyCoverageRect(InOwner));
		return;
	}
	PreparedVolumeMeshes.Add(InOwner, InResult.VoxelProxyMesh);
	PreparedVolumeBoundaryMeshes.Add(InOwner, InResult.VolumeBoundaryMesh);
	InResult.VoxelProxyMesh->WorkReceipt = InResult.WorkReceipt;
	if (InResult.VolumeBoundaryMesh) InResult.VolumeBoundaryMesh->WorkReceipt = InResult.WorkReceipt;
	if (const auto Context = DesiredVolumeContexts.FindRef(InOwner))
		for (const auto& Patch : Context->Patches)
			if (Patch.Face.Neighbor.Level == 0)
				if (const auto* Page = GetInputRoot().FinePagePlan.SectionOwners.Find(Patch.Face.Neighbor.Coordinate))
					MarkFinePageDirty(*Page);
	bFinePresentationDirty |= !DirtyFinePages.IsEmpty();
	PreparedVolumeSignatures.Add(InOwner, InSignature);
	MarkCoverageDirty(VoxelProxyCoverageRect(InOwner));
}

void FVoxelViewManager::QueueRetirement(const FVoxelPublishGroupKey& InKey)
{
	if (!RetirementIndices.Contains(InKey))
		RetirementIndices.Add(InKey, RetirementQueue.Add(InKey));
}

void FVoxelViewManager::RemoveRetirement(const FVoxelPublishGroupKey& InKey)
{
	const auto* Found = RetirementIndices.Find(InKey);
	if (!Found)
		return;
	const int32 Index = *Found;
	RetirementIndices.Remove(InKey);
	RetirementQueue.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	if (RetirementQueue.IsValidIndex(Index))
		RetirementIndices.FindChecked(RetirementQueue[Index]) = Index;
	RetirementCursor = RetirementQueue.IsEmpty() ? 0 : RetirementCursor % RetirementQueue.Num();
}


bool FVoxelViewManager::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.OverlayReadResult && !Module.IsOverlayReadCurrent(*InResult.OverlayReadResult))
	{
		++TerrainWorkMetrics.Stale;
		InResult.bCanceled = true;
		InResult.bSuccess = false;
	}
	if (InResult.bCanceled)
		++TerrainWorkMetrics.Canceled;
	if (InResult.Stamp.WorldEpoch != WorldEpoch)
		++TerrainWorkMetrics.Stale;
	if (ShouldDeferCoverage(InResult.Stamp) && InResult.bSuccess && !InResult.bCanceled && InResult.Stamp.WorldEpoch == WorldEpoch &&
		(InResult.Kind == EVoxelTaskKind::BuildFineMesh || InResult.Kind == EVoxelTaskKind::BuildVoxelProxy || InResult.Kind == EVoxelTaskKind::BuildSurface ||
			InResult.Kind == EVoxelTaskKind::BuildMacro))
	{
		DeferredCoverageResults.Add(MoveTemp(InResult));
		return true;
	}
	if (InResult.Stamp.WorldEpoch != WorldEpoch || InResult.bCanceled || !InResult.bSuccess)
	{
		if (InResult.Stamp.WorldEpoch == WorldEpoch && !InResult.bCanceled && !InResult.bSuccess)
		{
			Scheduler.ObserveDemand(InResult.Kind, InResult.Stamp, EVoxelTaskBlockReason::TerminalFailure);
			UE_LOG(LogTemp, Warning, TEXT("Voxel representation task failed: kind=%d error=%s"), static_cast<int32>(InResult.Kind), *InResult.Error);
		}
		return InResult.Kind == EVoxelTaskKind::BuildFineMesh || InResult.Kind == EVoxelTaskKind::BuildVoxelProxy ||
			   InResult.Kind == EVoxelTaskKind::BuildSurface || InResult.Kind == EVoxelTaskKind::BuildWater || InResult.Kind == EVoxelTaskKind::BuildMacro;
	}

	switch (InResult.Kind)
	{
	case EVoxelTaskKind::BuildFineMesh:
		if (PublishFine(InResult))
			MarkCoverageDirty(FineCoverageRect(InResult.Stamp.Section));
		return true;
	case EVoxelTaskKind::BuildVoxelProxy:
		if (PublishVoxelProxy(InResult))
			MarkCoverageDirty(VoxelProxyCoverageRect(InResult.Stamp.ViewKey));
		return true;
	case EVoxelTaskKind::BuildSurface:
		if (InResult.Surface && GetInputRoot().SurfaceWanted.Contains(InResult.Stamp.SurfaceKey) && PublishSurface(InResult))
		{
			const FVoxelSurfaceTileKey Key = InResult.Stamp.SurfaceKey;
			MarkCoverageDirty(SurfaceCoverageRect(Key));
			if (InResult.Water)
				WaterData.Add(Key, InResult.Water);
			else
				WaterData.Remove(Key);
		}
		return true;
	case EVoxelTaskKind::BuildWater:
		// 升级过程中已排队的旧 Water 任务只做丢弃。
		return true;
	case EVoxelTaskKind::BuildMacro:
		if (InResult.Macro && GetInputRoot().MacroWanted.Contains(InResult.Stamp.MacroKey) && PublishMacro(InResult))
		{
			MarkCoverageDirty(MacroCoverageRect(InResult.Stamp.MacroKey));
		}
		return true;
	default:
		return false;
	}
}

bool FVoxelViewManager::ApplyRemoteRepresentation(const FVoxelRepresentationReply& InReply, FString& OutError)
{
	if (InReply.SharedData && (!InReply.SharedData->Lease || InReply.SharedData->Lease->Bytes() <
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() + InReply.SharedData->Bytes.GetAllocatedSize()))
	{
		OutError = TEXT("Remote terrain body has no valid reservation");
		return false;
	}
	if (InReply.Kind == EVoxelRepresentationReplyKind::Retry || InReply.Kind == EVoxelRepresentationReplyKind::Unchanged)
	{
		OutError.Reset();
		return true;
	}
	const FVoxelTerrainGeometry* Desired = nullptr;
	FVoxelTaskRequest Request;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Revision = InReply.Revision;
	Request.WorkClass = EVoxelWorkClass::Visible;
	if (InReply.Type == EVoxelRepresentationWireType::VoxelProxy)
	{
		Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
		Request.Stamp.ViewKey = {InReply.Key.Coordinate, InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.ViewKey);
	}
	else if (InReply.Type == EVoxelRepresentationWireType::SurfaceProxy)
	{
		Request.Kind = EVoxelTaskKind::BuildSurface;
		Request.Stamp.SurfaceKey = {FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.SurfaceKey);
	}
	else if (InReply.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		Request.Kind = EVoxelTaskKind::BuildMacro;
		Request.Stamp.MacroKey = {FIntPoint(InReply.Key.Coordinate.X, InReply.Key.Coordinate.Y), InReply.Key.Level};
		Desired = GetViewInterest().FindGeometry(Request.Stamp.MacroKey);
	}
	if (InReply.Kind == EVoxelRepresentationReplyKind::Rejected)
	{
		if (!Desired)
		{
			OutError = TEXT("Rejected remote geometry is obsolete");
			return false;
		}
		Request.Stamp = MakeRepresentationStamp(*Desired, InReply.Revision);
		Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::TerminalFailure);
		OutError.Reset();
		return true;
	}
	Request.Kind = InReply.Type == EVoxelRepresentationWireType::VoxelProxy		? EVoxelTaskKind::GenerateVoxelProxy
				   : InReply.Type == EVoxelRepresentationWireType::SurfaceProxy ? EVoxelTaskKind::GenerateSurface
																				: EVoxelTaskKind::GenerateMacro;
	if (!Desired)
	{
		OutError = TEXT("Remote geometry is obsolete");
		return false;
	}
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	const auto Config = Module.GetGenerationConfig();
	if (!Registry || !Shapes || !Config)
	{
		OutError = TEXT("Remote terrain dependencies unavailable");
		return false;
	}
	Request.Stamp = MakeRepresentationStamp(*Desired, InReply.Revision);
	Request.bResumable = true;
	const uint64 RawBytes = FVoxelRemotePageBuild::RawCapacity(*Desired);
	if (!RawBytes)
	{
		Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
		OutError = TEXT("Oversized: remote sampling cannot fit the wire format");
		return false;
	}
	const auto Storage = CreateCoarseBuildStorage(Scheduler, Request, *Desired,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRemotePageBuild>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
		(InReply.SharedData ? 0 : VoxelBoundedAllocation::ArrayBytes<uint8>(InReply.Data.Num())), 1, 1);
	if (!Storage)
	{
		OutError = TEXT("Remote terrain input capacity deferred");
		return false;
	}
	Storage->GeometryCapacityBytes = RawBytes;
	if (!AdmitCoarseCompletion(Scheduler, Request, Storage))
	{
		OutError = TEXT("Remote terrain completion capacity deferred");
		return false;
	}
	const auto Page = VoxelBoundedAllocation::MakeWorkerShared<FVoxelRemotePageBuild>(Storage->MetadataLease);
	Page->Storage = Storage;
	Page->Expected = {*Desired, InReply.Serial, InReply.Revision};
	Page->MeshPieceLimit = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
	Page->GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
	Page->SharedBytes = InReply.SharedData;
	if (!Page->SharedBytes)
	{
		Page->Bytes.Reserve(InReply.Data.Num());
		Page->Bytes.Append(InReply.Data);
	}
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Page, Registry, Shapes, Config, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells](const TAtomic<bool>& Cancel)
	{
		return Page->Advance(*Registry, *Shapes, *Config, TextureStretch, Cancel);
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}
	if (!EnqueuePreparedData(MoveTemp(Request)))
	{
		OutError = TEXT("Remote terrain admission deferred");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelViewManager::InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate,
	int32& InOutCursor, FString& OutError, const int32 InMaximumEntries)
{
	if (InOutCursor < 0 || InOutCursor > InInvalidate.Entries.Num() || InInvalidate.Entries.IsEmpty() ||
		InInvalidate.Entries.Num() > 256 || InMaximumEntries <= 0)
	{
		OutError = TEXT("InvalidRequest: remote invalidation cursor or native count");
		return false;
	}
	auto& Hierarchy = Module.GetRuntime()->GetChangeHierarchy();
	for (int32 Processed = 0; InOutCursor < InInvalidate.Entries.Num() && Processed < InMaximumEntries; ++Processed)
	{
		const auto& Entry = InInvalidate.Entries[InOutCursor];
		const auto& Key = Entry.Geometry.Key;
		const auto* Desired = GetViewInterest().Geometry.Find(Key);
		if (!Desired || *Desired != Entry.Geometry)
		{
			++InOutCursor;
			continue;
		}
		const FVoxelCoverageRect Bounds = Key.Representation == EVoxelTerrainRepresentation::VoxelProxy ?
			VoxelProxyCoverageRect({Key.Coordinate, Key.Level}) : Key.Representation == EVoxelTerrainRepresentation::Surface ?
			SurfaceCoverageRect({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}) :
			MacroCoverageRect({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
		if (ShouldDeferCoverage(Bounds))
		{
			OutError = TEXT("PlanningPending: original invalidation awaits coverage cut");
			return false;
		}
		bool bChanged = false;
		switch (Key.Representation)
		{
		case EVoxelTerrainRepresentation::VoxelProxy:
		{
			const FVoxelViewKey Proxy{Key.Coordinate, Key.Level};
			if (Entry.Revision <= Hierarchy.GetVoxelProxyRevision(Proxy)) break;
			if (!Hierarchy.SetVoxelProxyRevision(Proxy, Entry.Revision, OutError)) return false;
			bChanged = true;
			break;
		}
		case EVoxelTerrainRepresentation::Surface:
		{
			const FVoxelSurfaceTileKey Surface{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
			if (Entry.Revision <= Hierarchy.GetSurfaceRevision(Surface)) break;
			if (!Hierarchy.SetSurfaceRevision(Surface, Entry.Revision, OutError)) return false;
			bChanged = true;
			break;
		}
		case EVoxelTerrainRepresentation::Macro:
		{
			const FVoxelMacroTileKey Macro{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
			if (Entry.Revision <= Hierarchy.GetMacroRevision(Macro)) break;
			if (!Hierarchy.SetMacroRevision(Macro, Entry.Revision, OutError)) return false;
			bChanged = true;
			break;
		}
		default:
			OutError = TEXT("InvalidRequest: remote invalidation representation");
			return false;
		}
		if (bChanged)
		{
			bTerrainBuildStageDirty = true;
			Scheduler.SetObservedTerrainStage(0);
			InvalidateRepresentationKey(Key);
		}
		++InOutCursor;
	}
	if (InOutCursor < InInvalidate.Entries.Num())
	{
		OutError = TEXT("PlanningPending: original remote invalidation cursor");
		return false;
	}
	OutError.Reset();
	return true;
}

void FVoxelViewManager::InvalidateSection(const FIntVector& InKey)
{
	OnContentChanged(InKey);
	if (ShouldDeferCoverage(FineCoverageRect(InKey)))
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue({FineCoverageRect(InKey),
			[this, InKey]
			{
				InvalidateSection(InKey);
			}});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	PrunePreparedData();
	InvalidateNeighbors(InKey);
	InvalidateFinePresentation(InKey);
	Scheduler.CancelSection(InKey);
	Module.GetRuntime()->GetChangeHierarchy().VisitAffectedKeys(InKey,
		[this](const FVoxelTerrainSpatialKey& Key) { InvalidateRepresentationKey(Key); });
}

void FVoxelViewManager::InvalidateRepresentationKeys(const FVoxelAffectedRepresentationKeys& InAffected)
{
	for (const auto& Key : InAffected.Proxy)
		InvalidateRepresentationKey({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level});
	for (const auto& Key : InAffected.Surface)
		InvalidateRepresentationKey({EVoxelTerrainRepresentation::Surface, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level});
	for (const auto& Key : InAffected.Macro)
		InvalidateRepresentationKey({EVoxelTerrainRepresentation::Macro, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level});
}

void FVoxelViewManager::InvalidateRepresentationKey(const FVoxelTerrainSpatialKey& InKey)
{
	const FVoxelCoverageRect Bounds = InKey.Representation == EVoxelTerrainRepresentation::VoxelProxy ?
		VoxelProxyCoverageRect({InKey.Coordinate, InKey.Level}) : InKey.Representation == EVoxelTerrainRepresentation::Surface ?
		SurfaceCoverageRect({FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level}) :
		MacroCoverageRect({FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level});
	if (ShouldDeferCoverage(Bounds))
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue({Bounds, [this, InKey] { InvalidateRepresentationKey(InKey); }});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	if (InKey.Representation == EVoxelTerrainRepresentation::VoxelProxy)
	{
		const FVoxelViewKey Key{InKey.Coordinate, InKey.Level};
		const bool bRelevant = GetInputRoot().ProxyWanted.Contains(Key) || VoxelProxyReady.Contains(Key) || VoxelProxyActors.Contains(Key) ||
			VisibleTerrainNodes.Contains(Key) || VoxelProxyData.Contains(Key);
		VoxelProxyRevisions.Remove(Key);
		if (bRelevant) MarkCoverageDirty(VoxelProxyCoverageRect(Key));
		bHeightfieldTopologyDirty |= ProxySurfaceCoverage.Remove(Key) > 0;
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		PreparedVolumeSignatures.Remove(Key);
		PreparedVolumeMeshes.Remove(Key);
	}
	else if (InKey.Representation == EVoxelTerrainRepresentation::Surface)
	{
		const FVoxelSurfaceTileKey Key{FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level};
		const bool bRelevant = GetInputRoot().SurfaceWanted.Contains(Key) || SurfaceReady.Contains(Key) || SurfaceActors.Contains(Key) || SurfaceData.Contains(Key);
		SurfaceRevisions.Remove(Key);
		bHeightfieldTopologyDirty |= SurfaceReady.Remove(Key) > 0;
		if (bRelevant) MarkCoverageDirty(SurfaceCoverageRect(Key));
		SurfaceData.Remove(Key);
		WaterData.Remove(Key);
	}
	else if (InKey.Representation == EVoxelTerrainRepresentation::Macro)
	{
		const FVoxelMacroTileKey Key{FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level};
		const bool bRelevant = GetInputRoot().MacroWanted.Contains(Key) || MacroReady.Contains(Key) || MacroActors.Contains(Key) || MacroData.Contains(Key);
		MacroRevisions.Remove(Key);
		bHeightfieldTopologyDirty |= MacroReady.Remove(Key) > 0;
		if (bRelevant) MarkCoverageDirty(MacroCoverageRect(Key));
		MacroData.Remove(Key);
	}
}

void FVoxelViewManager::InvalidateNeighbors(const FIntVector& InKey)
{
	if (ShouldDeferCoverage(FineCoverageRect(InKey)))
	{
		++CoverageContentRevision;
		DeferredCoverageCallbacks.Enqueue({FineCoverageRect(InKey),
			[this, InKey]
			{
				InvalidateNeighbors(InKey);
			}});
		return;
	}
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	TSet<FIntVector> Neighbors;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (const int32 Sign : {-1, 1})
		{
			FIntVector Key = InKey;
			Key[Axis] += Sign;
			if (!GetInputRoot().FineWanted.Contains(Key) && !FineReady.Contains(Key) && !FineSectionMeshes.Contains(Key))
				continue;
			const bool bRemoved = FineReady.Remove(Key) > 0;
			bReadyTerrainBranchesDirty |= bRemoved;
			if (bRemoved)
				MarkCoverageDirty(FineCoverageRect(Key));
			InvalidateFinePresentation(Key);
			Neighbors.Add(Key);
		}
	}
	if (Neighbors.IsEmpty())
		return;
	Scheduler.CancelMatching(
		[&Neighbors](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildFineMesh && Neighbors.Contains(Stamp.Section);
		});
}

void FVoxelViewManager::Reset()
{
	CancelInterestWorkspace();
	ReleaseInterestWorkspace(CommittedWorkspaceEvents);
	ResetRetirementContinuation();
	while (!FineShardSlots.IsEmpty()) RetireFineShard(FineShardSlots.CreateConstIterator().Key());
	FineShardSlots.Empty();
	FineShardSlotOrder.Empty();
	FineShardAdmissionSelection.Reset();
	for (int32 Stage = 0; Stage < 2; ++Stage)
	{
		TransitionAdmissionSelection[Stage].Reset();
		TransitionAdmissionCursor[Stage] = 0;
		TransitionAdmissionCostMicroseconds[Stage] = 50.0;
	}
	FineShardPresentationSelection.Reset();
	for (auto& Queue : FinePresentationQueues) Queue.Empty();
	FineShardBackingLease.Reset();
	FineShardBackingCapacity = FineShardBackingRejectedCount = 0;
	FineShardBackingReleaseVersion = 0;
	FineShardBackingAdmission = EVoxelTerrainResourceAdmission::Granted;
	FineShardAdmissionCursor = 0;
	FineShardPresentationCursor = 0;
	check(TaskOwnerIncarnation != MAX_uint64);
	++TaskOwnerIncarnation;
	CoverageIndexRetries.Empty();
	CoverageIndexRetryLease.Reset();
	CoverageIndexRetryCapacity = CoverageIndexRetryCursor = 0;
	BindCoverageIndexResources();
	RepresentationAttempts.Reset();
	CoverageContinuation.Reset();
	CoverageInterestSnapshot.Reset();
	bCoverageReadScope = false;
	bCoverageRetryRequested = false;
	ReleaseInputRoot(CoverageInputRoot);
	ReleaseInputRoot(AppliedInputRoot);
	DeferredCoverageResults.Reset();
	DeferredCoverageResultCursor = 0;
	DeferredCoverageCallbacks.Empty();
	GeometryMetrics.Reset();
	PresentedGeometryMetrics.Reset();
	TerrainWorkMetrics = {};
	bTerrainBuildStageDirty = true;
	Scheduler.SetObservedTerrainStage(0);
	++VolumePlanSerial;
	++HeightfieldPlanSerial;
	HeightfieldCoveragePlans.Reset();
	PendingHeightfieldPlanOwners.Reset();
	VolumeCoveragePlans.Reset();
	VolumePlanIndex.Reset();
	PendingVolumeHandoffBoxes.Reset();
	for (const TPair<FVoxelViewKey, TObjectPtr<AActor>>& Pair : VoxelProxyActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : SurfaceActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>>& Pair : WaterActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>>& Pair : MacroActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	for (const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>>& Pair : HeightfieldTransitionActors)
	{
		if (Pair.Value)
			Pair.Value->Destroy();
	}
	PreparedData.Reset();
	PreparedEnvironmentRetentions.Reset();
	PreparedDataBytes = 0;
	PendingDataBytes = 0;
	SkippedProxyMeshes = 0;
	SkippedVolumeMeshes = 0;
	for (auto& Scans : DataScanIndices)
		Scans.Reset();
	for (auto& Scans : DataFrontierScanIndices)
		Scans.Reset();
	FineSectionMeshes.Reset();
	for (int32 Stage = 0; Stage < 2; ++Stage)
	{
		verify(AdmissionSelection[Stage].Reset());
		AdmissionAges[Stage].Empty();
		AdmissionAgeLeases[Stage].Reset();
		AdmissionAgeCapacities[Stage] = 0;
		AdmissionAgeCleanupCursors[Stage] = 0;
		bAdmissionAgeCapacityWaiting[Stage] = false;
		AdmissionAgeCapacityVersions[Stage] = 0;
		bServiceSelectionCapacityWaiting[Stage] = false;
		ServiceSelectionCapacityVersions[Stage] = 0;
		ServiceAdmissionKinds[Stage] = 0;
		for (int32 Kind = 0; Kind < 4; ++Kind)
		{
			ServiceAdmissionScans[Stage][Kind] = 0;
			AdmissionCostEstimates[Stage][Kind] = 0.0;
		}
	}
	GetInputRoot().FinePagePlan = {};
	PresentedFineSectionOwners.Reset();
	DirtyFinePages.Reset();
	PendingFinePages.Reset();
	bFinePresentationDirty = true;
	bHeightfieldTopologyDirty = true;
	FineRevisions.Reset();
	VoxelProxyActors.Reset();
	VoxelProxyRevisions.Reset();
	SurfaceActors.Reset();
	WaterActors.Reset();
	MacroActors.Reset();
	HeightfieldTransitionActors.Reset();
	HeightfieldTransitionSignatures.Reset();
	DesiredHeightfieldInputs.Reset();
	DesiredTransitionSignatures.Reset();
	PendingTransitionSignatures.Reset();
	PreparedTransitionSignatures.Reset();
	PreparedTransitionMeshes.Reset();
	DesiredTransitionEdges.Reset();
	UnsubmittedTransitionOwners.Reset();
	PendingHeightfieldHandoffRects.Reset();
	HeightfieldUnbalancedEdgeCount = 0;
	VolumeTransitionSignatures.Reset();
	DesiredVolumeSignatures.Reset();
	PendingVolumeSignatures.Reset();
	PreparedVolumeSignatures.Reset();
	PreparedVolumeMeshes.Reset();
	PreparedVolumeBoundaryMeshes.Reset();
	for (const auto& Pair : VolumeBoundaryActors)
		if (Pair.Value)
		{
			Publisher->Forget(Pair.Value);
			Pair.Value->Destroy();
		}
	VolumeBoundaryActors.Reset();
	DesiredVolumeContexts.Reset();
	UnsubmittedVolumeOwners.Reset();
	VolumeUnbalancedFaceCount = 0;
	SurfaceRevisions.Reset();
	MacroRevisions.Reset();
	VisibleTerrainNodes.Reset();
	VisibleTerrainNodesByGroup.Reset();
	CoverageIndex.Reset();
	GetInputRoot().TerrainRootIndex.Reset();
	GetInputRoot().IndexedTerrainRoots.Reset();
	HeightfieldCoverageIndex.Reset();
	GetInputRoot().FineWanted.Reset();
	GetInputRoot().ProxyWanted.Reset();
	GetInputRoot().SurfaceWanted.Reset();
	GetInputRoot().MacroWanted.Reset();
	FineReady.Reset();
	VoxelProxyReady.Reset();
	ReadyTerrainBranches.Reset();
	bReadyTerrainBranchesDirty = true;
	SurfaceReady.Reset();
	MacroReady.Reset();
	VoxelProxyData.Reset();
	AirVoxelProxyNodes.Reset();
	ProxySurfaceCoverage.Reset();
	SurfaceData.Reset();
	WaterData.Reset();
	MacroData.Reset();
	FineLastWanted.Reset();
	VoxelProxyLastWanted.Reset();
	SurfaceLastWanted.Reset();
	MacroLastWanted.Reset();
	Admissions = {};
	PendingPublicationInputs.Reset();
	PendingInterestSnapshot.Reset();
	QueuedInterestSnapshot.Reset();
	AppliedInterestSnapshot.Reset();
	PendingInterestRevision = 0;
	bTaskPrioritiesDirty = false;
	InterestInputPhase = InterestInputCursor = 0;
	PendingFinePagePlan = {};
	PendingLayoutInvalidations.Reset();
	PriorityObservers.Reset();
	for (auto& Scans : AdmissionScanIndices)
		Scans.Reset();
	Publisher->Reset();
	OpenPublishGroups.Reset();
	ActiveCoveragePublishGroups.Reset();
	AppliedInterestRevision = 0;
	LastActiveAdmissionKind = 4;
	LastActiveAdmissionPriority = MAX_int32;
	LastActiveDataKind = 4;
	AdmissionPriorities = {};
	LastResolvedFrontier = 0.0;
	MarkCoverageDirty();
	ActiveDirtyCoverageRects.Reset();
	bActiveCoverageFull = true;
	RetirementQueue.Reset();
	RetirementIndices.Reset();
	RetirementCursor = 0;
	NextRetireCheck = 0.0;
	LastRetireMilliseconds = 0.0;
	NextRepresentationDebugLog = 0.0;
}

bool FVoxelViewManager::RequiresSectionData(const FIntVector& InKey) const
{
	return Publisher->GetTerrainRenderer().RequiresF0RuntimeData(InKey,
		[this](const FIntVector& Section, const FVoxelFineOwnerEvidence& Evidence)
		{
			return (Evidence.Owner.Kind == EVoxelFineOwnerKind::F1Plus && IsLatestFinePresented(Section)) ||
				Publisher->IsCurrentAtomEvidence(Section, Evidence);
		});
}

bool FVoxelViewManager::HasPrimaryRepresentation() const
{
	const auto Presented = CapturePresentedSnapshot();
	return Presented && (Presented->GetFineAtomCount() > 0 || Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Proxy) > 0 ||
		Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Surface) > 0 || Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Macro) > 0);
}

FVoxelRepresentationReadiness FVoxelViewManager::GetRepresentationReadiness() const
{
	FVoxelRepresentationReadiness Result;
	const auto Presented = CapturePresentedSnapshot();
	Result.PresentedFine = Presented ? Presented->GetFineAtomCount() : 0;
	Result.PresentedVoxelProxy = Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Proxy) : 0;
	Result.PresentedSurface = Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Surface) : 0;
	Result.PresentedMacro = Presented ? Presented->GetCoarseOwnerCount(EVoxelFineOwnerKind::Macro) : 0;
	return Result;
}

FVoxelTerrainReadinessSnapshot FVoxelViewManager::CaptureTerrainReadiness() const
{
	const FVoxelInterestSet& Interest = GetViewInterest();
	FVoxelTerrainReadinessSnapshot Snapshot;
	Snapshot.WorldEpoch = WorldEpoch;
	Snapshot.PublicationRevision = Interest.PublicationRevision;
	Snapshot.FineLayoutRevision = FinePageLayoutRevision;
	Snapshot.bAdoptingInputs = bConsumingPublicationInputs;
	for (const auto& Pair : Interest.Sources)
	{
		Snapshot.Sources.Add(Pair.Key, Pair.Value.Source);
	}
	Snapshot.Targets.Reserve(Interest.Geometry.Num());
	for (const auto& Pair : Interest.Geometry)
	{
		FVoxelTerrainTargetReadiness& Target = Snapshot.Targets.AddDefaulted_GetRef();
		Target.Geometry = Pair.Value;
		const FVoxelTerrainSpatialKey& Key = Pair.Key;
		AActor* Actor = nullptr;
		if (Key.Representation == EVoxelTerrainRepresentation::Fine)
		{
			const FVoxelFinePageKey Page{Key.Coordinate, Key.Level};
			TArray<FIntVector> Sections;
			FVoxelFinePagePlanner::GatherSections(Page, Sections);
			Target.RequiredSections = Sections.Num();
			Target.bMeshReady = !Sections.IsEmpty();
			Target.bKnownEmpty = !Sections.IsEmpty();
			for (const FIntVector& Section : Sections)
			{
				const FVoxelSection* Data = Module.GetRuntime()->FindSection(Section);
				Target.ReadySections += Data && Data->Status == EVoxelSectionStatus::DataReady ? 1 : 0;
				Target.bFailed |= Data && Data->Status == EVoxelSectionStatus::Failed;
				const bool bPresented = IsLatestFinePresented(Section);
				const bool bPrepared = IsFineGeometryPrepared(Section);
				Target.PresentedSections += bPresented ? 1 : 0;
				Target.bMeshReady &= bPrepared || bPresented;
				Target.bPublishing |= bPrepared && !bPresented;
				const auto* Actual = Publisher->GetTerrainRenderer().FindPresentedAtom(Section);
				Target.bKnownEmpty &= bPresented && Actual && Actual->Evidence.bKnownEmpty;
			}
			Target.bDataReady = Target.RequiredSections > 0 && Target.ReadySections == Target.RequiredSections;
			Target.bPresented = Target.RequiredSections > 0 && Target.PresentedSections == Target.RequiredSections;
			Target.bVisible = Target.bPresented;
		}
		else
		{
			FVoxelViewAdmission Admission;
			uint8 GroupKind = 0;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::VoxelProxy:
				Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
				Admission.ProxyKey = {Key.Coordinate, Key.Level};
				Actor = VoxelProxyActors.FindRef(Admission.ProxyKey);
				GroupKind = 1;
				break;
			case EVoxelTerrainRepresentation::Surface:
				Admission.Kind = EVoxelViewAdmissionKind::Surface;
				Admission.SurfaceKey = {FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
				Actor = SurfaceActors.FindRef(Admission.SurfaceKey);
				GroupKind = 2;
				break;
			case EVoxelTerrainRepresentation::Macro:
				Admission.Kind = EVoxelViewAdmissionKind::Macro;
				Admission.MacroKey = {FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
				Actor = MacroActors.FindRef(Admission.MacroKey);
				GroupKind = 3;
				break;
			default:
				continue;
			}
			Target.bDataReady = IsAdmissionDataReady(Admission);
			Target.bMeshReady = IsAdmissionMeshReady(Admission);
			Target.bPresented = IsAdmissionPresented(Admission);
			Target.bFailed = IsAdmissionTerminalFailure(Admission);
			Target.bPublishing = Publisher->IsGroupBusy({GroupKind, Key.Coordinate, Key.Level});
			if (const FVoxelTerrainGeometryMetrics* Metrics = GeometryMetrics.Find(Key))
			{
				Target.bKnownEmpty = Target.bMeshReady && Metrics->Geometry == Target.Geometry && Metrics->bGenerated && Metrics->Triangles == 0;
			}
		}
		if (Key.Representation != EVoxelTerrainRepresentation::Fine)
			Target.bVisible = Target.bMeshReady && Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor);
	}
	return Snapshot;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetPrimaryFineReadiness(const TMap<FIntVector, FVoxelExactDemand>& InExact) const
{
	FVoxelPrimaryFineReadiness Result;
	const auto& Renderer = Publisher->GetTerrainRenderer();
	for (const auto& Pair : InExact)
	{
		if (!Pair.Value.bWarmupData || !Pair.Value.bFineRender) continue;
		++Result.Required;
		const bool bPrepared = IsFineGeometryPrepared(Pair.Key);
		const bool bPresented = IsLatestFinePresented(Pair.Key);
		const auto* Actual = Renderer.FindPresentedAtom(Pair.Key);
		Result.Ready += bPrepared;
		Result.Owned += bPresented;
		Result.Presented += bPresented;
		Result.Renderable += bPresented && Actual && !Actual->Evidence.bKnownEmpty;
		Result.PublicationPending += Renderer.IsLocked(Pair.Key);
		for (const int32 Side : {16, 32, 64})
		{
			FIntVector Min;
			if (!FVoxelFineSpatial::TrySnap(Pair.Key * 16, Side, Min)) continue;
			if (const auto* Slot = FineShardSlots.Find({Min, Side})) Result.bFailed |= !Slot->Build.GetFailure().IsEmpty();
		}
	}
	return Result;
}

FVoxelPrimaryFineReadiness FVoxelViewManager::GetFineRadiusReadiness(TConstArrayView<FIntVector> InFineKeys) const
{
	FVoxelPrimaryFineReadiness Result;
	const auto& Renderer = Publisher->GetTerrainRenderer();
	for (const FIntVector& Key : InFineKeys)
	{
		++Result.Required;
		const bool bPresented = IsLatestFinePresented(Key);
		const auto* Actual = Renderer.FindPresentedAtom(Key);
		Result.Ready += IsFineGeometryPrepared(Key);
		Result.Owned += bPresented;
		Result.Presented += bPresented;
		Result.Renderable += bPresented && Actual && !Actual->Evidence.bKnownEmpty;
		Result.PublicationPending += Renderer.IsLocked(Key);
		for (const int32 Side : {16, 32, 64})
		{
			FIntVector Min;
			if (!FVoxelFineSpatial::TrySnap(Key * 16, Side, Min)) continue;
			if (const auto* Slot = FineShardSlots.Find({Min, Side})) Result.bFailed |= !Slot->Build.GetFailure().IsEmpty();
		}
	}
	return Result;
}
void FVoxelViewManager::AdvanceInterestInputs(const uint64 InRevision, TConstArrayView<FVector> InObservers)
{
	const double Deadline = TerrainDeadline(1.0);
	AdvanceCommittedInterestEvents(Deadline);
	if (CommittedWorkspaceEvents) return;
	const auto Latest = Module.GetDesiredInterestSnapshot();
	if (!Latest) return;
	QueuedInterestSnapshot = Latest;
	if (InterestWorkspace && InterestWorkspace->Snapshot->PublicationRevision != Latest->PublicationRevision)
		CancelInterestWorkspace();
	if (!InterestWorkspace && (!AppliedInterestSnapshot || AppliedInterestRevision != Latest->PublicationRevision ||
		Latest != Module.GetCurrentInterestSnapshot()))
	{
		if (!BeginInterestWorkspace(QueuedInterestSnapshot)) return;
		PriorityObservers = TArray<FVector>(InObservers);
	}
	AdvanceInterestWorkspace(Deadline);
	TryCommitInterestRoot();
	AdvanceCommittedInterestEvents(Deadline);
}
bool FVoxelViewManager::TryResolveFineWithoutMesh(const FIntVector& InSection, const uint64 InRevision)
{
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(InSection);
	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady || Section->CommittedRevision != InRevision ||
		Section->Blocks.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide || !Registry)
	{
		return false;
	}

	auto IsOccludingFullCube = [&Registry](const FVoxelBlockState& Block)
	{
		if (Block.IsAir())
		{
			return false;
		}
		const FVoxelRuntimeDefinition* Definition = Registry->Find(Block.TypeId);
		return Definition && Definition->Shape == EVoxelShapeKind::FullCube && Definition->bOccludes;
	};

	bool bAllAir = true;
	bool bFullyOccluding = true;
	for (const FVoxelBlockState& Block : Section->Blocks)
	{
		if (Block.IsAir())
		{
			bFullyOccluding = false;
			continue;
		}
		bAllAir = false;
		if (!IsOccludingFullCube(Block))
		{
			bFullyOccluding = false;
			break;
		}
	}

	if (!bAllAir && !bFullyOccluding)
	{
		return false;
	}

	if (bFullyOccluding)
	{
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 UAxis = (Axis + 1) % 3;
			const int32 VAxis = (Axis + 2) % 3;
			FIntVector NeighborKey = InSection;
			NeighborKey[Axis] += Face % 2 == 0 ? 1 : -1;
			const FVoxelSection* Neighbor = Module.GetRuntime()->FindSection(NeighborKey);
			if (!Neighbor || Neighbor->Status != EVoxelSectionStatus::DataReady ||
				Neighbor->Blocks.Num() != ViewSectionSide * ViewSectionSide * ViewSectionSide)
			{
				return false;
			}

			for (int32 V = 0; V < ViewSectionSide; ++V)
			{
				for (int32 U = 0; U < ViewSectionSide; ++U)
				{
					FIntVector Local = FIntVector::ZeroValue;
					Local[Axis] = Face % 2 == 0 ? 0 : ViewSectionSide - 1;
					Local[UAxis] = U;
					Local[VAxis] = V;
					const int32 Index = Local.X + ViewSectionSide * (Local.Y + ViewSectionSide * Local.Z);
					if (!IsOccludingFullCube(Neighbor->Blocks[Index]))
					{
						return false;
					}
				}
			}
		}
	}

	FVoxelTaskResult Result;
	Result.Kind = EVoxelTaskKind::BuildFineMesh;
	Result.Stamp.WorldEpoch = WorldEpoch;
	Result.Stamp.Token = Section->Stamp.Token;
	Result.Stamp.Revision = InRevision;
	Result.Stamp.Section = InSection;
	Result.bSuccess = true;
	Result.FineMesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	const bool bPublished = PublishFine(Result);
	if (bPublished)
		MarkCoverageDirty(FineCoverageRect(InSection));
	return bPublished;
}
bool FVoxelViewManager::RequestFine(const FIntVector& InSection, const uint64 InRevision)
{
	return RequestF0Fine(InSection, InRevision);
}
bool FVoxelViewManager::BuildVoxelProxySnapshot(
	const FVoxelVoxelProxyData& InData, const uint64 InRecipeHash, FVoxelSectionSnapshot& OutSnapshot, FString& OutError)
{
	if (!InData.Geometry.Validate(OutError) ||
		InData.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InData.Key.Coordinate, InData.Key.Level} ||
		InData.Key.Level == 0 || InData.GridSide <= 0 || InData.GridSide > 128 || InData.LocalMinCells != FIntVector::ZeroValue ||
		InData.GridSide != InData.Key.GetSide() / InData.Geometry.SampleStepCells ||
		int64(InData.Cells.Num()) != int64(InData.GridSide) * InData.GridSide * InData.GridSide)
	{
		OutError = TEXT("Voxel proxy snapshot has invalid cell data");
		return false;
	}

	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = InData.GridSide;
	Snapshot.Section = InData.Key.Coordinate;
	Snapshot.Stamp.RecipeHash = InRecipeHash;
	Snapshot.Revision = InData.Revision;
	Snapshot.Blocks.Reserve(InData.Cells.Num());
	for (const FVoxelBlockState State : InData.Cells)
		Snapshot.Blocks.Add(State.Pack());

	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Known[Face] = InData.Known[Face];
		if (!Snapshot.Known[Face])
			continue;
		const TArray<FVoxelBlockState>& Source = InData.Halo[Face];
		if (Source.Num() != InData.GridSide * InData.GridSide)
		{
			OutError = TEXT("Voxel proxy snapshot has invalid halo data");
			return false;
		}
		Snapshot.Halo[Face].Reserve(Source.Num());
		for (const FVoxelBlockState State : Source)
			Snapshot.Halo[Face].Add(State.Pack());
	}

	OutSnapshot = MoveTemp(Snapshot);
	OutError.Reset();
	return true;
}

bool FVoxelViewManager::RequestVoxelProxy(const FVoxelViewKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	FString GeometryError;
	if (!Geometry.Validate(GeometryError) || InKey.Level == 0 ||
		Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level}) return false;
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();
	if (!Config || !Config->Recipe)
		return false;
	const int32 Step = Geometry.SampleStepCells;
	const FVoxelTreeGenerationSettings& Tree = Config->Recipe->Settings.Ecology.Tree;
	const int32 Crown = Tree.bEnabled ? Tree.CrownRadius : 0;
	const int32 TreeHeight = Tree.bEnabled ? Tree.MaxHeight + Crown : 0;
	const int32 SampleStep = Geometry.SampleStepCells;
	const int32 TreePadding = Tree.bEnabled ? (FMath::Max(1, (Crown - 1) / SampleStep) + 1) * SampleStep : 0;
	const int32 HorizontalMargin = FMath::Max(Step + Crown, TreePadding);
	FVoxelGenerationBounds OverlayBounds;
	if (!VoxelStreamingGeometry::TrySamplingBounds(Geometry,
		FIntVector(HorizontalMargin, HorizontalMargin, Step + TreeHeight), FIntVector(HorizontalMargin, HorizontalMargin, Step),
		OverlayBounds.Min, OverlayBounds.Max, GeometryError)) return false;
	if (!Module.IsAuthority())
	{
		const bool bModified = Module.GetRuntime()->GetChangeIndex().Count(OverlayBounds) > 0;

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.ViewKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(InKey);
		RemoteDataStamp = MakeRepresentationStamp(Geometry, RemoteDataStamp.Revision);
		if (bModified && !PreparedData.Contains({EVoxelTaskKind::GenerateVoxelProxy, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = VoxelProxyRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(EVoxelRepresentationWireType::VoxelProxy, {InKey.Coordinate, InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache = Module.GetGenerationCache();
	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	if (!Config || !Cache)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildVoxelProxy;

	Request.TerrainStage = VoxelProxyTerrainStage(InKey);
	Request.WorkClass = Request.TerrainStage == 0 ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(InKey.Coordinate), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.ViewKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(InKey);

	Request.Stamp = MakeRepresentationStamp(Geometry, Request.Stamp.Revision);
	Request.GenerationConsumer = MakeCoarseGenerationConsumer(Geometry, Request.Stamp);
	if (!Request.GenerationConsumer.IsValid()) return false;
	FVoxelGenerationConsumerScope ConsumerScope(Request.GenerationConsumer);

	Request.ReservedBytes = 1;
	const FVoxelGenerationBounds ProxyBounds = InKey.GetBounds();
	Request.DistanceScore = GetViewInterest().TerrainPlan.FineDependencies.Contains(InKey) ? 0.0 : MinimumObserverDistanceCells(ProxyBounds);
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind) || HasDeferredRepresentation(Request.Stamp))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateVoxelProxy, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		Request.ReservedBytes = 1;
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = FindPreparedData(DataKey);
		if (!Prepared || !Prepared->VoxelProxy || Prepared->VoxelProxy->Geometry != Geometry)
			return false;
		if (Prepared->bKnownEmptyMesh)
		{
			FVoxelTaskResult Empty;
			Empty.Stamp = Request.Stamp;
			if (Prepared->OverlayReadResult && !Module.IsOverlayReadCurrent(*Prepared->OverlayReadResult))
			{
				RemovePreparedData(DataKey);
				return false;
			}
			Empty.OverlayReadResult = Prepared->OverlayReadResult;
			Empty.VoxelProxy = Prepared->VoxelProxy;
			if (!Prepared->VoxelProxy->ResourceLease) return false;
			Empty.VoxelProxyMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(Prepared->VoxelProxy->ResourceLease);
			Empty.VoxelProxyMesh->ResourceLease = Prepared->VoxelProxy->ResourceLease;
			Empty.VoxelProxyMesh->InputWorkReceipt = Prepared->WorkReceipt;
			Empty.VoxelProxyMesh->FrozenVoxelProxy = Prepared->VoxelProxy;
			Empty.VoxelProxyMesh->ValidateAndCache();
			if (!PublishVoxelProxy(Empty))
				return false;
			++SkippedProxyMeshes;
			MarkCoverageDirty(VoxelProxyCoverageRect(InKey));
			RemovePreparedData(DataKey);
			return true;
		}
	}

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	const TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes = Module.GetShapes();

	const uint64 RecipeHash = Module.GetManifest().RecipeHash;
	FVoxelFineOverlayCapture OverlayCapture;
	FString OverlayError;
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !EnvironmentRetention) return false;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureFineOverlays(OverlayBounds, OverlayCapture, OverlayError, &Request.GenerationConsumer))
		return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : OverlayCapture.AllocatedBytes();

	const auto BuildStorage = CreateCoarseBuildStorage(Scheduler, Request, Geometry,
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelProxyPageBuild>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelProxyPageMesh>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FProxyMeshEstimate>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FRepresentationOverlayRead>(), 1, bDataOnly ? 2 : 1);
    if (!BuildStorage) return false;
	BuildStorage->WorkingCapacityBytes = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
	const uint64 GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
    TSharedPtr<FProxyMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelProxyPageBuild, ESPMode::ThreadSafe> PageBuild;
	TSharedPtr<FRepresentationOverlayRead, ESPMode::ThreadSafe> OverlayRead;
	if (bDataOnly)
	{
		PageBuild = VoxelBoundedAllocation::MakeWorkerShared<FVoxelProxyPageBuild>(BuildStorage->MetadataLease);
		FString Error;
		MeshEstimate = VoxelBoundedAllocation::MakeWorkerShared<FProxyMeshEstimate>(BuildStorage->MetadataLease);
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true, BuildStorage->WorkingCapacityBytes, GeometryLimit))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			UE_LOG(LogTemp, Warning, TEXT("Proxy page retained after capacity failure: %s"), *Error);
			return false;
		}
		OverlayRead = VoxelBoundedAllocation::MakeWorkerShared<FRepresentationOverlayRead>(BuildStorage->MetadataLease);
        OverlayRead->MetadataLease = BuildStorage->ResultInputLease;
        OverlayRead->FactResources = Scheduler.GetRuntimeResources();
        OverlayRead->FactConsumer = BuildStorage->Consumer;
        {
			FString CallbackBindingError;
			if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[BuildStorage, OverlayRead]
        {
            if (OverlayRead->IsBlocked())
                return EVoxelTaskBlockReason::Memory;
            return BuildStorage->BlockReason();
        }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				return false;
			}
        }
		if (!OverlayRead->Reader.Initialize(MoveTemp(OverlayCapture), Error))
		{
			return false;
		}
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = PageBuild->MaximumRetainedBytes() + 4 * VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelWaterSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelMacroTileData>();
		Request.ReservedBytes = 1;
	}
	auto PageMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelProxyPageMesh>(BuildStorage->MetadataLease);
	if (!bDataOnly)
	{
		FString Error;
		const uint64 RetainedBytes = Prepared->VoxelProxy->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes, false, BuildStorage->WorkingCapacityBytes, GeometryLimit))
			return false;
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = RetainedBytes + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>();
		Request.ReservedBytes = 1;
	}
	if (!AdmitCoarseCompletion(Scheduler, Request, BuildStorage)) return false;
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[BuildStorage, bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, Config, Cache, Generator, EnvironmentRetention, Registry, Shapes, RecipeHash,
						  OverlayRead, InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
		if (bDataOnly) OverlayRead->Begin();
		if (!BuildStorage->Begin(Result, WorkingLease)) return Result;
		Result.InputResourceLease = BuildStorage->ResultInputLease;
		if (bDataOnly && !OverlayRead->Advance(Result, InCancel))
		{
			return Result;
		}
		if (!bDataOnly)
		{
			Result.OverlayReadResult = Prepared->OverlayReadResult;
		}

		if (bDataOnly)
		{
			if (MeshEstimate->Data)
			{
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess = MeshEstimate->Counter.Advance(*MeshEstimate->Data, *Registry, *Shapes, nullptr,
					Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Geometry.SampleStepCells, TextureStretch), CountedMesh, bComplete, Result.Error,
					&InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Data->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.VoxelProxy = MeshEstimate->Data;
				Result.bKnownEmptyMesh = Result.VoxelProxy->EstimatedMeshBytes == 0;
				BuildStorage->SettleCompleted(Result);
				return Result;
			}
			Result.VoxelProxy = BuildStorage->MakeResult<FVoxelVoxelProxyData>();

			const FVoxelVoxelProxyBuilder Builder(Config.ToSharedRef(), Cache.ToSharedRef(), Generator);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, *OverlayRead->Result->Snapshot, *Result.VoxelProxy, bComplete, Result.Error, &InCancel);
			if (Result.VoxelProxy) Result.VoxelProxy->ResourceLease = BuildStorage->GeometryLease;
			if (!Result.bSuccess && !InCancel.Load() && OverlayRead->DeferFacts(Result))
			{
				Result.VoxelProxy.Reset();
				return Result;
			}
			if (Result.bSuccess && !bComplete)
			{
				Result.VoxelProxy.Reset();
				Result.bContinue = true;
				return Result;
			}

			if (Result.bSuccess)
			{
				MeshEstimate->Data = Result.VoxelProxy;
				Result.VoxelProxy.Reset();
				Result.bContinue = true;
			}
			return Result;
		}
		Result.bSuccess = true;
		Result.VoxelProxy = Prepared->VoxelProxy;

		if (!Result.bSuccess || !Registry || !Shapes)
		{
			return Result;
		}

		Result.VoxelProxyMesh = BuildStorage->MakeResult<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(*Result.VoxelProxy, *Registry, *Shapes, nullptr,
			Geometry.SampleStepCells / VoxelViewLod::TexturePeriodCells(Geometry.SampleStepCells, TextureStretch), *Result.VoxelProxyMesh, bComplete,
			Result.Error, &InCancel);
		if (Result.VoxelProxyMesh) Result.VoxelProxyMesh->ResourceLease = BuildStorage->GeometryLease;
		if (Result.bSuccess && bComplete)
		{
			Result.VoxelProxyMesh->InputWorkReceipt = Prepared->WorkReceipt;
			Result.VoxelProxyMesh->FrozenVoxelProxy = Prepared->VoxelProxy;
			BuildStorage->SettleCompleted(Result);
		}
		if (Result.bSuccess && !bComplete)
		{
			Result.VoxelProxy.Reset();
			Result.VoxelProxyMesh.Reset();
			Result.bContinue = true;
		}

		return Result;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestSurface(const FVoxelSurfaceTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	FString GeometryError;
	if (!Geometry.Validate(GeometryError) ||
		Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}) return false;
	if (!Module.IsAuthority())
	{
		const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, InKey.Level);

		const FIntPoint Min = InKey.Coordinate * Side;

		const FVoxelGenerationSettings& Settings = Module.GetManifest().Settings;

		const bool bModified = Module.GetRuntime()->GetChangeIndex().Count(
			{FIntVector(Min.X, Min.Y, Settings.MinZ), FIntVector(Min.X + Side, Min.Y + Side, Settings.MaxZ)}) > 0;

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.SurfaceKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(InKey);
		RemoteDataStamp = MakeRepresentationStamp(Geometry, RemoteDataStamp.Revision);
		if (bModified && !PreparedData.Contains({EVoxelTaskKind::GenerateSurface, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = SurfaceRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::SurfaceProxy, {FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	if (!Generator || !Config || !Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildSurface;

	Request.WorkClass = EVoxelWorkClass::Exploration;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.SurfaceKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(InKey);

	Request.Stamp = MakeRepresentationStamp(Geometry, Request.Stamp.Revision);
	Request.GenerationConsumer = MakeCoarseGenerationConsumer(Geometry, Request.Stamp);
	if (!Request.GenerationConsumer.IsValid()) return false;
	FVoxelGenerationConsumerScope ConsumerScope(Request.GenerationConsumer);

	Request.ReservedBytes = 1;
	Request.DistanceScore = MinimumObserverDistanceCells(SurfaceWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind) || HasDeferredRepresentation(Request.Stamp))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateSurface, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = FindPreparedData(DataKey);
		if (!Prepared || !Prepared->Surface || Prepared->Surface->Geometry != Geometry)
			return false;
	}

	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	FVoxelFineOverlayCapture OverlayCapture;
	const int32 Step = Geometry.SampleStepCells;
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled ? Settings.Ecology.Tree.CrownRadius : 0);
	FVoxelGenerationBounds OverlayBounds;
	const FIntVector Padding(Margin, Margin, 0);
	if (!VoxelStreamingGeometry::TrySamplingBounds(Geometry, Padding, Padding, OverlayBounds.Min, OverlayBounds.Max, GeometryError)) return false;
	FString OverlayError;
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, OverlayBounds) : nullptr;
	if (bDataOnly && !EnvironmentRetention) return false;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(OverlayBounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(OverlayBounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureFineOverlays(OverlayBounds, OverlayCapture, OverlayError, &Request.GenerationConsumer))
		return false;
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : OverlayCapture.AllocatedBytes();

	const auto BuildStorage = CreateCoarseBuildStorage(Scheduler, Request, Geometry,
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelHeightfieldPageBuild>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelHeightfieldPageMesh>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FHeightfieldMeshEstimate>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FRepresentationOverlayRead>(), 1, bDataOnly ? 2 : 1);
    if (!BuildStorage) return false;
	BuildStorage->WorkingCapacityBytes = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
	const uint64 GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
    TSharedPtr<FHeightfieldMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe> PageBuild;
	TSharedPtr<FRepresentationOverlayRead, ESPMode::ThreadSafe> OverlayRead;
	if (bDataOnly)
	{
		PageBuild = VoxelBoundedAllocation::MakeWorkerShared<FVoxelHeightfieldPageBuild>(BuildStorage->MetadataLease);
		FString Error;
		MeshEstimate = VoxelBoundedAllocation::MakeWorkerShared<FHeightfieldMeshEstimate>(BuildStorage->MetadataLease);
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true, BuildStorage->WorkingCapacityBytes, GeometryLimit))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		OverlayRead = VoxelBoundedAllocation::MakeWorkerShared<FRepresentationOverlayRead>(BuildStorage->MetadataLease);
        OverlayRead->MetadataLease = BuildStorage->ResultInputLease;
        OverlayRead->FactResources = Scheduler.GetRuntimeResources();
        OverlayRead->FactConsumer = BuildStorage->Consumer;
        {
			FString CallbackBindingError;
			if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[BuildStorage, OverlayRead]
        {
            if (OverlayRead->IsBlocked())
                return EVoxelTaskBlockReason::Memory;
            return BuildStorage->BlockReason();
        }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				return false;
			}
        }
		if (!OverlayRead->Reader.Initialize(MoveTemp(OverlayCapture), Error))
		{
			return false;
		}
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = PageBuild->MaximumRetainedBytes() + 4 * VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelWaterSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelMacroTileData>();
		Request.ReservedBytes = 1;
	}
	TSharedPtr<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe> PageMesh;
	if (!bDataOnly)
	{
		PageMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelHeightfieldPageMesh>(BuildStorage->MetadataLease);
		FString Error;
		const uint64 RetainedBytes = Prepared->Surface->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes, false, BuildStorage->WorkingCapacityBytes, GeometryLimit))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = RetainedBytes + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>();
		Request.ReservedBytes = 1;
	}
	if (!AdmitCoarseCompletion(Scheduler, Request, BuildStorage)) return false;
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[BuildStorage, bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, OverlayRead, Generator, EnvironmentRetention, Config, Registry,
						  Settings, InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
		if (bDataOnly) OverlayRead->Begin();
		if (!BuildStorage->Begin(Result, WorkingLease)) return Result;
		Result.InputResourceLease = BuildStorage->ResultInputLease;
		if (bDataOnly && !OverlayRead->Advance(Result, InCancel))
		{
			return Result;
		}
		if (!bDataOnly)
		{
			Result.OverlayReadResult = Prepared->OverlayReadResult;
		}

		if (bDataOnly)
		{
			if (MeshEstimate->Surface)
			{
				if (!MeshEstimate->bCompacted)
				{
					if (!BuildStorage->CompactGeometryArray(MeshEstimate->Surface->DistantCells,
						PageBuild->MaximumDistantBytes(), WorkingLease, Result)) return Result;
					MeshEstimate->bCompacted = true;
				}
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess = MeshEstimate->Counter.Advance(
					*MeshEstimate->Surface, *MeshEstimate->Water, *Config, *Registry, TextureStretch, CountedMesh, bComplete, Result.Error, &InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Surface->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.Surface = MeshEstimate->Surface;
				Result.Water = MeshEstimate->Water;
				BuildStorage->SettleCompleted(Result);
				return Result;
			}
			Result.Surface = BuildStorage->MakeResult<FVoxelSurfaceTileData>();

			const FVoxelSurfaceProxyBuilder Builder(Generator.ToSharedRef(), Config.ToSharedRef(), Settings, *OverlayRead->Result->Snapshot, Registry.ToSharedRef(), Lod);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, *Result.Surface, bComplete, Result.Error, &InCancel);
			if (Result.Surface) Result.Surface->ResourceLease = BuildStorage->GeometryLease;
			if (!Result.bSuccess && !InCancel.Load() && OverlayRead->DeferFacts(Result))
			{
				Result.Surface.Reset();
				return Result;
			}
			if (Result.bSuccess && !bComplete)
			{
				Result.Surface.Reset();
				Result.bContinue = true;
				return Result;
			}

			if (!Result.bSuccess)
				return Result;
			Result.Water = BuildStorage->MakeResult<FVoxelWaterSurfaceTileData>();
			FVoxelWaterViewBuilder WaterBuilder;
			Result.bSuccess = WaterBuilder.Build(*Result.Surface, *Result.Water, Result.Error);
			if (Result.Water) Result.Water->ResourceLease = BuildStorage->GeometryLease;
			if (Result.Surface) Result.Surface->ResourceLease = BuildStorage->GeometryLease;
			if (!Result.bSuccess)
			{
				return Result;
			}
			MeshEstimate->Surface = Result.Surface;
			MeshEstimate->Water = Result.Water;
			Result.Surface.Reset();
			Result.Water.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.SurfaceMesh = BuildStorage->MakeResult<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(
			*Prepared->Surface, *Prepared->Water, *Config, *Registry, TextureStretch, *Result.SurfaceMesh, bComplete, Result.Error, &InCancel);
		if (Result.SurfaceMesh) Result.SurfaceMesh->ResourceLease = BuildStorage->GeometryLease;
		if (Result.bSuccess && bComplete)
		{
			Result.SurfaceMesh->InputWorkReceipt = Prepared->WorkReceipt;
			Result.SurfaceMesh->FrozenSurface = Prepared->Surface;
			Result.SurfaceMesh->FrozenWater = Prepared->Water;
			BuildStorage->SettleCompleted(Result);
		}
		if (Result.bSuccess && !bComplete)
		{
			Result.SurfaceMesh.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.Surface = Prepared->Surface;
		Result.Water = Prepared->Water;

		return Result;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::RequestMacro(const FVoxelMacroTileKey& InKey, const int32 InPriority, const bool bDataOnly)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InKey);
	if (!DesiredGeometry)
		return false;
	const FVoxelTerrainGeometry Geometry = *DesiredGeometry;
	FString GeometryError;
	if (!Geometry.Validate(GeometryError) ||
		Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}) return false;
	if (!Module.IsAuthority())
	{
		const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, InKey.Level);

		const FIntPoint Min = InKey.Coordinate * Side;

		const FVoxelGenerationSettings& Settings = Module.GetManifest().Settings;

		const bool bModified = Module.GetRuntime()->GetChangeIndex().Count(
			{FIntVector(Min.X, Min.Y, Settings.MinZ), FIntVector(Min.X + Side, Min.Y + Side, Settings.MaxZ)}) > 0;

		FVoxelTaskStamp RemoteDataStamp;
		RemoteDataStamp.WorldEpoch = WorldEpoch;
		RemoteDataStamp.MacroKey = InKey;
		RemoteDataStamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(InKey);
		RemoteDataStamp = MakeRepresentationStamp(Geometry, RemoteDataStamp.Revision);
		if (bModified && !PreparedData.Contains({EVoxelTaskKind::GenerateMacro, RemoteDataStamp}))
		{
			if (bDataOnly)
				return false;
			const uint64* Revision = MacroRevisions.Find(InKey);

			return Module.RequestRemoteRepresentation(
				EVoxelRepresentationWireType::MacroTerrain, {FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, Revision ? *Revision : 0);
		}
	}

	const TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator = Module.GetGenerator();

	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config = Module.GetGenerationConfig();

	const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();

	if (!Generator || !Config || !Registry)
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.SourcePriority = InPriority;

	Request.Kind = EVoxelTaskKind::BuildMacro;

	Request.WorkClass = EVoxelWorkClass::Background;

	const bool bCriticalDependency =
		IsCriticalDependency(FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level});
	if (bCriticalDependency)
	{
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.TerrainStage = 0;
	}
	Request.Stamp.WorldEpoch = WorldEpoch;

	Request.Stamp.MacroKey = InKey;

	Request.Stamp.Revision = Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(InKey);

	Request.Stamp = MakeRepresentationStamp(Geometry, Request.Stamp.Revision);
	Request.GenerationConsumer = MakeCoarseGenerationConsumer(Geometry, Request.Stamp);
	if (!Request.GenerationConsumer.IsValid()) return false;
	FVoxelGenerationConsumerScope ConsumerScope(Request.GenerationConsumer);

	Request.ReservedBytes = 1;
	Request.DistanceScore = MinimumObserverDistanceCells(MacroWorldCenter(InKey));
	Request.ForwardScore = 0.0;

	if (Scheduler.Has(Request.Stamp, Request.Kind) || HasDeferredRepresentation(Request.Stamp))
	{
		return false;
	}

	const FVoxelTaskKey DataKey{EVoxelTaskKind::GenerateMacro, Request.Stamp};
	TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe> Prepared;
	if (bDataOnly)
	{
		if (PreparedData.Contains(DataKey) || Scheduler.Has(Request.Stamp, DataKey.Kind))
			return false;
		Request.Kind = DataKey.Kind;
		Request.WorkClass = bCriticalDependency ? EVoxelWorkClass::Critical : EVoxelWorkClass::Prefetch;
	}
	else
	{
		Prepared = FindPreparedData(DataKey);
		if (!Prepared || !Prepared->Macro || Prepared->Macro->Geometry != Geometry)
			return false;
	}

	const FVoxelGenerationSettings Settings = Module.GetManifest().Settings;
	const int32 Step = Geometry.SampleStepCells;
	const int32 Margin = FMath::Max(Step, Settings.Ecology.Tree.bEnabled ? Settings.Ecology.Tree.CrownRadius : 0);
	FVoxelFineOverlayCapture OverlayCapture;
	FString OverlayError;
	FVoxelGenerationBounds Bounds;
	const FIntVector Padding(Margin, Margin, 0);
	if (!VoxelStreamingGeometry::TrySamplingBounds(Geometry, Padding, Padding, Bounds.Min, Bounds.Max, GeometryError)) return false;
	Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Dependency, Request.InputBytes, Request.ReservedBytes);
	const auto EnvironmentRetention = bDataOnly ? RetainPreparedEnvironment(DataKey, *Generator, Bounds) : nullptr;
	if (bDataOnly && !EnvironmentRetention) return false;
	if (bDataOnly && !Module.EnsureEnvironmentBounds(Bounds, Request.WorkClass, InPriority, Request.DistanceScore, Request.ForwardScore).bReady)
		return false;
	if (bDataOnly && !Generator->EnsureCoarseOverlayBoundsReady(Bounds, Request.WorkClass, InPriority).bReady)
		return false;
	if (bDataOnly && !Module.CaptureFineOverlays(Bounds, OverlayCapture, OverlayError, &Request.GenerationConsumer))
	{
		return false;
	}
	Request.InputBytes = Prepared ? Prepared->ResultBytes() : OverlayCapture.AllocatedBytes();
	const auto BuildStorage = CreateCoarseBuildStorage(Scheduler, Request, Geometry,
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelHeightfieldPageBuild>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FVoxelHeightfieldPageMesh>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FHeightfieldMeshEstimate>() +
        VoxelBoundedAllocation::WorkerSharedBytes<FRepresentationOverlayRead>(), 1, bDataOnly ? 2 : 1);
    if (!BuildStorage) return false;
	BuildStorage->WorkingCapacityBytes = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
	const uint64 GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
    TSharedPtr<FHeightfieldMeshEstimate, ESPMode::ThreadSafe> MeshEstimate;
	TSharedPtr<FVoxelHeightfieldPageBuild, ESPMode::ThreadSafe> PageBuild;
	TSharedPtr<FRepresentationOverlayRead, ESPMode::ThreadSafe> OverlayRead;
	if (bDataOnly)
	{
		PageBuild = VoxelBoundedAllocation::MakeWorkerShared<FVoxelHeightfieldPageBuild>(BuildStorage->MetadataLease);
		FString Error;
		MeshEstimate = VoxelBoundedAllocation::MakeWorkerShared<FHeightfieldMeshEstimate>(BuildStorage->MetadataLease);
		if (!PageBuild->Initialize(Geometry, Error) || !MeshEstimate->Counter.Initialize(Geometry, Error, 0, true, BuildStorage->WorkingCapacityBytes, GeometryLimit))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		OverlayRead = VoxelBoundedAllocation::MakeWorkerShared<FRepresentationOverlayRead>(BuildStorage->MetadataLease);
        OverlayRead->MetadataLease = BuildStorage->ResultInputLease;
        OverlayRead->FactResources = Scheduler.GetRuntimeResources();
        OverlayRead->FactConsumer = BuildStorage->Consumer;
        {
			FString CallbackBindingError;
			if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[BuildStorage, OverlayRead]
        {
            if (OverlayRead->IsBlocked())
                return EVoxelTaskBlockReason::Memory;
            return BuildStorage->BlockReason();
        }, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				return false;
			}
        }
		if (!OverlayRead->Reader.Initialize(MoveTemp(OverlayCapture), Error))
		{
			return false;
		}
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = PageBuild->MaximumRetainedBytes() + 4 * VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelWaterSurfaceTileData>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelMacroTileData>();
		Request.ReservedBytes = 1;
	}
	TSharedPtr<FVoxelHeightfieldPageMesh, ESPMode::ThreadSafe> PageMesh;
	if (!bDataOnly)
	{
		PageMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelHeightfieldPageMesh>(BuildStorage->MetadataLease);
		FString Error;
		const uint64 RetainedBytes = Prepared->Macro->EstimatedMeshBytes + 64ull * 1024ull;
		if (!PageMesh->Initialize(Geometry, Error, RetainedBytes, false, BuildStorage->WorkingCapacityBytes, GeometryLimit))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Oversized);
			return false;
		}
		Request.bResumable = true;
		BuildStorage->GeometryCapacityBytes = RetainedBytes + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>();
		Request.ReservedBytes = 1;
	}
	if (!AdmitCoarseCompletion(Scheduler, Request, BuildStorage)) return false;
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[BuildStorage, bDataOnly, PageBuild, MeshEstimate, PageMesh, Prepared, OverlayRead, Settings, Generator, EnvironmentRetention, Config,
						  Registry, InKey, Geometry, TextureStretch = Module.GetViewSettings().MaximumTextureStretchCells,
						  Lod = Module.GetViewSettings().Lod](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
		if (bDataOnly) OverlayRead->Begin();
		if (!BuildStorage->Begin(Result, WorkingLease)) return Result;
		Result.InputResourceLease = BuildStorage->ResultInputLease;
		if (bDataOnly && !OverlayRead->Advance(Result, InCancel))
		{
			return Result;
		}
		if (!bDataOnly)
		{
			Result.OverlayReadResult = Prepared->OverlayReadResult;
		}

		if (bDataOnly)
		{
			if (MeshEstimate->Macro)
			{
				if (!MeshEstimate->bCompacted)
				{
					if (!BuildStorage->CompactGeometryArray(MeshEstimate->Macro->DistantCells,
						PageBuild->MaximumDistantBytes(), WorkingLease, Result)) return Result;
					MeshEstimate->bCompacted = true;
				}
				FVoxelSectionMeshResult CountedMesh;
				bool bComplete = false;
				Result.bSuccess =
					MeshEstimate->Counter.Advance(*MeshEstimate->Macro, *Config, *Registry, TextureStretch, CountedMesh, bComplete, Result.Error, &InCancel);
				if (!Result.bSuccess)
					return Result;
				if (!bComplete)
				{
					Result.bContinue = true;
					return Result;
				}
				MeshEstimate->Macro->EstimatedMeshBytes = MeshEstimate->Counter.GetEstimatedMeshBytes();
				Result.Macro = MeshEstimate->Macro;
				BuildStorage->SettleCompleted(Result);
				return Result;
			}
			Result.Macro = BuildStorage->MakeResult<FVoxelMacroTileData>();

			const FVoxelMacroTerrainBuilder Builder(Generator.ToSharedRef(), Config.ToSharedRef(), Settings, *OverlayRead->Result->Snapshot, Registry.ToSharedRef(), Lod);

			bool bComplete = false;
			Result.bSuccess = PageBuild->Advance(Builder, *Result.Macro, bComplete, Result.Error, &InCancel);
			if (Result.Macro) Result.Macro->ResourceLease = BuildStorage->GeometryLease;
			if (!Result.bSuccess && !InCancel.Load() && OverlayRead->DeferFacts(Result))
			{
				Result.Macro.Reset();
				return Result;
			}
			if (Result.bSuccess && !bComplete)
			{
				Result.Macro.Reset();
				Result.bContinue = true;
				return Result;
			}

			if (!Result.bSuccess)
				return Result;
			MeshEstimate->Macro = Result.Macro;
			Result.Macro.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.MacroMesh = BuildStorage->MakeResult<FVoxelSectionMeshResult>();
		bool bComplete = false;
		Result.bSuccess = PageMesh->Advance(*Prepared->Macro, *Config, *Registry, TextureStretch, *Result.MacroMesh, bComplete, Result.Error, &InCancel);
		if (Result.MacroMesh) Result.MacroMesh->ResourceLease = BuildStorage->GeometryLease;
		if (Result.bSuccess && bComplete)
		{
			Result.MacroMesh->InputWorkReceipt = Prepared->WorkReceipt;
			Result.MacroMesh->FrozenMacro = Prepared->Macro;
			BuildStorage->SettleCompleted(Result);
		}
		if (Result.bSuccess && !bComplete)
		{
			Result.MacroMesh.Reset();
			Result.bContinue = true;
			return Result;
		}
		Result.Macro = Prepared->Macro;

		return Result;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}

	if (bDataOnly)
		return EnqueuePreparedData(MoveTemp(Request));
	if (!EnqueueViewTask(MoveTemp(Request)))
		return false;
	RemovePreparedData(DataKey);
	return true;
}

bool FVoxelViewManager::HasRenderableMesh(const FVoxelSectionMeshResult& InMesh)
{
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		if (!Batch.Mesh.Vertices.IsEmpty() && !Batch.Mesh.Triangles.IsEmpty())
			return true;
	}
	return false;
}

bool FVoxelViewManager::PublishFine(const FVoxelTaskResult& InResult)
{
	const FIntVector& Key = InResult.Stamp.Section;
	if (!InResult.FineMesh || !GetInputRoot().FineWanted.Contains(Key))
		return false;
	const FVoxelSection* Section = Module.GetRuntime()->FindSection(Key);
	if (!Section || Section->Stamp.Epoch != InResult.Stamp.WorldEpoch || Section->Stamp.Token != InResult.Stamp.Token ||
		Section->CommittedRevision != InResult.Stamp.Revision)
		return false;
	FineReady.Add(Key);
	FineRevisions.Add(Key, InResult.Stamp.Revision);
	FineSectionMeshes.Add(Key, InResult.FineMesh);
	IndexCoverage(FVoxelViewKey{Key, 0});
	FineLastWanted.Add(Key, FPlatformTime::Seconds());
	TrackReadyTerrainNode({Key, 0});
	if (const FVoxelFinePageKey* Page = GetInputRoot().FinePagePlan.SectionOwners.Find(Key))
		MarkFinePageDirty(*Page);
	bFinePresentationDirty = true;
	bTerrainBuildStageDirty = true;
	MarkCoverageDirty(FineCoverageRect(Key));
	return true;
}

bool FVoxelViewManager::PublishVoxelProxy(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.ViewKey);
	if (!InResult.VoxelProxy || !DesiredGeometry || InResult.VoxelProxy->Geometry != *DesiredGeometry)
		return false;
	const FVoxelViewKey& Key = InResult.Stamp.ViewKey;
	if (!InResult.VoxelProxy || !InResult.VoxelProxyMesh || !GetInputRoot().ProxyWanted.Contains(Key) ||
		!IsRepresentationStampCurrent(InResult.Stamp) ||
		Module.GetRuntime()->GetChangeHierarchy().GetVoxelProxyRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	if (!RetainRawPublication(InResult.VoxelProxy->ResourceLease, InResult.VoxelProxy->Geometry, Publisher->GetTerrainRenderer())) return false;
	VoxelProxyData.Add(Key, InResult.VoxelProxy);
	if (!InResult.VoxelProxy->Cells.IsEmpty() && !InResult.VoxelProxy->Cells.ContainsByPredicate(
													 [](const FVoxelBlockState& State)
													 {
														 return !State.IsAir();
													 }))
		AirVoxelProxyNodes.Add(Key);
	else
		AirVoxelProxyNodes.Remove(Key);
	PreparedVolumeSignatures.Remove(Key);
	PreparedVolumeMeshes.Remove(Key);
	FVoxelBoundaryTransitionContext BaseContext;
	BaseContext.Owner = Key;
	BaseContext.SampleStepCells = InResult.VoxelProxy->Geometry.SampleStepCells;
	check(NextVolumeInputSerial < MAX_uint64);
	BaseContext.InputSerial = ++NextVolumeInputSerial;
	VolumeTransitionSignatures.Add(Key, BaseContext.Signature(InResult.VoxelProxy->Revision));
	UpdateProxySurfaceCoverage(InResult.VoxelProxy);
	bTerrainBuildStageDirty = true;
	VoxelProxyReady.Add(Key);
	TrackReadyTerrainNode(Key);
	VoxelProxyLastWanted.Add(Key, FPlatformTime::Seconds());
	VoxelProxyRevisions.Add(Key, InResult.Stamp.Revision);
	IndexCoverage(Key);
	const int32 TerrainStage = VoxelProxyTerrainStage(Key);
	const FGuid Group = BeginPublishGroup({1, Key.Coordinate, Key.Level});
	Publisher->SetTerrainStage(Group, TerrainStage);
	AActor* Host = VoxelProxyActors.FindRef(Key);
	InResult.VoxelProxyMesh->Stamp = InResult.Stamp;
	const TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh = InResult.VoxelProxyMesh;
	if (!Publisher->StageImmutable(Group, Host, FVector(Key.GetBounds().Min) * Module.BlockSize(),
			Module.BlockSize() * InResult.VoxelProxy->Geometry.SampleStepCells, Mesh, TerrainStage))
	{
		bReadyTerrainBranchesDirty |= VoxelProxyReady.Remove(Key) > 0;
		VoxelProxyData.Remove(Key);
		AirVoxelProxyNodes.Remove(Key);
		ProxySurfaceCoverage.Remove(Key);
		VoxelProxyRevisions.Remove(Key);
		VolumeTransitionSignatures.Remove(Key);
		return false;
	}
	VoxelProxyActors.Add(Key, Host);
	RecordTerrainMesh({EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
	return true;
}

bool FVoxelViewManager::PublishSurface(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.SurfaceKey);
	if (!InResult.Surface || !DesiredGeometry || InResult.Surface->Geometry != *DesiredGeometry)
		return false;
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;
	if (!InResult.Surface || !InResult.SurfaceMesh || !GetInputRoot().SurfaceWanted.Contains(Key) ||
		!IsRepresentationStampCurrent(InResult.Stamp) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	if (!RetainRawPublication(InResult.Surface->ResourceLease, InResult.Surface->Geometry, Publisher->GetTerrainRenderer())) return false;
	AActor* Host = SurfaceActors.FindRef(Key);
	const int32 TileSide = InResult.Surface->GetTileSide();
	InResult.SurfaceMesh->Stamp = InResult.Stamp;
	const TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh = InResult.SurfaceMesh;
	if (!Publisher->StageImmutable(BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh, 2))
	{
		SurfaceReady.Remove(Key);
		return false;
	}
	SurfaceActors.Add(Key, Host);
	bTerrainBuildStageDirty = true;
	SurfaceReady.Add(Key);
	bHeightfieldTopologyDirty = true;
	SurfaceLastWanted.Add(Key, FPlatformTime::Seconds());
	SurfaceRevisions.Add(Key, InResult.Stamp.Revision);
	SurfaceData.Add(Key, InResult.Surface);
	IndexCoverage(Key);
	RecordTerrainMesh(
		{EVoxelTerrainRepresentation::Surface, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
	return true;
}

void FVoxelViewManager::PublishWater(const FVoxelTaskResult& InResult)
{
	const FVoxelSurfaceTileKey& Key = InResult.Stamp.SurfaceKey;

	if (!InResult.Water || !InResult.WaterMesh || !GetInputRoot().SurfaceWanted.Contains(Key) ||
		!IsRepresentationStampCurrent(InResult.Stamp) ||
		Module.GetRuntime()->GetChangeHierarchy().GetSurfaceRevision(Key) != InResult.Stamp.Revision)
	{
		return;
	}

	AActor* Host = WaterActors.FindRef(Key);

	const int32 TileSide = (InResult.Water->Side - 1) * InResult.Water->Step;
	InResult.WaterMesh->Stamp = InResult.Stamp;

	if (Publisher->Stage(BeginPublishGroup({2, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), MoveTemp(*InResult.WaterMesh), 2))
	{
		WaterActors.Add(Key, Host);
	}
}

bool FVoxelViewManager::PublishMacro(const FVoxelTaskResult& InResult)
{
	const auto* DesiredGeometry = GetViewInterest().FindGeometry(InResult.Stamp.MacroKey);
	if (!InResult.Macro || !DesiredGeometry || InResult.Macro->Geometry != *DesiredGeometry)
	{
		return false;
	}

	const FVoxelMacroTileKey& Key = InResult.Stamp.MacroKey;

	if (!InResult.MacroMesh || !GetInputRoot().MacroWanted.Contains(Key) || !IsRepresentationStampCurrent(InResult.Stamp) ||
		Module.GetRuntime()->GetChangeHierarchy().GetMacroRevision(Key) != InResult.Stamp.Revision)
	{
		return false;
	}

	if (!RetainRawPublication(InResult.Macro->ResourceLease, InResult.Macro->Geometry, Publisher->GetTerrainRenderer())) return false;
	AActor* Host = MacroActors.FindRef(Key);
	const int32 TileSide = InResult.Macro->GetTileSide();
	InResult.MacroMesh->Stamp = InResult.Stamp;
	const TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh = InResult.MacroMesh;

	if (Publisher->StageImmutable(BeginPublishGroup({3, Key.Coordinate, Key.Level}), Host,
			FVector(Key.Coordinate.X * TileSide, Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh, 3))
	{
		MacroActors.Add(Key, Host);
		MacroData.Add(Key, InResult.Macro);
		MacroReady.Add(Key);
		MacroLastWanted.Add(Key, FPlatformTime::Seconds());
		MacroRevisions.Add(Key, InResult.Stamp.Revision);

		bTerrainBuildStageDirty = true;
		bHeightfieldTopologyDirty = true;
		IndexCoverage(Key);
		RecordTerrainMesh(
			{EVoxelTerrainRepresentation::Macro, FIntVector(Key.Coordinate.X, Key.Coordinate.Y, 0), Key.Level}, *Mesh, InResult.ExecuteMilliseconds);
		return true;
	}

	MacroReady.Remove(Key);
	return false;
}

bool FVoxelViewManager::IsFineCoverageReady(const FIntVector& InSection, const IVoxelTerrainPresentedSnapshot* InPresented) const
{
	if (!InPresented || InPresented->GetWorldEpoch() != WorldEpoch) return false;
	const auto* Evidence = InPresented->Find(InSection);
	if (!Evidence || Evidence->Content.SampleStepCells != 1 ||
		(Evidence->Owner.Kind != EVoxelFineOwnerKind::F0 && Evidence->Owner.Kind != EVoxelFineOwnerKind::F1Plus)) return false;
	const auto Geometry = StaticCastSharedPtr<const FVoxelFineShardGeometry>(Evidence->GeometryLease);
	return Geometry && Module.IsOverlayVersionCurrent(Geometry->OverlayVersion) &&
		Evidence->Content.GeneratorIdentity == Module.GetManifest().BaseSampleHash &&
		Evidence->Content.RecipeRevision == Module.GetManifest().RecipeHash &&
		Evidence->Content.RegistryRevision == Module.GetRegistry()->Hash &&
		Evidence->Content.ShapeRevision == Module.GetShapes()->GetContentHash() &&
		Geometry->Attempt.Input.DataRevision == Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Geometry->Attempt.Input.Shard);
}

bool FVoxelViewManager::IsFinePresented(const FIntVector& InSection) const
{
	return IsLatestFinePresented(InSection);
}

void FVoxelViewManager::RebuildFinePagePlan()
{
	const auto& Planned = GetViewInterest().FinePagePlan;
	FVoxelFinePagePlan Next;
	if (Planned)
	{
		Next = *Planned;
	}
	else
	{
		TArray<FVoxelStreamingSource> Sources;
		for (const auto& Pair : GetViewInterest().Sources)
			Sources.Add(Pair.Value.Source);
		FString Error;
		if (!FVoxelFinePagePlanner::Build(GetInputRoot().FineWanted, Sources, Module.GetViewSettings(), Next, Error))
		{
			if (LastFinePagePlanningError != Error)
			{
				UE_LOG(LogTemp, Error, TEXT("Fine page plan rejected; committed layout retained: %s"), *Error);
				LastFinePagePlanningError = Error;
			}
			return;
		}
	}
	LastFinePagePlanningError.Reset();
	for (const auto& Pair : Next.SectionOwners)
	{
		const FVoxelFinePageKey* Previous = GetInputRoot().FinePagePlan.SectionOwners.Find(Pair.Key);
		if (!Previous || *Previous != Pair.Value)
		{
			InvalidateFinePageLayout(Pair.Value);
			if (Previous)
			{
				InvalidateFinePageLayout(*Previous);
			}
			MarkCoverageDirty(FineCoverageRect(Pair.Key));
		}
	}
	for (const auto& Pair : GetInputRoot().FinePagePlan.SectionOwners)
	{
		if (!Next.SectionOwners.Contains(Pair.Key))
		{
			InvalidateFinePageLayout(Pair.Value);
			MarkCoverageDirty(FineCoverageRect(Pair.Key));
		}
	}
	if (Next.Pages.Num() != GetInputRoot().FinePagePlan.Pages.Num() || !Next.Pages.Difference(GetInputRoot().FinePagePlan.Pages).IsEmpty())
	{
		++FinePageLayoutRevision;
	}
	for (const auto& Page : GetInputRoot().FinePagePlan.Pages)
		if (!Next.Pages.Contains(Page))
		{
			RetireFinePageLayoutState(Page);
			QueueRetirement({0, Page.Coordinate, Page.Level});
		}
	GetInputRoot().FinePagePlan = MoveTemp(Next);
	bFinePresentationDirty |= !DirtyFinePages.IsEmpty();
}

void FVoxelViewManager::GatherFinePageCandidates(const TSet<FVoxelViewKey>& InCandidates, TSet<FVoxelFinePageKey>& OutPages) const
{
	OutPages.Reset();
	const auto AddScopedPages = [this, &OutPages](const TSet<FVoxelFinePageKey>& Pages)
	{
		for (const FVoxelFinePageKey& Page : Pages)
		{
			const FVoxelGenerationBounds Bounds = Page.GetBounds();
			if (CoverageAffects({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)}))
				OutPages.Add(Page);
		}
	};
	AddScopedPages(DirtyFinePages);
	AddScopedPages(PendingFinePages);
	for (const FVoxelViewKey& Node : InCandidates)
	{
		if (Node.Level != 0)
			continue;
		if (const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(Node.Coordinate))
			OutPages.Add(*Page);
		if (const FVoxelFinePageKey* Page = GetInputRoot().FinePagePlan.SectionOwners.Find(Node.Coordinate))
			OutPages.Add(*Page);
	}
	for (const FVoxelFinePageKey& Page : OutPages.Array())
	{
		TArray<FIntVector> Sections;
		FVoxelFinePagePlanner::GatherSections(Page, Sections);
		for (const FIntVector& Section : Sections)
		{
			if (const FVoxelFinePageKey* Desired = GetInputRoot().FinePagePlan.SectionOwners.Find(Section))
				OutPages.Add(*Desired);
		}
	}
}

void FVoxelViewManager::InvalidateFinePresentation(const FIntVector& InSection)
{
	FineSectionMeshes.Remove(InSection);
	bReadyTerrainBranchesDirty |= FineReady.Remove(InSection) > 0;
	FineRevisions.Remove(InSection);
	if (const FVoxelFinePageKey* Page = GetInputRoot().FinePagePlan.SectionOwners.Find(InSection))
	{
		MarkFinePageDirty(*Page);
	}
	if (const FVoxelFinePageKey* Page = PresentedFineSectionOwners.Find(InSection))
		MarkFinePageDirty(*Page);
	bFinePresentationDirty = true;
	MarkCoverageDirty(FineCoverageRect(InSection));
}

FVoxelCoverageRect FVoxelViewManager::TerrainCoverageRect(const FVoxelViewKey& InKey) const
{
	return InKey.Level == 0 ? FineCoverageRect(InKey.Coordinate) : VoxelProxyCoverageRect(InKey);
}

void FVoxelViewManager::BuildCoveragePassSnapshot(FCoveragePassSnapshot& OutSnapshot) const
{
	++CoverageSnapshotBuildCount;
	OutSnapshot = {};
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	if (bActiveCoverageFull)
	{
		CaptureCoverageCandidates(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates);
		ProxyCandidates = VoxelProxyReady;
		SurfaceCandidates = GetInputRoot().SurfaceWanted;
		MacroCandidates = GetInputRoot().MacroWanted;
	}
	else
		CaptureCoverageCandidates(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates);
	for (const FIntVector& Section : FineCandidates)
	{
		if (!GetInputRoot().FineWanted.Contains(Section) || !VisibleTerrainNodes.Contains({Section, 0}) || !IsFinePresented(Section))
			continue;
		OutSnapshot.PresentedFineSections.Add(Section);
		if (const auto* Owner = PresentedFineSectionOwners.Find(Section)) OutSnapshot.PresentedFineOwners.Add(Section, *Owner);
		OutSnapshot.Fine.Add(FineCoverageBox(Section));
		OutSnapshot.FineNodes.Add({Section, 0});
	}
	for (const FVoxelViewKey& Node : ProxyCandidates)
	{
		if (!VoxelProxyReady.Contains(Node) || !GetInputRoot().ProxyWanted.Contains(Node) || !VisibleTerrainNodes.Contains(Node))
			continue;
		AActor* Actor = VoxelProxyActors.FindRef(Node);
		if (Actor && (Actor->IsHidden() || !Publisher->IsCommitted(Actor)))
			continue;
		OutSnapshot.ProxyNodes.Add(Node);
		OutSnapshot.Proxy.Add(VoxelProxyCoverageBox(Node));
		if (const auto Data = ProxySurfaceCoverage.FindRef(Node))
			OutSnapshot.ProxySurface.Append(Data->TopSurfaceCoverage);
	}
	for (const auto& Key : SurfaceCandidates)
	{
		AActor* Actor = SurfaceActors.FindRef(Key);
		if (GetInputRoot().SurfaceWanted.Contains(Key) && SurfaceReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Surface.Add(SurfaceCoverageRect(Key));
	}
	for (const auto& Key : MacroCandidates)
	{
		AActor* Actor = MacroActors.FindRef(Key);
		if (GetInputRoot().MacroWanted.Contains(Key) && MacroReady.Contains(Key) && Actor && !Actor->IsHidden() && Publisher->IsCommitted(Actor))
			OutSnapshot.Macro.Add(MacroCoverageRect(Key));
	}
	OutSnapshot.Heightfield = OutSnapshot.Surface;
	OutSnapshot.Heightfield.Append(OutSnapshot.Macro);
	for (const auto& Key : SurfaceCandidates)
		OutSnapshot.WantedHeightfield.Add(SurfaceCoverageRect(Key));
	for (const auto& Key : MacroCandidates)
		OutSnapshot.WantedHeightfield.Add(MacroCoverageRect(Key));
	const auto BuildBranches = [](const TSet<FVoxelViewKey>& Nodes, TSet<FVoxelViewKey>& Branches)
	{
		for (FVoxelViewKey Node : Nodes)
		{
			while (Node.Level <= 20)
			{
				bool bExists = false;
				Branches.Add(Node, &bExists);
				if (bExists)
					break;
				Node = Node.GetParent();
			}
		}
	};
	BuildBranches(OutSnapshot.FineNodes, OutSnapshot.FineBranches);
	BuildBranches(OutSnapshot.ProxyNodes, OutSnapshot.ProxyBranches);
	OutSnapshot.bComplete = true;
}

bool FVoxelViewManager::IsTerrainVolumeCovered(const FVoxelViewKey& InNode, const FCoveragePassSnapshot& InSnapshot, const bool bByFine) const
{
	const auto& Nodes = bByFine ? InSnapshot.FineNodes : InSnapshot.ProxyNodes;
	const auto& Branches = bByFine ? InSnapshot.FineBranches : InSnapshot.ProxyBranches;
	FVoxelViewKey Ancestor = InNode;
	while (Ancestor.Level <= 20)
	{
		if (Nodes.Contains(Ancestor))
			return true;
		Ancestor = Ancestor.GetParent();
	}
	if (InNode.Level == 0 || !Branches.Contains(InNode))
		return false;
	TArray<FVoxelViewKey> Children;
	InNode.GetChildren(Children);
	for (const FVoxelViewKey& Child : Children)
	{
		if (!IsTerrainVolumeCovered(Child, InSnapshot, bByFine))
			return false;
	}
	return true;
}

bool FVoxelViewManager::IsFineReplacementReady(const FIntVector& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return IsTerrainVolumeCovered({InKey, 0}, InSnapshot, false) || VoxelCoverage::IsFullyCovered2D(FineCoverageRect(InKey), InSnapshot.Heightfield);
}

bool FVoxelViewManager::IsProxyReplacementReady(const FVoxelViewKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return IsTerrainVolumeCovered(InKey, InSnapshot, true) || VoxelCoverage::IsFullyCovered2D(VoxelProxyCoverageRect(InKey), InSnapshot.Heightfield);
}

bool FVoxelViewManager::IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return VoxelCoverage::IsFullyCovered2D(SurfaceCoverageRect(InKey), InSnapshot.Macro) ||
		   VoxelCoverage::IsFullyCovered2D(SurfaceCoverageRect(InKey), InSnapshot.ProxySurface);
}

bool FVoxelViewManager::IsMacroReplacementReady(const FVoxelMacroTileKey& InKey, const FCoveragePassSnapshot& InSnapshot) const
{
	return VoxelCoverage::IsFullyCovered2D(MacroCoverageRect(InKey), InSnapshot.Surface);
}

double FVoxelViewManager::TerrainDeadline(const double InMaximumMilliseconds) const
{
	return Module.GetTerrainFrameContext().Deadline(InMaximumMilliseconds);
}

void FVoxelViewManager::IndexCoverage(const FVoxelViewKey& InKey)
{
	FVoxelCoverageRecord Value;
	auto* Record = &Value;
	Record->Geometry.Key = {InKey.Level == 0 ? EVoxelTerrainRepresentation::Fine : EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level};
	if (InKey.Level > 0)
	{
		if (const auto Data = VoxelProxyData.FindRef(InKey))
			Record->Geometry = Data->Geometry;
		else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
			Record->Geometry = *Geometry;
	}
	Record->Stamp = {WorldEpoch, 0, InKey.Level == 0 ? FineRevisions.FindRef(InKey.Coordinate) : VoxelProxyRevisions.FindRef(InKey), 0};
	Record->bCommitted = VisibleTerrainNodes.Contains(InKey);
	Record->bWanted = InKey.Level == 0 ? GetInputRoot().FineWanted.Contains(InKey.Coordinate) : GetInputRoot().ProxyWanted.Contains(InKey);
	Record->bReady = InKey.Level == 0 ? FineReady.Contains(InKey.Coordinate) : VoxelProxyReady.Contains(InKey);
	Record->bHasGeometry =
		Record->bReady && (InKey.Level > 0 ? !AirVoxelProxyNodes.Contains(InKey)
									   : (FineSectionMeshes.FindRef(InKey.Coordinate) && HasRenderableMesh(*FineSectionMeshes.FindRef(InKey.Coordinate))));
	if (InKey.Level == 0)
	{
		Record->Geometry.SampleStepCells = 1;
		Record->Geometry.WorldMinZ = Module.GetManifest().Settings.MinZ;
		Record->Geometry.WorldMaxZ = Module.GetManifest().Settings.MaxZ;
		Record->bReady = IsFineGeometryPrepared(InKey.Coordinate);
		if (const auto* Actual = Publisher->GetTerrainRenderer().FindPresentedAtom(InKey.Coordinate))
		{
			Record->Stamp.ContentRevision = Actual->Evidence.Content.DataRevision;
			Record->bCommitted = IsLatestFinePresented(InKey.Coordinate);
			Record->bReady |= Record->bCommitted;
			Record->bHasGeometry = !Actual->Evidence.bKnownEmpty;
		}
	}
	const auto Bounds = InKey.GetBounds();
	Record->Min = Bounds.Min;
	Record->Max = Bounds.Max;
	ApplyCoverageIndexRecord(*Record);
}

void FVoxelViewManager::IndexCoverage(const FVoxelSurfaceTileKey& InKey)
{
	FVoxelCoverageRecord Value;
	auto* Record = &Value;
	Record->Geometry.Key = {EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
	if (const auto Data = SurfaceData.FindRef(InKey))
		Record->Geometry = Data->Geometry;
	else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
		Record->Geometry = *Geometry;
	Record->Geometry.WorldMinZ = Module.GetManifest().Settings.MinZ;
	Record->Geometry.WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	Record->Stamp = {WorldEpoch, 0, SurfaceRevisions.FindRef(InKey), 0};
	Record->bWanted = GetInputRoot().SurfaceWanted.Contains(InKey);
	Record->bReady = SurfaceReady.Contains(InKey);
	Record->bHasGeometry = Record->bReady;
	Record->bCommitted = SurfaceActors.FindRef(InKey) && Publisher->IsCommitted(SurfaceActors.FindRef(InKey));
	Record->Geometry.Key.TryBounds(Record->Geometry.WorldMinZ, Record->Geometry.WorldMaxZ, Record->Min, Record->Max);
	ApplyCoverageIndexRecord(*Record);
}

void FVoxelViewManager::IndexCoverage(const FVoxelMacroTileKey& InKey)
{
	FVoxelCoverageRecord Value;
	auto* Record = &Value;
	Record->Geometry.Key = {EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
	if (const auto Data = MacroData.FindRef(InKey))
		Record->Geometry = Data->Geometry;
	else if (const auto* Geometry = GetViewInterest().FindGeometry(InKey))
		Record->Geometry = *Geometry;
	Record->Geometry.WorldMinZ = Module.GetManifest().Settings.MinZ;
	Record->Geometry.WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	Record->Stamp = {WorldEpoch, 0, MacroRevisions.FindRef(InKey), 0};
	Record->bWanted = GetInputRoot().MacroWanted.Contains(InKey);
	Record->bReady = MacroReady.Contains(InKey);
	Record->bHasGeometry = Record->bReady;
	Record->bCommitted = MacroActors.FindRef(InKey) && Publisher->IsCommitted(MacroActors.FindRef(InKey));
	Record->Geometry.Key.TryBounds(Record->Geometry.WorldMinZ, Record->Geometry.WorldMaxZ, Record->Min, Record->Max);
	ApplyCoverageIndexRecord(*Record);
}

void FVoxelViewManager::CaptureCoverageCandidates(
	TSet<FIntVector>& OutFine, TSet<FVoxelViewKey>& OutProxy, TSet<FVoxelSurfaceTileKey>& OutSurface, TSet<FVoxelMacroTileKey>& OutMacro) const
{
	for (const FVoxelCoverageRect& Rect : ActiveDirtyCoverageRects)
	{
		FVoxelCoverageLocalSnapshot Snapshot;
		CoverageIndex.Capture(FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Snapshot);
		CoverageIndexNodesVisited += Snapshot.VisitedIndexNodes;
		CoverageRecordsExamined += Snapshot.ExaminedRecords;
		CoverageRecordsCaptured += Snapshot.Records.Num();
		CoverageReadGuards += Snapshot.Guards.Num();
		for (const auto& Record : Snapshot.Records)
		{
			const auto& Key = Record->Geometry.Key;
			switch (Key.Representation)
			{
			case EVoxelTerrainRepresentation::Fine:
				OutFine.Add(Key.Coordinate);
				break;
			case EVoxelTerrainRepresentation::VoxelProxy:
				OutProxy.Add({Key.Coordinate, Key.Level});
				break;
			case EVoxelTerrainRepresentation::Surface:
				OutSurface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			case EVoxelTerrainRepresentation::Macro:
				OutMacro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				break;
			default:
				break;
			}
		}
	}
}

void FVoxelViewManager::RefreshIndexedCoverage(const FVoxelCoverageRect& InBounds)
{
	FVoxelCoverageLocalSnapshot Snapshot;
	CoverageIndex.Capture(FIntVector(InBounds.Min.X, InBounds.Min.Y, MIN_int32 + 1), FIntVector(InBounds.Max.X, InBounds.Max.Y, MAX_int32), Snapshot);
	for (const auto& Record : Snapshot.Records)
	{
		const auto& Key = Record->Geometry.Key;
		switch (Key.Representation)
		{
		case EVoxelTerrainRepresentation::Fine:
		case EVoxelTerrainRepresentation::VoxelProxy:
			IndexCoverage(FVoxelViewKey{Key.Coordinate, Key.Level});
			break;
		case EVoxelTerrainRepresentation::Surface:
			IndexCoverage(FVoxelSurfaceTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			break;
		case EVoxelTerrainRepresentation::Macro:
			IndexCoverage(FVoxelMacroTileKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			break;
		default:
			break;
		}
	}
}

const FVoxelInterestSet& FVoxelViewManager::GetViewInterest() const
{
	if (bRetirementReadScope && RetirementInterestSnapshot)
		return *RetirementInterestSnapshot;
	if (bCoverageReadScope && CoverageInterestSnapshot)
		return *CoverageInterestSnapshot;
	return AppliedInterestSnapshot ? *AppliedInterestSnapshot : Module.GetCurrentInterest();
}

void FVoxelViewManager::MarkFinePageDirty(const FVoxelFinePageKey& InPage)
{
	DirtyFinePages.Add(InPage);
	bFinePresentationDirty = true;
}

void FVoxelViewManager::InvalidateFinePageLayout(const FVoxelFinePageKey& InPage)
{
	MarkFinePageDirty(InPage);
}

void FVoxelViewManager::RetireFinePageLayoutState(const FVoxelFinePageKey& InPage)
{
	DirtyFinePages.Remove(InPage);
}

bool FVoxelViewManager::IsCriticalDependency(const FVoxelTerrainSpatialKey& InKey) const
{
	if (Module.GetCurrentInterest().CriticalDependencies.Contains(InKey))
		return true;
	if (InKey.Representation == EVoxelTerrainRepresentation::VoxelProxy)
	{
		return VolumeTransitionWorkClass({InKey.Coordinate, InKey.Level}) == EVoxelWorkClass::Critical;
	}
	return false;
}

bool FVoxelViewManager::IsCriticalDependency(const FVoxelViewAdmission& InAdmission) const
{
	if (InAdmission.Kind == EVoxelViewAdmissionKind::Fine)
	{
		const auto& Interest = Module.GetCurrentInterest();
		if (Interest.FinePagePlan)
		{
			const auto* Page = Interest.FinePagePlan->SectionOwners.Find(InAdmission.FineKey);
			if (Page && Interest.CriticalFinePages.Contains(*Page))
			{
				return true;
			}
		}
		const auto* Demand = Interest.Exact.Find(InAdmission.FineKey);
		return Demand && (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
							 Demand->bMovementCriticalFine);
	}
	const FVoxelTerrainSpatialKey Key =
		InAdmission.Kind == EVoxelViewAdmissionKind::VoxelProxy
			? FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InAdmission.ProxyKey.Coordinate, InAdmission.ProxyKey.Level}
		: InAdmission.Kind == EVoxelViewAdmissionKind::Surface
			? FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface,
				  FIntVector(InAdmission.SurfaceKey.Coordinate.X, InAdmission.SurfaceKey.Coordinate.Y, 0), InAdmission.SurfaceKey.Level}
			: FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InAdmission.MacroKey.Coordinate.X, InAdmission.MacroKey.Coordinate.Y, 0),
				  InAdmission.MacroKey.Level};
	return IsCriticalDependency(Key);
}

FVoxelGameThreadContinuation FVoxelViewManager::RebuildHeightfieldTransitionSteps(const FCoveragePassSnapshot& CoverageSnapshot)
{
	TArray<FVoxelHeightfieldTileView> Views;
	TArray<FHeightfieldTransitionTileSource> ViewPins;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	bool bCaptured = false;
	TSet<FIntVector> FineCandidates;
	TSet<FVoxelViewKey> ProxyCandidates;
	TSet<FVoxelSurfaceTileKey> SurfaceCandidates;
	TSet<FVoxelMacroTileKey> MacroCandidates;
	co_await CaptureCoverageCandidateSteps(FineCandidates, ProxyCandidates, SurfaceCandidates, MacroCandidates, CandidateLease, bCaptured);
	if (!bCaptured) { RetryActiveCoverage(); co_return; }
	TSet<FVoxelHeightfieldNodeKey> LocalOwners;
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (CoverageAffects(SurfaceCoverageRect(Key)))
			LocalOwners.Add({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (CoverageAffects(MacroCoverageRect(Key)))
			LocalOwners.Add({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
	}
	if (!bActiveCoverageFull)
		for (const auto& Owner : LocalOwners)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto Rect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																							   : MacroCoverageRect({Owner.Coordinate, Owner.Level});
			FVoxelCoverageLocalSnapshot Neighbors;
			co_await HeightfieldCoverageIndex.CaptureSteps(
				FIntVector(Rect.Min.X - 1, Rect.Min.Y - 1, MIN_int32 + 1), FIntVector(Rect.Max.X + 1, Rect.Max.Y + 1, MAX_int32), Neighbors, true, false);
			if (Neighbors.bCountOnly) { RetryActiveCoverage(); co_return; }
			for (const auto& Record : Neighbors.Records)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				const auto& Key = Record->Geometry.Key;
				if (Key.Representation == EVoxelTerrainRepresentation::Surface)
					SurfaceCandidates.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				else if (Key.Representation == EVoxelTerrainRepresentation::Macro)
					MacroCandidates.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
			}
		}
	Views.Reserve(SurfaceCandidates.Num() + MacroCandidates.Num());
	for (const auto& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelSurfaceTileData, ESPMode::ThreadSafe>> Pair(Key, SurfaceData.FindRef(Key));
		if (!Pair.Value || !SurfaceReady.Contains(Pair.Key) || !SurfaceActors.FindRef(Pair.Key))
		{
			continue;
		}
		if (!GetInputRoot().SurfaceWanted.Contains(Pair.Key))
		{
			bool bReplaced = false;
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.Macro, bReplaced);
			if (!bReplaced)
			{
				co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.ProxySurface, bReplaced);
			}
			if (bReplaced)
			{
				continue;
			}
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
		FHeightfieldTransitionTileSource Pin;
		Pin.Surface = Pair.Value;
		ViewPins.Add(MoveTemp(Pin));
	}
	for (const auto& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelMacroTileKey, TSharedPtr<const FVoxelMacroTileData, ESPMode::ThreadSafe>> Pair(Key, MacroData.FindRef(Key));
		if (!Pair.Value || !MacroReady.Contains(Pair.Key) || !MacroActors.FindRef(Pair.Key))
		{
			continue;
		}
		if (!GetInputRoot().MacroWanted.Contains(Pair.Key))
		{
			bool bReplaced = false;
			co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Pair.Key), CoverageSnapshot.Surface, bReplaced);
			if (bReplaced)
			{
				continue;
			}
		}
		Views.Add(FVoxelHeightfieldTransitionBuilder::MakeView(*Pair.Value));
		FHeightfieldTransitionTileSource Pin;
		Pin.Macro = Pair.Value;
		ViewPins.Add(MoveTemp(Pin));
	}
	TArray<FVoxelHeightfieldTileFootprint> PendingTiles;
	PendingTiles.Reserve(SurfaceCandidates.Num() + MacroCandidates.Num());
	for (const FVoxelSurfaceTileKey& Key : SurfaceCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!GetInputRoot().SurfaceWanted.Contains(Key))
			continue;
		if (SurfaceReady.Contains(Key) && SurfaceData.Contains(Key) && SurfaceActors.FindRef(Key))
			continue;
		const auto* DesiredGeometry = GetViewInterest().FindGeometry(Key);
		if (!DesiredGeometry)
			continue;
		const int32 Step = DesiredGeometry->SampleStepCells;
		const int32 Side = VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, Key.Level);
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	for (const FVoxelMacroTileKey& Key : MacroCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!GetInputRoot().MacroWanted.Contains(Key))
			continue;
		if (MacroReady.Contains(Key) && MacroData.Contains(Key) && MacroActors.FindRef(Key))
			continue;
		const auto* DesiredGeometry = GetViewInterest().FindGeometry(Key);
		if (!DesiredGeometry)
			continue;
		const int32 Step = DesiredGeometry->SampleStepCells;
		const int32 Side = VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, Key.Level);
		PendingTiles.Add({{EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}, Key.Coordinate * Side, Side, Step});
	}
	TMap<FVoxelHeightfieldNodeKey, int32> ViewIndices;
	TMap<FVoxelHeightfieldNodeKey, int32> PendingIndices;
	for (int32 Index = 0; Index < Views.Num(); ++Index)
		ViewIndices.Add(Views[Index].Key, Index);
	for (int32 Index = 0; Index < PendingTiles.Num(); ++Index)
		PendingIndices.Add(PendingTiles[Index].Key, Index);
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Dependencies;
	co_await FVoxelHeightfieldTransitionBuilder::GatherOwnerDependenciesSteps(Views, PendingTiles, Dependencies);

	if (bActiveCoverageFull)
		PendingHeightfieldPlanOwners.Reset();
	else
		for (const auto& Key : LocalOwners)
			PendingHeightfieldPlanOwners.Remove(Key);
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	for (const auto& OwnerView : Views)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelHeightfieldNodeKey Owner = OwnerView.Key;
		if (!LocalOwners.Contains(Owner))
			continue;
		TArray<FVoxelHeightfieldTileView> LocalViews;
		TArray<FVoxelHeightfieldTileFootprint> LocalPending;
		TArray<FHeightfieldTransitionTileSource> Pins;
		uint64 Signature = 1469598103934665603ull;
		const auto Mix = [&Signature](const uint64 Value)
		{
			Signature = (Signature ^ Value) * 1099511628211ull;
		};
		const auto& Keys = Dependencies.FindChecked(Owner);
		for (const auto& Key : Keys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Mix(GetTypeHash(Key));
			if (const int32* Index = ViewIndices.Find(Key))
			{
				LocalViews.Add(Views[*Index]);
				Mix(Views[*Index].Revision);
				Mix(Views[*Index].GeometryRevision);
				FHeightfieldTransitionTileSource Pin;
				if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
					Pin.Surface = SurfaceData.FindRef({Key.Coordinate, Key.Level});
				else
					Pin.Macro = MacroData.FindRef({Key.Coordinate, Key.Level});
				Pins.Add(MoveTemp(Pin));
			}
			else if (const int32* PendingIndex = PendingIndices.Find(Key))
			{
				Mix(0);
				LocalPending.Add(PendingTiles[*PendingIndex]);
			}
		}
		auto& State = HeightfieldCoveragePlans.FindOrAdd(Owner);
		State.DesiredSignature = Signature;
		if (State.Plan)
		{
			const auto PlanSnapshot = State.Plan;
			co_await VoxelGameThread::AppendSteps(Edges, PlanSnapshot->Edges);
		}
		if (!State.Plan || State.Plan->Signature != Signature)
		{
			for (const auto& Key : Keys)
				PendingHeightfieldPlanOwners.Add(Key);
			// 高度场拓扑使用不可变输入完成一次规划；Fine 的移动裁剪由发布器单独处理。
			if (State.PendingSignature != 0)
				continue;
			FVoxelTaskRequest Request;
			Request.Kind = EVoxelTaskKind::BuildViewCoverage;
			Request.WorkClass = EVoxelWorkClass::Boundary;
			Request.TerrainStage = Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3;
			if (Owner.Representation == EVoxelHeightfieldRepresentation::Surface)
			{
				if (const auto* Priority = GetViewInterest().SurfacePriorities.Find({Owner.Coordinate, Owner.Level}))
					Request.SourcePriority = *Priority;
			}
			else if (const auto* Priority = GetViewInterest().MacroPriorities.Find({Owner.Coordinate, Owner.Level}))
				Request.SourcePriority = *Priority;
			Request.Stamp.WorldEpoch = WorldEpoch;
			Request.Stamp.Section = FIntVector(Owner.Coordinate.X, Owner.Coordinate.Y, static_cast<int32>(Owner.Representation));
			Request.Stamp.ViewKey.Level = Owner.Level;
			Request.Stamp.Token = Signature;
			Request.ReservedBytes = 1024ull * 1024ull;
			Request.InputBytes = LocalViews.GetAllocatedSize() + LocalPending.GetAllocatedSize();
			Request.DistanceScore = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
										? MinimumObserverDistanceCells(SurfaceWorldCenter({Owner.Coordinate, Owner.Level}))
										: MinimumObserverDistanceCells(MacroWorldCenter({Owner.Coordinate, Owner.Level}));
			{
				FString CallbackBindingError;
				if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
					[Owner, LocalViews = MoveTemp(LocalViews), LocalPending = MoveTemp(LocalPending), Pins = MoveTemp(Pins), Signature](
								  const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load())
					return Result;
				auto Payload = MakeShared<FVoxelHeightfieldCoveragePlan, ESPMode::ThreadSafe>();
				Payload->Signature = Signature;
				if (!FVoxelHeightfieldTransitionBuilder::BuildEdges(LocalViews, Payload->Edges, Result.Error))
					return Result;
				FVoxelHeightfieldTransitionBuilder::AppendPendingEdges(LocalViews, LocalPending, Payload->Edges);
				Payload->Edges.RemoveAll(
					[Owner](const auto& Edge)
					{
						return !(Edge.Owner == Owner);
					});
				Result.CustomPayload = Payload;
				Result.bSuccess = !Cancel.Load();
				return Result;
			}, CallbackBindingError))
				{
					Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
						CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
							? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
					continue;
				}
			}
			const FVoxelCoverageRect OwnerRect = Owner.Representation == EVoxelHeightfieldRepresentation::Surface
													 ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
													 : MacroCoverageRect({Owner.Coordinate, Owner.Level});
			FVoxelCoverageLocalSnapshot Guard;
			co_await HeightfieldCoverageIndex.CaptureSteps(FIntVector(OwnerRect.Min.X - 1, OwnerRect.Min.Y - 1, Module.GetManifest().Settings.MinZ),
				FIntVector(OwnerRect.Max.X + 1, OwnerRect.Max.Y + 1, Module.GetManifest().Settings.MaxZ), Guard, false, false);
			if (Guard.bCountOnly) { RetryActiveCoverage(); co_return; }
			auto& ResumedState = HeightfieldCoveragePlans.FindChecked(Owner);
			const uint64 Serial = HeightfieldPlanSerial;
			{
				FString CallbackBindingError;
				if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
					[this, Serial, Owner, Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
			{
				if (Serial != HeightfieldPlanSerial || Result.Stamp.WorldEpoch != WorldEpoch)
					return;
				auto* Current = HeightfieldCoveragePlans.Find(Owner);
				if (!Current || Current->PendingSignature != Signature)
					return;
				Current->PendingSignature = 0;
				if (!HeightfieldCoverageIndex.IsCurrent(Guard))
				{
					MarkCoverageDirty(Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																									   : MacroCoverageRect({Owner.Coordinate, Owner.Level}));
					return;
				}
				if (Result.bSuccess && !Result.bCanceled &&
					(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
				{
					Current->Plan = StaticCastSharedPtr<const FVoxelHeightfieldCoveragePlan>(Result.CustomPayload);
				}
				MarkCoverageDirty(Owner.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Owner.Coordinate, Owner.Level})
																								   : MacroCoverageRect({Owner.Coordinate, Owner.Level}));
			}, CallbackBindingError))
				{
					Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
						CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
							? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
					continue;
				}
			}
			if (EnqueueViewTask(MoveTemp(Request)))
				ResumedState.PendingSignature = Signature;
			continue;
		}
	}
	if (bActiveCoverageFull)
	{
		for (auto It = HeightfieldCoveragePlans.CreateIterator(); It; ++It)
			if (!ViewIndices.Contains(It.Key()))
				It.RemoveCurrent();
	}
	else
		for (const auto& Key : LocalOwners)
			if (!ViewIndices.Contains(Key))
				HeightfieldCoveragePlans.Remove(Key);
	HeightfieldUnbalancedEdgeCount = 0;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		HeightfieldUnbalancedEdgeCount += Edge.bUnbalanced ? 1 : 0;
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> Revisions;
	for (const FVoxelHeightfieldTileView& View : Views)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Revisions.Add(View.Key, View.Revision);
	}
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>> NewEdges;
	for (const FVoxelHeightfieldTransitionEdge& Edge : Edges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		NewEdges.FindOrAdd(Edge.Owner).Add(Edge);
	}
	TMap<FVoxelHeightfieldNodeKey, uint64> NewSignatures;
	TMap<FVoxelHeightfieldNodeKey, TSharedPtr<const FVoxelFrozenHeightfieldInputs, ESPMode::ThreadSafe>> NewInputs;
	TArray<FVoxelHeightfieldNodeKey> NewUnsubmitted;
	for (const auto& Pair : NewEdges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		check(NextHeightfieldInputSerial < MAX_uint64);
		const uint64 Serial = ++NextHeightfieldInputSerial;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
			FIntVector(Pair.Key.Coordinate.X, Pair.Key.Coordinate.Y, int32(Pair.Key.Representation)), Serial);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFrozenHeightfieldInputs>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelFrozenHeightfieldSource>(Pair.Value.Num() + 1) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelHeightfieldTransitionEdge>(Pair.Value.Num()) + 512;
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, InputBytes, InputLease))
		{
			MarkCoverageDirty(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ?
				SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}) : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}));
			continue;
		}
		auto Inputs = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFrozenHeightfieldInputs>(InputLease);
		Inputs->ResourceLease = MoveTemp(InputLease);
		Inputs->Owner = Pair.Key;
		Inputs->WorldEpoch = WorldEpoch;
		Inputs->GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Inputs->RegistryRevision = Module.GetRegistry()->Hash;
		Inputs->ShapeRevision = Module.GetShapes()->GetContentHash();
		Inputs->Sources.Reserve(Pair.Value.Num() + 1);
		Inputs->Edges.Reserve(Pair.Value.Num());
		Inputs->Edges.Append(Pair.Value);
		const auto Pin = [&](const FVoxelHeightfieldNodeKey& Key)
		{
			if (Inputs->Sources.ContainsByPredicate([&Key](const auto& Source) { return Source.Key == Key; })) return;
			FVoxelFrozenHeightfieldSource Source;
			Source.Key = Key;
			if (const int32* Index = ViewIndices.Find(Key))
			{
				Source.Surface = ViewPins[*Index].Surface;
				Source.Macro = ViewPins[*Index].Macro;
			}
			Inputs->Sources.Add(MoveTemp(Source));
		};
		Pin(Pair.Key);
		for (const auto& Edge : Pair.Value) if (!Edge.bPending) Pin(Edge.Neighbor);
		const auto Previous = DesiredHeightfieldInputs.FindRef(Pair.Key);
		Inputs->InputSerial = Previous && Previous->SameInputs(*Inputs) ? Previous->InputSerial : Serial;
		Inputs->FirstQueuedSeconds = Previous && Previous->SameInputs(*Inputs) ? Previous->FirstQueuedSeconds : FPlatformTime::Seconds();
		const uint64 Signature = Inputs->InputSerial;
		NewInputs.Add(Pair.Key, Inputs);
		NewSignatures.Add(Pair.Key, Signature);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		const uint64* Pending = PendingTransitionSignatures.Find(Pair.Key);
		const uint64* Prepared = PreparedTransitionSignatures.Find(Pair.Key);
		if ((!Built || *Built != Signature) && (!Pending || *Pending != Signature) && (!Prepared || *Prepared != Signature))
		{
			NewUnsubmitted.Add(Pair.Key);
		}
	}
	if (bActiveCoverageFull)
	{
		for (const auto& Pair : DesiredTransitionSignatures)
			LocalOwners.Add(Pair.Key);
		DesiredTransitionSignatures.Reset();
		DesiredHeightfieldInputs.Reset();
		DesiredTransitionEdges.Reset();
		UnsubmittedTransitionOwners.Reset();
	}
	for (const auto& Key : LocalOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Desired = NewSignatures.Find(Key);
		if (!Desired || PreparedTransitionSignatures.FindRef(Key) != *Desired)
		{
			PreparedTransitionMeshes.Remove(Key);
			PreparedTransitionSignatures.Remove(Key);
		}
		DesiredTransitionSignatures.Remove(Key);
		DesiredHeightfieldInputs.Remove(Key);
		DesiredTransitionEdges.Remove(Key);
		UnsubmittedTransitionOwners.RemoveSingleSwap(Key, EAllowShrinking::No);
	}
	for (const auto& Pair : NewSignatures)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredTransitionSignatures.Add(Pair.Key, Pair.Value);
	}
	for (auto& Pair : NewInputs)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredHeightfieldInputs.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	for (auto& Pair : NewEdges)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredTransitionEdges.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	co_await VoxelGameThread::SortSteps(NewUnsubmitted,
		[this](const FVoxelHeightfieldNodeKey& A, const FVoxelHeightfieldNodeKey& B)
		{
			const bool bASurface = A.Representation == EVoxelHeightfieldRepresentation::Surface;
			const bool bBSurface = B.Representation == EVoxelHeightfieldRepresentation::Surface;
			if (bASurface != bBSurface)
				return !bASurface;
			const double DistanceA = bASurface ? MinimumObserverDistanceCells(SurfaceWorldCenter({A.Coordinate, A.Level}))
											   : MinimumObserverDistanceCells(MacroWorldCenter({A.Coordinate, A.Level}));
			const double DistanceB = bBSurface ? MinimumObserverDistanceCells(SurfaceWorldCenter({B.Coordinate, B.Level}))
											   : MinimumObserverDistanceCells(MacroWorldCenter({B.Coordinate, B.Level}));
			return DistanceA > DistanceB;
		});
	for (const auto& Key : NewUnsubmitted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UnsubmittedTransitionOwners.AddUnique(Key);
	}
	Scheduler.CancelMatching(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			if (Kind != EVoxelTaskKind::BuildViewTransition)
				return false;
			const FVoxelHeightfieldNodeKey Owner{
				static_cast<EVoxelHeightfieldRepresentation>(Stamp.Section.Z), FIntPoint(Stamp.Section.X, Stamp.Section.Y), Stamp.ViewKey.Level};
			const uint64* WantedSignature = DesiredTransitionSignatures.Find(Owner);
			return !WantedSignature || *WantedSignature != Stamp.Token;
		});
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::RebuildVolumeTransitionSteps(const TSet<FVoxelViewKey>& InTargetNodes)
{
	// Fine 的局部面由 Native 同帧根事务接管。粗域拓扑规划只处理 Proxy，不能因 F0/F1 切换重建整份 Proxy body。
	TSet<FVoxelViewKey> CoarseNodes;
	for (const auto& Node : InTargetNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Node.Level > 0) CoarseNodes.Add(Node);
	}
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
	co_await FVoxelVolumeTransitionPlanner::GatherCoverageRegionsSteps(CoarseNodes, Regions);
	if (bActiveCoverageFull)
		PendingVolumeHandoffBoxes.Reset();
	else
		PendingVolumeHandoffBoxes.RemoveAll(
			[this](const FVoxelCoverageBox& Box)
			{
				return CoverageAffects({FIntPoint(Box.Min.X, Box.Min.Y), FIntPoint(Box.Max.X, Box.Max.Y)});
			});
	TSet<FVoxelViewKey> ChangedTopologyNodes;
	TArray<FVoxelVolumeTransitionFace> Faces;
	for (const auto& Pair : Regions)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelViewKey Region = Pair.Key;
		if (!CoverageAffects(VoxelProxyCoverageRect(Region)))
			continue;
		auto& State = VolumeCoveragePlans.FindOrAdd(Region);
		auto RegionRecord = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		const auto RegionBounds = Region.GetBounds();
		RegionRecord->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Region.Coordinate, Region.Level}, 2, Module.GetManifest().Settings.MinZ,
			Module.GetManifest().Settings.MaxZ, 0, 0};
		RegionRecord->Min = RegionBounds.Min;
		RegionRecord->Max = RegionBounds.Max;
		FString RegionIndexError;
		VolumePlanIndex.Upsert(RegionRecord, RegionIndexError);
		uint64 Signature = 0;
		for (const auto& Node : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			Signature ^= VoxelGeneration::Mix(GetTypeHash(Node));
			if (Node.Level > 0)
				if (const auto* Geometry = GetViewInterest().FindGeometry(Node))
					Signature ^= VoxelGeneration::Mix(GetTypeHash(*Geometry));
		}
		Signature = VoxelGeneration::Mix(Signature ^ Pair.Value.Num());
		State.DesiredSignature = Signature;
		if (State.Plan)
		{
			const auto PlanSnapshot = State.Plan;
			co_await VoxelGameThread::AppendSteps(Faces, PlanSnapshot->Faces);
		}
		if (State.Plan && State.Plan->Signature == Signature && State.Plan->Nodes.Includes(Pair.Value) && State.Plan->Nodes.Num() == Pair.Value.Num())
		{
			continue;
		}
		// 同精度 Fine 使用 section 邻接网格，不存在 LOD 过渡面；规划快照更新不阻止其取得所有权。
		auto NeedsTopologyHandoff = [this, Region](const FVoxelViewKey& Node, const TSet<FVoxelViewKey>& Nodes)
		{
			if (Node.Level > 0)
				return true;
			const FVoxelGenerationBounds Bounds = Node.GetBounds();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				for (const int32 Sign : {-1, 1})
				{
					FIntVector SameLevel = Node.Coordinate;
					SameLevel[Axis] += Sign;
					if (Nodes.Contains({SameLevel, 0}))
						continue;
					FIntVector Point = Bounds.Min + FIntVector(8);
					Point[Axis] = Sign > 0 ? Bounds.Max[Axis] : Bounds.Min[Axis] - 1;
					for (uint8 Level = 1; Level <= Region.Level; ++Level)
					{
						const int32 Side = 16 << Level;
						if (Nodes.Contains({FIntVector(VoxelGeneration::FloorDivide(Point.X, Side), VoxelGeneration::FloorDivide(Point.Y, Side),
												VoxelGeneration::FloorDivide(Point.Z, Side)),
								Level}))
							return true;
					}
				}
			}
			return false;
		};
		bool bHasProxy = false;
		for (const auto& Node : Pair.Value)
			if (Node.Level > 0)
			{
				bHasProxy = true;
				break;
			}
		bool bHadProxy = false;
		if (State.Plan)
			for (const auto& Node : State.Plan->Nodes)
				if (Node.Level > 0)
				{
					bHadProxy = true;
					break;
				}
		if (bHasProxy || bHadProxy)
			for (const FVoxelViewKey& Node : Pair.Value)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if ((!State.Plan || !State.Plan->Nodes.Contains(Node)) &&
					(NeedsTopologyHandoff(Node, Pair.Value) || (State.Plan && NeedsTopologyHandoff(Node, State.Plan->Nodes))))
					ChangedTopologyNodes.Add(Node);
			}
		if (State.Plan && (bHasProxy || bHadProxy))
		{
			for (const FVoxelViewKey& Node : State.Plan->Nodes)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!Pair.Value.Contains(Node) && (NeedsTopologyHandoff(Node, Pair.Value) || NeedsTopologyHandoff(Node, State.Plan->Nodes)))
					ChangedTopologyNodes.Add(Node);
			}
		}
		// 保留正在计算的不可变快照；完成后只为最新差异补充任务，避免移动时持续取消。
		if (State.PendingSignature != 0)
			continue;
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildViewCoverage;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.TerrainStage = 1;
		Request.SourcePriority = MAX_int32;
		for (const auto& Node : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Node.Level == 0)
				Request.TerrainStage = 0;
			if (const auto* Demand = Node.Level == 0 ? GetViewInterest().Exact.Find(Node.Coordinate) : nullptr)
			{
				Request.SourcePriority = FMath::Min(Request.SourcePriority, Demand->Priority);
				if (Demand->bWarmupData || Demand->bWarmupCollision || Demand->bMovementCriticalData || Demand->bMovementCriticalCollision ||
					Demand->bMovementCriticalFine)
					Request.WorkClass = EVoxelWorkClass::Critical;
			}
			else if (const auto* Priority = GetViewInterest().VoxelProxyPriorities.Find(Node))
				Request.SourcePriority = FMath::Min(Request.SourcePriority, *Priority);
		}
		Request.Stamp.WorldEpoch = WorldEpoch;
		Request.Stamp.Section = FIntVector(Region.Coordinate.X, Region.Coordinate.Y, -1);
		Request.Stamp.ViewKey = Region;
		Request.Stamp.Token = Signature;
		Request.ReservedBytes = 4ull * 1024ull * 1024ull;
		Request.InputBytes = Pair.Value.GetAllocatedSize();
		Request.DistanceScore = MinimumObserverDistanceCells(Region.GetBounds());
		TMap<FVoxelViewKey, int32> SampleSteps;
		for (const auto& Node : Pair.Value)
			if (Node.Level == 0)
				SampleSteps.Add(Node, 1);
			else if (const auto* Geometry = GetViewInterest().FindGeometry(Node))
				SampleSteps.Add(Node, Geometry->SampleStepCells);
		{
			FString CallbackBindingError;
			if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[Region, Nodes = Pair.Value, Signature, SampleSteps = MoveTemp(SampleSteps)](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load())
				return Result;
			auto Payload = MakeShared<FVoxelVolumeCoveragePlan, ESPMode::ThreadSafe>();
			Payload->Signature = Signature;
			Payload->Nodes = Nodes;
			uint8 MaximumLevel = 0;
			for (const FVoxelViewKey& Key : Nodes)
				MaximumLevel = FMath::Max(MaximumLevel, Key.Level);
			FVoxelVolumeTransitionPlanner::Build(Nodes, MaximumLevel, Payload->Faces, &SampleSteps);
			Payload->Faces.RemoveAll(
				[Region](const auto& Face)
				{
					FVoxelViewKey Owner = Face.Owner;
					while (Owner.Level < Region.Level)
						Owner = Owner.GetParent();
					return !(Owner == Region);
				});
			Result.CustomPayload = Payload;
			Result.bSuccess = !Cancel.Load();
			return Result;
		}, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		const auto Bounds = Region.GetBounds();
		FVoxelCoverageLocalSnapshot Guard;
		CoverageIndex.Capture(Bounds.Min - FIntVector(1), Bounds.Max + FIntVector(1), Guard, false);
		const uint64 Serial = VolumePlanSerial;
		{
			FString CallbackBindingError;
			if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
				[this, Serial, Region, Signature, Guard = MoveTemp(Guard)](FVoxelTaskResult&& Result)
		{
			if (Serial != VolumePlanSerial || Result.Stamp.WorldEpoch != WorldEpoch)
				return;
			auto* Current = VolumeCoveragePlans.Find(Region);
			if (!Current || Current->PendingSignature != Signature)
				return;
			Current->PendingSignature = 0;
			if (!CoverageIndex.IsCurrent(Guard))
			{
				MarkCoverageDirty(VoxelProxyCoverageRect(Region));
				return;
			}
			if (Result.bSuccess && !Result.bCanceled &&
				(!Current->Plan || Current->Plan->Signature != Current->DesiredSignature || Signature == Current->DesiredSignature))
			{
				Current->Plan = StaticCastSharedPtr<const FVoxelVolumeCoveragePlan>(Result.CustomPayload);
			}
			MarkCoverageDirty(VoxelProxyCoverageRect(Region));
		}, CallbackBindingError))
			{
				Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
					CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
						? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
				continue;
			}
		}
		if (EnqueueViewTask(MoveTemp(Request)))
			State.PendingSignature = Signature;
	}
	TSet<FVoxelViewKey> PreviousRegions;
	if (bActiveCoverageFull)
		for (const auto& Pair : VolumeCoveragePlans)
			PreviousRegions.Add(Pair.Key);
	else
		for (const auto& Rect : ActiveDirtyCoverageRects)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			FVoxelCoverageLocalSnapshot LocalPlans;
			VolumePlanIndex.Capture(FIntVector(Rect.Min.X, Rect.Min.Y, MIN_int32), FIntVector(Rect.Max.X, Rect.Max.Y, MAX_int32), LocalPlans);
			for (const auto& Record : LocalPlans.Records)
				PreviousRegions.Add({Record->Geometry.Key.Coordinate, Record->Geometry.Key.Level});
		}
	for (const auto& Region : PreviousRegions)
		if (!Regions.Contains(Region))
		{
			VolumeCoveragePlans.Remove(Region);
			VolumePlanIndex.Remove({EVoxelTerrainRepresentation::VoxelProxy, Region.Coordinate, Region.Level});
		}
	for (const FVoxelViewKey& Node : ChangedTopologyNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		FVoxelCoverageBox Bounds = Node.Level == 0 ? FineCoverageBox(Node.Coordinate) : VoxelProxyCoverageBox(Node);
		Bounds.Min -= FIntVector(1, 1, 1);
		Bounds.Max += FIntVector(1, 1, 1);
		PendingVolumeHandoffBoxes.Add(Bounds);
	}
	VolumeUnbalancedFaceCount = 0;
	MissingFineBoundaryCount = 0;
	MissingProxyBoundaryCount = 0;
	InvalidVolumeContextCount = 0;
	TMap<FVoxelViewKey, TSharedPtr<FVoxelBoundaryTransitionContext>> Contexts;
	TSet<FVoxelViewKey> BlockedOwners;
	const auto NewContext = [&](const FVoxelViewKey& Owner) -> TSharedPtr<FVoxelBoundaryTransitionContext>
	{
		check(NextVolumeInputSerial < MAX_uint64);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
			Owner.Coordinate, ++NextVolumeInputSerial);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelBoundaryTransitionContext>(), Lease)) return nullptr;
		auto Context = VoxelBoundedAllocation::MakeWorkerShared<FVoxelBoundaryTransitionContext>(Lease);
		Context->ResourceLease = MoveTemp(Lease);
		Context->Owner = Owner;
		Context->WorldEpoch = WorldEpoch;
		Context->GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Context->RegistryRevision = Module.GetRegistry()->Hash;
		Context->ShapeRevision = Module.GetShapes()->GetContentHash();
		Context->MaximumTextureStretchCells = Module.GetViewSettings().MaximumTextureStretchCells;
		return Context;
	};
	const auto GrowContext = [&](FVoxelBoundaryTransitionContext& Context, const int32 PatchCount,
		const int32 SuppressionCount, const int32 SourceCount)
	{
		using namespace VoxelBoundedAllocation;
		const auto Capacity = [](const int32 Previous, const int32 Needed)
		{
			return Previous >= Needed ? Previous : int32(FMath::RoundUpToPowerOfTwo(uint32(FMath::Max(Needed, 4))));
		};
		const int32 Patches = Capacity(Context.Patches.Max(), PatchCount);
		const int32 Suppressions = Capacity(Context.SuppressedFaces.Max(), SuppressionCount);
		const int32 Sources = Capacity(Context.SuppressionSources.Max(), SourceCount);
		if (Patches == Context.Patches.Max() && Suppressions == Context.SuppressedFaces.Max() && Sources == Context.SuppressionSources.Max()) return true;
		const uint64 OldBytes = Context.Patches.GetAllocatedSize() + Context.SuppressedFaces.GetAllocatedSize() + Context.SuppressionSources.GetAllocatedSize();
		const uint64 Bytes = WorkerSharedBytes<FVoxelBoundaryTransitionContext>() + ArrayBytes<FVoxelBoundaryTransitionPatch>(Patches) +
			ArrayBytes<FVoxelVolumeTransitionFace>(Suppressions) + ArrayBytes<TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe>>(Sources);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, TaskOwnerId, TaskOwnerIncarnation,
			Context.Owner.Coordinate, NextVolumeInputSerial);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Overlap;
		if ((OldBytes && !Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Working, OldBytes, Overlap)) ||
			!Context.ResourceLease->Resize(Bytes)) return false;
		Context.Patches.Reserve(Patches); Context.SuppressedFaces.Reserve(Suppressions); Context.SuppressionSources.Reserve(Sources);
		return true;
	};
	// 拓扑依赖不足与网格尚未发布不同；后者只阻止交接，不阻止关联网格继续准备。
	const TArray<FVoxelCoverageBox> PendingTopologyBoxes = PendingVolumeHandoffBoxes;
	for (const FVoxelVolumeTransitionFace& Face : Faces)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Face.Owner.Level == 0 || Face.Neighbor.Level == 0) continue;
		if (!InTargetNodes.Contains(Face.Owner) || !InTargetNodes.Contains(Face.Neighbor))
			continue;
		if (VoxelCoverage::IntersectsAny3D(
				Face.Neighbor.Level == 0 ? FineCoverageBox(Face.Neighbor.Coordinate) : VoxelProxyCoverageBox(Face.Neighbor), PendingTopologyBoxes))
		{
			BlockedOwners.Add(Face.Owner);
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner));
			continue;
		}
		VolumeUnbalancedFaceCount += Face.Ratio > 2 ? 1 : 0;
		FVoxelBoundaryFaceSnapshot Boundary;
		const uint8 NeighborFace = static_cast<uint8>(Face.Direction) ^ 1;
		bool bCaptured = false;
		if (Face.Neighbor.Level == 0)
		{
			bCaptured = FVoxelBoundaryFaceSnapshot::CaptureFineShardAtom(Face.Neighbor, NeighborFace,
				FindFineBoundaryGeometry(Face.Neighbor.Coordinate), Boundary);
		}
		else if (const auto Data = VoxelProxyData.FindRef(Face.Neighbor))
		{
			bCaptured = FVoxelBoundaryFaceSnapshot::CaptureProxyFrozen(Data, NeighborFace, Boundary);
		}
		if (!bCaptured)
		{
			if (Face.Neighbor.Level == 0)
				++MissingFineBoundaryCount;
			else
				++MissingProxyBoundaryCount;
			BlockedOwners.Add(Face.Owner);
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext>& Context = Contexts.FindOrAdd(Face.Owner);
		if (!Context)
		{
			Context = NewContext(Face.Owner);
			if (!Context) { BlockedOwners.Add(Face.Owner); PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner)); continue; }
			if (const auto OwnerData = VoxelProxyData.FindRef(Face.Owner))
			{
				Context->SampleStepCells = OwnerData->Geometry.SampleStepCells;
				Context->OwnerGeometry = OwnerData;
			}
		}
		const int32 FaceAxis = static_cast<uint8>(Face.Direction) / 2;
		const int32 TangentU = (FaceAxis + 1) % 3, TangentV = (FaceAxis + 2) % 3;
		for (int32 V = Face.Min[TangentV]; V < Face.Max[TangentV]; V += 16)
			for (int32 U = Face.Min[TangentU]; U < Face.Max[TangentU]; U += 16)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!GrowContext(*Context, Context->Patches.Num() + 1, Context->SuppressedFaces.Num(), Context->SuppressionSources.Num()))
				{
					BlockedOwners.Add(Face.Owner); PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Face.Owner)); continue;
				}
				auto Tile = Face;
				Tile.Min[TangentU] = U; Tile.Max[TangentU] = FMath::Min(U + 16, Face.Max[TangentU]);
				Tile.Min[TangentV] = V; Tile.Max[TangentV] = FMath::Min(V + 16, Face.Max[TangentV]);
				Context->Patches.Add({Tile, Boundary});
			}
		if (Face.Neighbor.Level > 0)
		{
			auto& Opposite = Contexts.FindOrAdd(Face.Neighbor);
			if (!Opposite)
			{
				Opposite = NewContext(Face.Neighbor);
				if (!Opposite) { BlockedOwners.Add(Face.Owner); BlockedOwners.Add(Face.Neighbor); continue; }
			}
			if (!GrowContext(*Opposite, Opposite->Patches.Num(), Opposite->SuppressedFaces.Num() + 1, Opposite->SuppressionSources.Num() + 1))
			{ BlockedOwners.Add(Face.Owner); BlockedOwners.Add(Face.Neighbor); continue; }
			if (const auto OtherData = VoxelProxyData.FindRef(Face.Neighbor))
				Opposite->SampleStepCells = OtherData->Geometry.SampleStepCells;
			auto Suppression = Face;
			Suppression.Owner = Face.Neighbor;
			Suppression.Neighbor = Face.Owner;
			Suppression.Direction = static_cast<EVoxelVolumeFaceDirection>(static_cast<uint8>(Face.Direction) ^ 1);
			const int32 Axis = static_cast<uint8>(Face.Direction) / 2;
			const int32 Plane = (static_cast<uint8>(Face.Direction) & 1) ? Face.Max[Axis] : Face.Min[Axis];
			Suppression.Min[Axis] = Plane;
			Suppression.Max[Axis] = Plane;
			Opposite->SuppressedFaces.Add(Suppression);
			if (const auto OwnerData = VoxelProxyData.FindRef(Face.Owner))
			{
				Opposite->SuppressionSources.AddUnique(OwnerData);
				Opposite->SuppressionRevision = HashCombineFast(
					GetTypeHash(Opposite->SuppressionRevision), HashCombineFast(GetTypeHash(OwnerData->Revision), GetTypeHash(OwnerData->Geometry)));
			}
		}
	}
	TMap<FVoxelViewKey, uint64> NewSignatures;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>> NewContexts;
	TArray<FVoxelViewKey> NewUnsubmitted;
	for (const FVoxelViewKey& Key : InTargetNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		if (!CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		// 全空气节点保留空间所有权和邻接数据，但不会产生接缝几何或网格交接。
		if (BlockedOwners.Contains(Key) || VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingTopologyBoxes))
			continue;
		const auto Data = VoxelProxyData.FindRef(Key);
		if (!Data)
		{
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		TSharedPtr<FVoxelBoundaryTransitionContext> Context = Contexts.FindRef(Key);
		if (!Context)
		{
			Context = NewContext(Key);
			if (!Context) { PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key)); continue; }
			Context->SampleStepCells = Data->Geometry.SampleStepCells;
		}
		Context->SampleStepCells = Data->Geometry.SampleStepCells;
		Context->OwnerGeometry = Data;
		if (!Context->Validate())
		{
			++InvalidVolumeContextCount;
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			continue;
		}
		const auto PreviousContext = DesiredVolumeContexts.FindRef(Key);
		if (PreviousContext && PreviousContext->SameInputs(*Context))
		{
			Context->InputSerial = PreviousContext->InputSerial;
			Context->FirstQueuedSeconds = PreviousContext->FirstQueuedSeconds;
		}
		else
		{
			check(NextVolumeInputSerial < MAX_uint64);
			Context->InputSerial = ++NextVolumeInputSerial;
			Context->FirstQueuedSeconds = FPlatformTime::Seconds();
		}
		// 完整冻结输入决定 serial；hash 只用于查找，不能承担任务身份。
		const uint64 Signature = Context->InputSerial;
		NewSignatures.Add(Key, Signature);
		NewContexts.Add(Key, PreviousContext && PreviousContext->SameInputs(*Context) ? PreviousContext : Context);
		const uint64* Built = VolumeTransitionSignatures.Find(Key);
		const uint64* Pending = PendingVolumeSignatures.Find(Key);
		const uint64* Prepared = PreparedVolumeSignatures.Find(Key);
		if ((!Built || *Built != Signature) && (!Prepared || *Prepared != Signature))
		{
			PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Key));
			if (!Pending || *Pending != Signature)
				NewUnsubmitted.Add(Key);
		}
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	bool bCaptured = false;
	TSet<FVoxelViewKey> LocalOwners;
	if (bActiveCoverageFull)
	{
		for (const auto& Pair : DesiredVolumeSignatures)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			LocalOwners.Add(Pair.Key);
		}
		DesiredVolumeSignatures.Reset();
		DesiredVolumeContexts.Reset();
		UnsubmittedVolumeOwners.Reset();
	}
	else
	{
		TSet<FIntVector> Fine;
		TSet<FVoxelSurfaceTileKey> Surface;
		TSet<FVoxelMacroTileKey> Macro;
		co_await CaptureCoverageCandidateSteps(Fine, LocalOwners, Surface, Macro, CandidateLease, bCaptured);
		if (!bCaptured) { RetryActiveCoverage(); co_return; }
	}
	co_await VoxelGameThread::AppendSteps(LocalOwners, InTargetNodes);
	for (const auto& Key : LocalOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		if (!CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		const auto* Desired = NewSignatures.Find(Key);
		if (!Desired || PreparedVolumeSignatures.FindRef(Key) != *Desired)
		{
			PreparedVolumeMeshes.Remove(Key);
			PreparedVolumeBoundaryMeshes.Remove(Key);
			PreparedVolumeSignatures.Remove(Key);
		}
		DesiredVolumeSignatures.Remove(Key);
		DesiredVolumeContexts.Remove(Key);
		UnsubmittedVolumeOwners.RemoveSingleSwap(Key, EAllowShrinking::No);
	}
	for (const auto& Pair : NewSignatures)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredVolumeSignatures.Add(Pair.Key, Pair.Value);
	}
	for (auto& Pair : NewContexts)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		DesiredVolumeContexts.Add(Pair.Key, MoveTemp(Pair.Value));
	}
	co_await VoxelGameThread::SortSteps(NewUnsubmitted,
		[this](const FVoxelViewKey& A, const FVoxelViewKey& B)
		{
			const EVoxelWorkClass ClassA = VolumeTransitionWorkClass(A);
			const EVoxelWorkClass ClassB = VolumeTransitionWorkClass(B);
			if (ClassA != ClassB)
				return static_cast<uint8>(ClassA) > static_cast<uint8>(ClassB);
			return MinimumObserverDistanceCells(A.GetBounds()) > MinimumObserverDistanceCells(B.GetBounds());
		});

	for (const auto& Key : NewUnsubmitted)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UnsubmittedVolumeOwners.AddUnique(Key);
	}
	Scheduler.CancelMatching(
		[this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			if (Kind != EVoxelTaskKind::BuildVolumeTransition)
				return false;
			const uint64* Desired = DesiredVolumeSignatures.Find(Stamp.ViewKey);
			return !Desired || *Desired != Stamp.Token;
		});
	co_return;
}

FVoxelGameThreadContinuation FVoxelViewManager::ResolveTransitionVisibilitySteps()
{
	bCoverageRetryRequested = false;
	ON_SCOPE_EXIT
	{
		if (bCoverageRetryRequested)
		{
			// 失败发生在提交之前；归还原分组，下一轮按最新输入重新计算覆盖。
			for (const auto& Pair : ActiveCoveragePublishGroups)
			{
				check(!OpenPublishGroups.Contains(Pair.Key));
				OpenPublishGroups.Add(Pair.Key, Pair.Value);
			}
			ActiveCoveragePublishGroups.Reset();
			bHeightfieldTopologyDirty = true;
		}
	};
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ViewHandoff);
	const double ResolveStart = FPlatformTime::Seconds();
	const auto ContentSnapshot = Module.GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot();
	PrepareFineShardPresentations();
	const auto PresentedFine = CapturePresentedSnapshot();
	const FVoxelTerrainViewPlan& Plan = GetViewInterest().TerrainPlan;
	const bool bUsesTerrainPlan = !Plan.Roots.IsEmpty();
	FCoveragePassSnapshot CoverageSnapshot;
	TSet<FVoxelViewKey> TerrainActorCandidates;
	TSet<FVoxelViewKey> TargetTerrainNodes;
	TSet<FVoxelViewKey> PreviousTerrainNodes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	bool bCaptured = false;
	TSet<FIntVector> LocalFineKeys;
	TSet<FVoxelViewKey> LocalProxyKeys;
	TSet<FVoxelSurfaceTileKey> LocalSurfaceKeys;
	TSet<FVoxelMacroTileKey> LocalMacroKeys;
	TSet<FVoxelViewKey> ResolveRoots;
	co_await CaptureCoverageCandidateSteps(LocalFineKeys, LocalProxyKeys, LocalSurfaceKeys, LocalMacroKeys, CandidateLease, bCaptured);
	if (!bCaptured) { RetryActiveCoverage(); co_return; }
	for (const auto& Key : LocalFineKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (VisibleTerrainNodes.Contains({Key, 0}))
			PreviousTerrainNodes.Add({Key, 0});
	}
	for (const auto& Key : LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (VisibleTerrainNodes.Contains(Key))
			PreviousTerrainNodes.Add(Key);
	}
	if (!bActiveCoverageFull)
	{
		for (const auto& Rect : ActiveDirtyCoverageRects)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			FVoxelCoverageLocalSnapshot Roots;
			co_await GetInputRoot().TerrainRootIndex.CaptureSteps(FIntVector(Rect.Min.X, Rect.Min.Y, MIN_int32 + 1), FIntVector(Rect.Max.X, Rect.Max.Y, MAX_int32), Roots, true, false);
			if (Roots.bCountOnly) { RetryActiveCoverage(); co_return; }
			for (const auto& Record : Roots.Records)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				ResolveRoots.Add({Record->Geometry.Key.Coordinate, Record->Geometry.Key.Level});
			}
		}
	}
	if (bUsesTerrainPlan)
	{
		const TSet<FVoxelViewKey>& Previous = PreviousTerrainNodes;
		const uint8 RootLevel = GetInputRoot().MaximumTerrainRootLevel;
		TSet<FVoxelViewKey> PreviousScope;
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Key : Previous)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				FVoxelViewKey Root = Key;
				while (Root.Level < RootLevel && !Plan.Roots.Contains(Root))
					Root = Root.GetParent();
				if (ResolveRoots.Contains(Root) ||
					(!Plan.Roots.Contains(Root) && CoverageAffects(Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key))))
					PreviousScope.Add(Key);
			}
		}
		TSet<FVoxelViewKey> LocalReadyBranches;
		const auto TrackBranch = [&LocalReadyBranches](FVoxelViewKey Key)
		{
			while (Key.Level <= 8)
			{
				bool bExists = false;
				LocalReadyBranches.Add(Key, &bExists);
				if (bExists)
					break;
				Key = Key.GetParent();
			}
		};
		for (const auto& Key : LocalFineKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (IsFineCoverageReady(Key, PresentedFine.Get()))
				TrackBranch({Key, 0});
		}
		for (const auto& Key : LocalProxyKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (VoxelProxyReady.Contains(Key))
				TrackBranch(Key);
		}
		auto IsReady = [this, &Plan, &Previous, &PresentedFine](const FVoxelViewKey& Key)
		{
			if (!Plan.Leaves.Contains(Key) && !Previous.Contains(Key))
				return false;
			if (Key.Level > 0)
				return VoxelProxyReady.Contains(Key);
			return IsFineCoverageReady(Key.Coordinate, PresentedFine.Get());
		};
		if (bActiveCoverageFull)
		{
			co_await Plan.ResolveVisibleSteps(IsReady, TargetTerrainNodes, &Previous, &LocalReadyBranches, nullptr);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, Previous);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, TargetTerrainNodes);
		}
		else
		{
			co_await Plan.ResolveVisibleSteps(IsReady, TargetTerrainNodes, &PreviousScope, &LocalReadyBranches, &ResolveRoots);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, PreviousScope);
			co_await VoxelGameThread::AppendSteps(TerrainActorCandidates, TargetTerrainNodes);
			for (const FVoxelViewKey& Key : Previous)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!PreviousScope.Contains(Key))
					TargetTerrainNodes.Add(Key);
			}
		}
		co_await BuildCoverageSnapshotSteps(CoverageSnapshot);
		if (!CoverageSnapshot.bComplete) { RetryActiveCoverage(); co_return; }
		const double TreeMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
		TArray<FVoxelCoverageRect> OutsideTerrainDomain;
		TArray<FVoxelCoverageRect> ReadyOutsideCoverage;
		TArray<FVoxelCoverageBox> ReadyFineCoverage;
		TArray<FVoxelCoverageBox> ReadyProxyCoverage;
		bool bOutsideTerrainDomainBuilt = false;
		for (const FVoxelViewKey& Key : Previous)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!bActiveCoverageFull && !PreviousScope.Contains(Key))
				continue;
			FVoxelViewKey Root = Key;
			while (Root.Level < RootLevel && !Plan.Roots.Contains(Root))
			{
				co_await FVoxelGameThreadContinuation::Yield();
				Root = Root.GetParent();
			}
			if (Plan.Roots.Contains(Root))
			{
				continue;
			}
			if (!bOutsideTerrainDomainBuilt)
			{
				OutsideTerrainDomain.Reserve(GetInputRoot().SurfaceWanted.Num() + GetInputRoot().MacroWanted.Num());
				for (const FVoxelSurfaceTileKey& SurfaceKey : LocalSurfaceKeys)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					OutsideTerrainDomain.Add(SurfaceCoverageRect(SurfaceKey));
				}
				for (const FVoxelMacroTileKey& MacroKey : LocalMacroKeys)
				{
					co_await FVoxelGameThreadContinuation::Yield();
					OutsideTerrainDomain.Add(MacroCoverageRect(MacroKey));
				}
				co_await VoxelGameThread::AppendSteps(ReadyOutsideCoverage, CoverageSnapshot.Heightfield);
				bOutsideTerrainDomainBuilt = true;
			}
			const FVoxelCoverageRect PreviousRect = Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key);
			if (!VoxelCoverage::IntersectsAny2D(PreviousRect, OutsideTerrainDomain))
			{
				continue;
			}
			bool bReplacement = false;
			co_await IsTerrainVolumeCoveredSteps(Key, CoverageSnapshot, Key.Level > 0, bReplacement);
			if (!bReplacement)
				co_await VoxelCoverage::IsFullyCovered2DSteps(PreviousRect, ReadyOutsideCoverage, bReplacement);
			const bool bRetain = !bReplacement;
			if (bRetain)
			{
				TargetTerrainNodes.Add(Key);
			}
		}
		const double PlanNow = FPlatformTime::Seconds();
		if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && PlanNow >= NextPlanTimingLog)
		{
			UE_LOG(LogTemp, Display, TEXT("Voxel plan phase elapsed ms: tree=%.3f retain=%.3f previous=%d visible=%d"), TreeMilliseconds,
				(PlanNow - ResolveStart) * 1000.0 - TreeMilliseconds, Previous.Num(), TargetTerrainNodes.Num());
			NextPlanTimingLog = PlanNow + 5.0;
		}
	}
	if (!bUsesTerrainPlan)
	{
		co_await BuildCoverageSnapshotSteps(CoverageSnapshot);
		if (!CoverageSnapshot.bComplete) { RetryActiveCoverage(); co_return; }
	}
	if (!bUsesTerrainPlan)
	{
		co_await VoxelGameThread::AppendSteps(TargetTerrainNodes, PreviousTerrainNodes);
		for (const FVoxelViewKey& Node : PreviousTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const bool bWanted = Node.Level == 0 ? GetInputRoot().FineWanted.Contains(Node.Coordinate) : GetInputRoot().ProxyWanted.Contains(Node);
			if (!bWanted)
			{
				bool bReplacement = Node.Level == 0 && !VoxelCoverage::IntersectsAny2D(FineCoverageRect(Node.Coordinate), CoverageSnapshot.WantedHeightfield);
				if (!bReplacement)
				{
					co_await IsTerrainVolumeCoveredSteps(Node, CoverageSnapshot, Node.Level > 0, bReplacement);
				}
				if (!bReplacement)
				{
					co_await VoxelCoverage::IsFullyCovered2DSteps(TerrainCoverageRect(Node), CoverageSnapshot.Heightfield, bReplacement);
				}
				if (bReplacement)
				{
					TargetTerrainNodes.Remove(Node);
				}
			}
			if (CoverageAffects(TerrainCoverageRect(Node)))
				TerrainActorCandidates.Add(Node);
		}
		for (const FIntVector& Section : LocalFineKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (GetInputRoot().FineWanted.Contains(Section) && IsFineCoverageReady(Section, PresentedFine.Get()))
				TargetTerrainNodes.Add({Section, 0});
		}
		for (const auto& Node : LocalProxyKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (GetInputRoot().ProxyWanted.Contains(Node) && VoxelProxyReady.Contains(Node))
				TargetTerrainNodes.Add(Node);
		}
		for (const auto& Node : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (CoverageAffects(TerrainCoverageRect(Node)))
				TerrainActorCandidates.Add(Node);
		}
	}
#if !UE_BUILD_SHIPPING
	TArray<FVoxelViewKey, TInlineAllocator<2>> PointHandoffKeys;
	TSet<FVoxelViewKey> PointTreeTargets;
	if (FPlatformTime::Seconds() >= NextPointHandoffLog)
	{
		for (const auto& Pair : GetViewInterest().Sources)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto& Source = Pair.Value.Source;
			if (!Source.bLocalView || !Source.Has(EVoxelStreamingCapability::FineVisual) || Source.Has(EVoxelStreamingCapability::LocalRefinement))
				continue;
			const auto AddPoint = [&](const FIntVector& Cell)
			{
				PointHandoffKeys.AddUnique(
					{FIntVector(VoxelGeneration::FloorDivide(Cell.X, 16), VoxelGeneration::FloorDivide(Cell.Y, 16), VoxelGeneration::FloorDivide(Cell.Z, 16)),
						0});
			};
			AddPoint(Source.Center);
			FVoxelNaturalColumnEntryPtr Column;
			if (Module.GetGenerationCache() && Module.GetGenerationCache()->FindNaturalColumn(FIntPoint(Source.Center.X, Source.Center.Y), Column))
				AddPoint(FIntVector(Source.Center.X, Source.Center.Y, Column->Column.SurfaceZ));
			break;
		}
		for (const auto& Key : PointHandoffKeys)
			if (TargetTerrainNodes.Contains(Key))
				PointTreeTargets.Add(Key);
		NextPointHandoffLog = FPlatformTime::Seconds() + 5.0;
	}
#endif
	TSet<FVoxelHeightfieldNodeKey> LocalHeightfieldOwners;
	for (const auto& Key : LocalSurfaceKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level});
	for (const auto& Key : LocalMacroKeys)
		LocalHeightfieldOwners.Add({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level});
	co_await RebuildVolumeTransitionSteps(TargetTerrainNodes);
	if (bCoverageRetryRequested) co_return;
	TMap<FVoxelViewKey, uint64> VolumeToCommit;
	if (!bActiveCoverageFull)
	{
		auto TerrainRect = [this](const FVoxelViewKey& Key)
		{
			return Key.Level == 0 ? FineCoverageRect(Key.Coordinate) : VoxelProxyCoverageRect(Key);
		};
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!VisibleTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
			{
				ExpandActiveCoverage(TerrainRect(Key));
				TerrainActorCandidates.Add(Key);
			}
		}
		if (!bActiveCoverageFull)
		{
			for (const FVoxelViewKey& Key : PreviousTerrainNodes)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (!TargetTerrainNodes.Contains(Key) && !CoverageAffects(TerrainRect(Key)))
				{
					ExpandActiveCoverage(TerrainRect(Key));
					TerrainActorCandidates.Add(Key);
				}
			}
		}
	}
	const double PlanMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0;
	const bool bNeedsExclusions = !bUsesTerrainPlan || !SurfaceActors.IsEmpty() || !MacroActors.IsEmpty();
	TArray<FBox> FineBoxes;
	TArray<FBox> ProxyBoxes;
	TArray<FBox> ProxySurfaceBoxes;
	auto Box = [](const FVoxelCoverageBox& Bounds)
	{
		return FBox(FVector(Bounds.Min), FVector(Bounds.Max));
	};
	auto Column = [](const FVoxelCoverageRect& Bounds)
	{
		return FBox(FVector(Bounds.Min.X, Bounds.Min.Y, -1.e12), FVector(Bounds.Max.X, Bounds.Max.Y, 1.e12));
	};
	TArray<FBox> ScopeBoxes;
	for (const auto& Rect : ActiveDirtyCoverageRects)
		ScopeBoxes.Add(Column(Rect));
	const auto SetCoverage = [this, &ScopeBoxes](const FGuid& Group, AActor* Actor, TArray<FBox> Boxes,
								 TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> Common = nullptr) -> FVoxelGameThreadContinuation
	{
		bool bSuccess = false;
		if (bActiveCoverageFull)
		{
			co_await Publisher->SetCoverageSteps(Group, Actor, MoveTemp(Boxes), Common, bSuccess);
		}
		else
		{
			co_await Publisher->SetScopedCoverageSteps(Group, Actor, MoveTemp(Boxes), Common, ScopeBoxes, bSuccess);
		}
		if (!bSuccess)
		{
			MarkCoverageDirty();
		}
	};
	const auto SetSharedCoverage = [this, &ScopeBoxes](const FGuid& Group, AActor* Actor,
									   TSharedPtr<const TArray<FBox>, ESPMode::ThreadSafe> Common) -> FVoxelGameThreadContinuation
	{
		bool bSuccess = false;
		if (bActiveCoverageFull)
		{
			co_await Publisher->SetSharedCoverageSteps(Group, Actor, Common, bSuccess);
		}
		else
		{
			co_await Publisher->SetScopedCoverageSteps(Group, Actor, {}, Common, ScopeBoxes, bSuccess, true);
		}
		if (!bSuccess)
		{
			MarkCoverageDirty();
		}
	};
	if (bHeightfieldTopologyDirty)
	{
		co_await RebuildHeightfieldTransitionSteps(CoverageSnapshot);
		if (bCoverageRetryRequested) co_return;
		bHeightfieldTopologyDirty = false;
		++HeightfieldTopologyRebuildCount;
	}

	TMap<FVoxelHeightfieldNodeKey, uint64> HeightfieldToCommit;
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedTransitionSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const auto Mesh = PreparedTransitionMeshes.FindRef(Pair.Key);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!Desired || *Desired != Pair.Value || !Mesh || PendingHeightfieldPlanOwners.Contains(Pair.Key) || VolumeHandoffAffectsHeightfield(Pair.Key))
			continue;
		const int32 TileSide = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
								   ? VoxelViewLod::SurfaceSide(Module.GetViewSettings().Lod, Pair.Key.Level)
								   : VoxelViewLod::MacroSide(Module.GetViewSettings().Lod, Pair.Key.Level);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		if (!Publisher->StageImmutable(BeginPublishGroup({static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
										   Pair.Key.Coordinate, Pair.Key.Level}),
				Host, FVector(Pair.Key.Coordinate.X * TileSide, Pair.Key.Coordinate.Y * TileSide, 0) * Module.BlockSize(), Module.BlockSize(), Mesh,
				Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 2 : 3, true))
		{
			MarkCoverageDirty();
			continue;
		}
		HeightfieldTransitionActors.Add(Pair.Key, Host);
		co_await SetCoverage(BeginPublishGroup({static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5),
								 Pair.Key.Coordinate, Pair.Key.Level}),
			Host, {});
		HeightfieldToCommit.Add(Pair.Key, Pair.Value);
	}
	TSet<FVoxelHeightfieldNodeKey> BlockedHeightfieldNodes;
	for (const auto& Owner : LocalHeightfieldOwners)
		if (PendingHeightfieldPlanOwners.Contains(Owner))
			BlockedHeightfieldNodes.Add(Owner);
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, int32> Pair(LocalOwner, 0);
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (VolumeHandoffAffectsHeightfield(Pair.Key))
			BlockedHeightfieldNodes.Add(Pair.Key);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Edges = DesiredTransitionEdges.Find(LocalOwner);
		if (!Edges)
			continue;
		const TPair<FVoxelHeightfieldNodeKey, const TArray<FVoxelHeightfieldTransitionEdge>&> Pair(LocalOwner, *Edges);
		const uint64* Desired = DesiredTransitionSignatures.Find(Pair.Key);
		const uint64* Built = HeightfieldTransitionSignatures.Find(Pair.Key);
		AActor* Host = HeightfieldTransitionActors.FindRef(Pair.Key);
		const uint64* Staged = HeightfieldToCommit.Find(Pair.Key);
		const bool bReady = Desired && ((Built && *Built == *Desired && (!Host || Publisher->HasPresentation(Host))) || (Staged && *Staged == *Desired));
		if (bReady)
			continue;
		BlockedHeightfieldNodes.Add(Pair.Key);
		for (const FVoxelHeightfieldTransitionEdge& Edge : Pair.Value)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!Edge.bPending)
				BlockedHeightfieldNodes.Add(Edge.Neighbor);
		}
	}
	if (bActiveCoverageFull)
		PendingHeightfieldHandoffRects.Reset();
	else
		PendingHeightfieldHandoffRects.RemoveAll(
			[this](const FVoxelCoverageRect& Rect)
			{
				return CoverageAffects(Rect);
			});
	for (const FVoxelHeightfieldNodeKey& Key : BlockedHeightfieldNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Representation == EVoxelHeightfieldRepresentation::Surface)
		{
			const FVoxelSurfaceTileKey Tile{Key.Coordinate, Key.Level};
			if (GetInputRoot().SurfaceWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(SurfaceCoverageRect(Tile));
		}
		else
		{
			const FVoxelMacroTileKey Tile{Key.Coordinate, Key.Level};
			if (GetInputRoot().MacroWanted.Contains(Tile))
				PendingHeightfieldHandoffRects.Add(MacroCoverageRect(Tile));
		}
	}
	for (const auto& LocalOwner : LocalProxyKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Prepared = PreparedVolumeSignatures.Find(LocalOwner);
		if (!Prepared)
			continue;
		const TPair<FVoxelViewKey, uint64> Pair(LocalOwner, *Prepared);
		const uint64* Desired = DesiredVolumeSignatures.Find(Pair.Key);
		const auto Mesh = PreparedVolumeMeshes.FindRef(Pair.Key);
		if (!Desired || *Desired != Pair.Value || !Mesh || !TargetTerrainNodes.Contains(Pair.Key) ||
			VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Pair.Key), PendingVolumeHandoffBoxes))
			continue;
		// 空接缝结果也必须提交签名，释放准备结果并停止重复覆盖计算。
		const FGuid Group = BeginPublishGroup({1, Pair.Key.Coordinate, Pair.Key.Level});
		const int32 TerrainStage = VolumeTransitionWorkClass(Pair.Key) <= EVoxelWorkClass::Interactive ? 0 : 1;
		Publisher->SetTerrainStage(Group, TerrainStage);
		AActor* Host = VoxelProxyActors.FindRef(Pair.Key);
		if (HasRenderableMesh(*Mesh) || Host)
		{
			if (!Publisher->StageImmutable(Group, Host, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
					Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, Mesh, TerrainStage))
			{
				PendingVolumeHandoffBoxes.Add(VoxelProxyCoverageBox(Pair.Key));
				MarkCoverageDirty(VoxelProxyCoverageRect(Pair.Key));
				continue;
			}
			VoxelProxyActors.Add(Pair.Key, Host);
		}
		if (const auto BoundaryMesh = PreparedVolumeBoundaryMeshes.FindRef(Pair.Key))
		{
			AActor* BoundaryHost = VolumeBoundaryActors.FindRef(Pair.Key);
			if (HasRenderableMesh(*BoundaryMesh) || BoundaryHost)
			{
				if (!Publisher->StageImmutable(Group, BoundaryHost, FVector(Pair.Key.GetBounds().Min) * Module.BlockSize(),
						Module.BlockSize() * VoxelProxyData.FindChecked(Pair.Key)->Geometry.SampleStepCells, BoundaryMesh, TerrainStage, true))
					continue;
				VolumeBoundaryActors.Add(Pair.Key, BoundaryHost);
				co_await SetCoverage(Group, BoundaryHost, {});
				Publisher->SetHidden(Group, BoundaryHost, false);
			}
		}
		VolumeToCommit.Add(Pair.Key, Pair.Value);
	}
	for (auto It = TargetTerrainNodes.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = It->Level == 0 ? FineCoverageBox(It->Coordinate) : VoxelProxyCoverageBox(*It);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			It.RemoveCurrent();
	}
	for (const auto& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelCoverageBox Bounds = Key.Level == 0 ? FineCoverageBox(Key.Coordinate) : VoxelProxyCoverageBox(Key);
		if (VoxelCoverage::IntersectsAny3D(Bounds, PendingVolumeHandoffBoxes))
			TargetTerrainNodes.Add(Key);
	}
#if !UE_BUILD_SHIPPING
	for (const auto& Key : PointHandoffKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		UE_LOG(LogTemp, Display,
			TEXT(
				"Voxel point handoff: section=%s ready=%d desired=%d tree=%d volumeBlocked=%d target=%d owned=%d presented=%d"),
			*Key.Coordinate.ToString(), FineReady.Contains(Key.Coordinate), Plan.Leaves.Contains(Key), PointTreeTargets.Contains(Key),
			VoxelCoverage::IntersectsAny3D(FineCoverageBox(Key.Coordinate), PendingVolumeHandoffBoxes), TargetTerrainNodes.Contains(Key),
			VisibleTerrainNodes.Contains(Key), IsFineCoverageReady(Key.Coordinate, PresentedFine.Get()));
	}
#endif
	auto HandoffPending = [this](const FVoxelCoverageRect& Rect)
	{
		return VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects);
	};
	TArray<FVoxelCoverageRect> RecheckCoverageRects;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(VoxelProxyCoverageRect(Pair.Key));
	}
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		RecheckCoverageRects.Add(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
									 ? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
									 : MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level}));
	}
	// 裁剪与实际保留的体素分区一致，不能先裁掉仍在等待体素接缝的上一代覆盖。
	if (bUsesTerrainPlan && bNeedsExclusions)
	{
		for (const FVoxelViewKey& Key : TargetTerrainNodes)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (Key.Level == 0)
			{
				if (IsFineCoverageReady(Key.Coordinate, PresentedFine.Get()))
					FineBoxes.Add(Box(FineCoverageBox(Key.Coordinate)));
			}
			else
			{
				if (!VoxelProxyActors.FindRef(Key))
					continue;
				ProxyBoxes.Add(Box(VoxelProxyCoverageBox(Key)));
				if (const auto Data = ProxySurfaceCoverage.FindRef(Key))
				{
					const FVoxelCoverageBox Bounds = VoxelProxyCoverageBox(Key);
					for (const FVoxelCoverageRect& Rect : Data->TopSurfaceCoverage)
						ProxySurfaceBoxes.Add(FBox(FVector(Rect.Min.X, Rect.Min.Y, Bounds.Min.Z), FVector(Rect.Max.X, Rect.Max.Y, Bounds.Max.Z)));
				}
			}
		}
	}
	else if (!bUsesTerrainPlan)
	{
		for (const FIntVector& Key : LocalFineKeys)
			if (GetInputRoot().FineWanted.Contains(Key) && IsFineCoverageReady(Key, PresentedFine.Get()))
				FineBoxes.Add(Box(FineCoverageBox(Key)));
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(FineBoxes);
	co_await VoxelMeshClipper::NormalizeBoxesSteps(ProxyBoxes);
	co_await VoxelMeshClipper::NormalizeBoxesSteps(ProxySurfaceBoxes);
	const auto CommonHeightfieldBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>(FineBoxes);
	CommonHeightfieldBoxes->Append(ProxySurfaceBoxes);
	const auto CommonTransitionBoxes = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	for (FBox Bounds : *CommonHeightfieldBoxes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Bounds.Min.Z = -1.e12;
		Bounds.Max.Z = 1.e12;
		CommonTransitionBoxes->Add(Bounds);
	}
	co_await VoxelMeshClipper::NormalizeBoxesSteps(*CommonTransitionBoxes);
	const double BoxesMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds;
	TArray<FVoxelCoverageRect> PresentedHeightfieldRects;
	for (const auto& Key : LocalSurfaceKeys)
		if (AActor* Actor = SurfaceActors.FindRef(Key); Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
			PresentedHeightfieldRects.Add(SurfaceCoverageRect(Key));
	for (const auto& Key : LocalMacroKeys)
		if (AActor* Actor = MacroActors.FindRef(Key); Actor && !Actor->IsHidden() && Publisher->IsPresented(Actor))
			PresentedHeightfieldRects.Add(MacroCoverageRect(Key));

	TerrainCandidateCount = TerrainActorCandidates.Num();
	for (const FVoxelViewKey& Key : TerrainActorCandidates)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0)
			continue;
		AActor* Actor = VoxelProxyActors.FindRef(Key);
		if (!Actor || !CoverageAffects(VoxelProxyCoverageRect(Key)))
			continue;
		const bool bVisible = TargetTerrainNodes.Contains(Key);
		const FVoxelPublishGroupKey GroupKey{1, Key.Coordinate, Key.Level};
		if (!bVisible && Actor->IsHidden() && Publisher->IsCommitted(Actor) && !Publisher->IsGroupBusy(GroupKey))
			continue;
		const FGuid Group = BeginPublishGroup(GroupKey);
		if (VoxelCoverage::IntersectsAny3D(VoxelProxyCoverageBox(Key), PendingVolumeHandoffBoxes))
		{
			if (!Publisher->HasPresentation(Actor))
				Publisher->SetHidden(Group, Actor, true);
			continue;
		}
		if (bVisible)
		{
			TArray<FBox> Exclusions = FineBoxes;
			for (const auto& Other : TargetTerrainNodes)
				if (Other.Level > 0 && Other.Level < Key.Level)
					Exclusions.Add(Box(VoxelProxyCoverageBox(Other)));
			co_await SetCoverage(Group, Actor, MoveTemp(Exclusions));
		}
		Publisher->SetHidden(Group, Actor, !bVisible);
		if (AActor* Boundary = VolumeBoundaryActors.FindRef(Key))
			Publisher->SetHidden(Group, Boundary, !bVisible);
	}
	for (const FVoxelSurfaceTileKey& LocalKey : LocalSurfaceKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelSurfaceTileKey, TObjectPtr<AActor>> Pair(LocalKey, SurfaceActors.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(SurfaceCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({2, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(SurfaceCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!GetInputRoot().SurfaceWanted.Contains(Pair.Key) && HandoffPending(SurfaceCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				co_await SetSharedCoverage(Group, Water, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = GetInputRoot().SurfaceWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.Macro, bHidden);
		if (!bWanted && !bHidden)
			co_await VoxelCoverage::IsFullyCovered2DSteps(SurfaceCoverageRect(Pair.Key), CoverageSnapshot.ProxySurface, bHidden);
		const FVoxelCoverageRect ActorRect = SurfaceCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				Publisher->SetHidden(Group, Water, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : LocalSurfaceKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (SurfaceReady.Contains(Key) && GetInputRoot().SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		if (!bWanted)
		{
			for (const FVoxelMacroTileKey& Key : LocalMacroKeys)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				if (MacroReady.Contains(Key) && GetInputRoot().MacroWanted.Contains(Key) &&
					!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}))
					if (ActorRect.Intersects(MacroCoverageRect(Key)))
						Exclusions.Add(Column(MacroCoverageRect(Key)));
			}
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(ActorRect);
			co_await SetCoverage(Group, Pair.Value, Exclusions, CommonHeightfieldBoxes);
			if (AActor* Water = WaterActors.FindRef(Pair.Key))
				co_await SetCoverage(Group, Water, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
		if (AActor* Water = WaterActors.FindRef(Pair.Key))
			Publisher->SetHidden(Group, Water, bHidden);
	}
	for (const FVoxelMacroTileKey& LocalKey : LocalMacroKeys)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelMacroTileKey, TObjectPtr<AActor>> Pair(LocalKey, MacroActors.FindRef(LocalKey));
		if (!Pair.Value)
			continue;
		if (!CoverageAffects(MacroCoverageRect(Pair.Key)))
			continue;
		const FGuid Group = BeginPublishGroup({3, Pair.Key.Coordinate, Pair.Key.Level});
		const bool bHasPreviousCoverage =
			Publisher->HasPresentation(Pair.Value) || VoxelCoverage::IntersectsAny2D(MacroCoverageRect(Pair.Key), PresentedHeightfieldRects);
		if (bHasPreviousCoverage && (BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Pair.Key.Coordinate, Pair.Key.Level}) ||
										(!GetInputRoot().MacroWanted.Contains(Pair.Key) && HandoffPending(MacroCoverageRect(Pair.Key)))))
		{
			if (!Publisher->HasPresentation(Pair.Value))
				Publisher->SetHidden(Group, Pair.Value, true);
			else
				co_await SetSharedCoverage(Group, Pair.Value, CommonHeightfieldBoxes);
			continue;
		}
		const bool bWanted = GetInputRoot().MacroWanted.Contains(Pair.Key);
		bool bHidden = false;
		if (!bWanted)
			co_await VoxelCoverage::IsFullyCovered2DSteps(MacroCoverageRect(Pair.Key), CoverageSnapshot.Surface, bHidden);
		const FVoxelCoverageRect ActorRect = MacroCoverageRect(Pair.Key);
		if (!CoverageAffects(ActorRect))
		{
			Publisher->SetHidden(Group, Pair.Value, bHidden);
			continue;
		}
		TArray<FBox> Exclusions;
		for (const FVoxelSurfaceTileKey& Key : LocalSurfaceKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (SurfaceReady.Contains(Key) && GetInputRoot().SurfaceWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Surface, Key.Coordinate, Key.Level}))
				if (ActorRect.Intersects(SurfaceCoverageRect(Key)))
					Exclusions.Add(Column(SurfaceCoverageRect(Key)));
		}
		for (const FVoxelMacroTileKey& Key : LocalMacroKeys)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (MacroReady.Contains(Key) && GetInputRoot().MacroWanted.Contains(Key) &&
				!BlockedHeightfieldNodes.Contains({EVoxelHeightfieldRepresentation::Macro, Key.Coordinate, Key.Level}) && Key != Pair.Key &&
				(!bWanted || Key.Level < Pair.Key.Level))
				if (ActorRect.Intersects(MacroCoverageRect(Key)))
					Exclusions.Add(Column(MacroCoverageRect(Key)));
		}
		if (!bHidden)
		{
			if (!Publisher->IsCommitted(Pair.Value))
				RecheckCoverageRects.Add(ActorRect);
			co_await SetCoverage(Group, Pair.Value, MoveTemp(Exclusions), CommonHeightfieldBoxes);
		}
		Publisher->SetHidden(Group, Pair.Value, bHidden);
	}
	for (const auto& LocalOwner : LocalHeightfieldOwners)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const TPair<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>> Pair(LocalOwner, HeightfieldTransitionActors.FindRef(LocalOwner));
		if (!Pair.Value)
			continue;
		const FVoxelCoverageRect Rect = Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface
											? SurfaceCoverageRect({Pair.Key.Coordinate, Pair.Key.Level})
											: MacroCoverageRect({Pair.Key.Coordinate, Pair.Key.Level});
		if (!CoverageAffects(Rect))
			continue;
		const FGuid Group = BeginPublishGroup(
			{static_cast<uint8>(Pair.Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5), Pair.Key.Coordinate, Pair.Key.Level});
		if (Publisher->HasPresentation(Pair.Value) && (BlockedHeightfieldNodes.Contains(Pair.Key) || HandoffPending(Rect)))
		{
			co_await SetSharedCoverage(Group, Pair.Value, CommonTransitionBoxes);
			HeightfieldToCommit.Remove(Pair.Key);
			continue;
		}
		co_await SetCoverage(Group, Pair.Value, {}, CommonTransitionBoxes);
		Publisher->SetHidden(Group, Pair.Value, !DesiredTransitionSignatures.Contains(Pair.Key) || BlockedHeightfieldNodes.Contains(Pair.Key));
	}
	const double ActorsMilliseconds = (FPlatformTime::Seconds() - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> PreviousNodesByGroup;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> TargetNodesByGroup;
	const auto TerrainGroupKey = [](const FVoxelViewKey& Key)
	{
		return FVoxelPublishGroupKey{1, Key.Coordinate, Key.Level};
	};
	for (const FVoxelViewKey& Key : PreviousTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0 || !CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!TargetTerrainNodes.Contains(Key))
			BeginPublishGroup(TerrainGroupKey(Key));
		if (ActiveCoveragePublishGroups.Contains(TerrainGroupKey(Key)))
			PreviousNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	for (const FVoxelViewKey& Key : TargetTerrainNodes)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Key.Level == 0 || !CoverageAffects(TerrainCoverageRect(Key)))
			continue;
		if (!VisibleTerrainNodes.Contains(Key))
			BeginPublishGroup(TerrainGroupKey(Key));
		if (ActiveCoveragePublishGroups.Contains(TerrainGroupKey(Key)))
			TargetNodesByGroup.FindOrAdd(TerrainGroupKey(Key)).Add(Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VolumeByGroup;
	for (const auto& Pair : VolumeToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		VolumeByGroup.FindOrAdd(TerrainGroupKey(Pair.Key)).Add(Pair.Key);
	}
	TMap<FVoxelPublishGroupKey, TArray<FVoxelHeightfieldNodeKey>> HeightfieldByGroup;
	for (const auto& Pair : HeightfieldToCommit)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const FVoxelHeightfieldNodeKey& Key = Pair.Key;
		const uint8 Representation = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? 4 : 5;
		HeightfieldByGroup.FindOrAdd({Representation, Key.Coordinate, Key.Level}).Add(Key);
	}
	TSet<FVoxelPublishGroupKey> OwnershipChanges;
	for (const auto& Pair : ActiveCoveragePublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto* Previous = PreviousNodesByGroup.Find(Pair.Key);
		const auto* Target = TargetNodesByGroup.Find(Pair.Key);
		bool bChanged = (Previous ? Previous->Num() : 0) != (Target ? Target->Num() : 0);
		if (!bChanged && Previous && Target)
			for (const auto& Node : *Previous)
				if (!Target->Contains(Node))
				{
					bChanged = true;
					break;
				}
		if (bChanged || VolumeByGroup.Contains(Pair.Key) || HeightfieldByGroup.Contains(Pair.Key))
			OwnershipChanges.Add(Pair.Key);
	}
	TFunction<void()> OnPublishCommitted = [this, LocalOwners = MoveTemp(LocalHeightfieldOwners), Recheck = MoveTemp(RecheckCoverageRects)]()
	{
		for (const auto& Key : LocalOwners)
		{
			if (DesiredTransitionSignatures.Contains(Key))
				continue;
			const FVoxelCoverageRect Rect = Key.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Key.Coordinate, Key.Level})
																										   : MacroCoverageRect({Key.Coordinate, Key.Level});
			if (VoxelCoverage::IntersectsAny2D(Rect, PendingHeightfieldHandoffRects))
				continue;
			if (AActor* Actor = HeightfieldTransitionActors.FindRef(Key))
			{
				Publisher->Forget(Actor);
				Actor->Destroy();
			}
			HeightfieldTransitionSignatures.Remove(Key);
			HeightfieldTransitionActors.Remove(Key);
		}
		for (const auto& Rect : Recheck)
			MarkCoverageDirty(Rect);
	};
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics> GeneratedForCommit;
	for (const auto& Group : ActiveCoveragePublishGroups)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Group.Key.Representation == 0 || Group.Key.Representation > 3)
			continue;
		const auto Representation = static_cast<EVoxelTerrainRepresentation>(Group.Key.Representation + 1);
		const FVoxelTerrainSpatialKey Key{Representation, Group.Key.Coordinate, Group.Key.Level};
		if (const auto* Metrics = GeometryMetrics.Find(Key))
			GeneratedForCommit.Add(Key, *Metrics);
	}
	TFunction<void(TConstArrayView<FVoxelPublishGroupKey>)> OnGroupCommitted =
		[this, Generated = MoveTemp(GeneratedForCommit), Target = MoveTemp(TargetNodesByGroup),
			Volume = MoveTemp(VolumeByGroup), Heightfield = MoveTemp(HeightfieldByGroup),
			VolumeSignatures = MoveTemp(VolumeToCommit), HeightfieldSignatures = MoveTemp(HeightfieldToCommit)](TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		bTerrainBuildStageDirty = true;
		for (const FVoxelPublishGroupKey& Key : Keys)
		{
			const auto* Desired = Target.Find(Key);
			if (Key.Representation == 1 && CommitTerrainOwnership(Key, Desired ? MakeArrayView(*Desired) : TConstArrayView<FVoxelViewKey>()))
			{
				MarkCoverageDirty(VoxelProxyCoverageRect({Key.Coordinate, Key.Level}));
				bHeightfieldTopologyDirty = true;
			}
			if (Key.Representation >= 1 && Key.Representation <= 3)
			{
				const FVoxelTerrainSpatialKey Spatial{static_cast<EVoxelTerrainRepresentation>(Key.Representation + 1), Key.Coordinate, Key.Level};
				AActor* Actor = Key.Representation == 1 ? VoxelProxyActors.FindRef({Key.Coordinate, Key.Level})
								: Key.Representation == 2 ? SurfaceActors.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level})
														  : MacroActors.FindRef({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level});
				if (const auto* Metrics = Generated.Find(Spatial))
				{
					auto Presented = *Metrics;
					Presented.bPresented = Publisher->GetPresentedMeshCost(Actor, Presented.Vertices, Presented.Triangles, Presented.MeshBytes);
					if (Presented.bPresented)
						PresentedGeometryMetrics.Add(Spatial, Presented);
					else
						PresentedGeometryMetrics.Remove(Spatial);
				}
				else if (!Actor || Actor->IsHidden())
					PresentedGeometryMetrics.Remove(Spatial);
			}
			if (const TArray<FVoxelViewKey>* Nodes = Volume.Find(Key))
			{
				for (const FVoxelViewKey& Node : *Nodes)
				{
					if (const uint64* Signature = VolumeSignatures.Find(Node))
					{
						VolumeTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedVolumeSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedVolumeSignatures.Remove(Node);
							PreparedVolumeMeshes.Remove(Node);
							PreparedVolumeBoundaryMeshes.Remove(Node);
						}
						MarkCoverageDirty(VoxelProxyCoverageRect(Node));
					}
				}
			}
			if (const TArray<FVoxelHeightfieldNodeKey>* Nodes = Heightfield.Find(Key))
			{
				for (const FVoxelHeightfieldNodeKey& Node : *Nodes)
				{
					if (const uint64* Signature = HeightfieldSignatures.Find(Node))
					{
						HeightfieldTransitionSignatures.Add(Node, *Signature);
						const uint64* Prepared = PreparedTransitionSignatures.Find(Node);
						if (Prepared && *Prepared == *Signature)
						{
							PreparedTransitionSignatures.Remove(Node);
							PreparedTransitionMeshes.Remove(Node);
						}
						MarkCoverageDirty(Node.Representation == EVoxelHeightfieldRepresentation::Surface ? SurfaceCoverageRect({Node.Coordinate, Node.Level})
																										  : MacroCoverageRect({Node.Coordinate, Node.Level}));
					}
				}
			}
		}
	};
	for (auto It = ActiveCoveragePublishGroups.CreateIterator(); It; ++It)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		Publisher->SetOwnershipChanged(It.Value(), OwnershipChanges.Contains(It.Key()));
		if (Publisher->DiscardUnchangedGroup(It.Value()))
			It.RemoveCurrent();
	}
	if (ActiveCoveragePublishGroups.IsEmpty())
	{
		OnPublishCommitted();
	}
	else
	{
		TArray<FGuid> Handles;
		Handles.Reserve(ActiveCoveragePublishGroups.Num());
		for (const auto& Pair : ActiveCoveragePublishGroups) Handles.Add(Pair.Value);
		Publisher->SetCommitCallbacks(MoveTemp(OnPublishCommitted), MoveTemp(OnGroupCommitted), Handles);
		const FVoxelCoverageIndex FrozenCoverage = CoverageIndex;
		bool bAccepted = true;
		for (const auto& Pair : ActiveCoveragePublishGroups)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const auto Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Pair.Key, Module.BlockSize(), Module.GetViewSettings().Lod);
			FVoxelGenerationBounds GuardBounds;
			if (Bounds.IsValid)
			{
				const bool bVolume = Pair.Key.Representation <= 1;
				const int32 MinZ = bVolume ? FMath::FloorToInt(Bounds.Min.Z / Module.BlockSize()) - 1 : MIN_int32 + 1;
				const int32 MaxZ = bVolume ? FMath::CeilToInt(Bounds.Max.Z / Module.BlockSize()) + 1 : MAX_int32;
				GuardBounds = {
					FIntVector(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()) - 1, FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize()) - 1, MinZ),
					FIntVector(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()) + 1, FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()) + 1, MaxZ)};
			}
			const uint64 Epoch = WorldEpoch;
			const FVoxelGenerationBounds DependencyBounds = Bounds.IsValid ? FVoxelGenerationBounds{
				FIntVector(FMath::FloorToInt(Bounds.Min.X / Module.BlockSize()), FMath::FloorToInt(Bounds.Min.Y / Module.BlockSize()),
					Pair.Key.Representation <= 1 ? FMath::FloorToInt(Bounds.Min.Z / Module.BlockSize()) : Module.GetManifest().Settings.MinZ),
				FIntVector(FMath::CeilToInt(Bounds.Max.X / Module.BlockSize()), FMath::CeilToInt(Bounds.Max.Y / Module.BlockSize()),
					Pair.Key.Representation <= 1 ? FMath::CeilToInt(Bounds.Max.Z / Module.BlockSize()) : Module.GetManifest().Settings.MaxZ)} : FVoxelGenerationBounds{};
			Publisher->SetValidation(Pair.Value,
				[this, Epoch, ContentSnapshot, DependencyBounds, GuardBounds, FrozenCoverage]
				{
					return Epoch == WorldEpoch && (!GuardBounds.IsValid() ||
						CoverageIndex.IsRangeCurrent(FrozenCoverage, GuardBounds.Min, GuardBounds.Max)) && (!DependencyBounds.IsValid() ||
						Module.GetRuntime()->GetChangeHierarchy().IsVersionSnapshotCurrent(ContentSnapshot, DependencyBounds));
				});
			bAccepted &= Publisher->CommitGroup(Pair.Value);
		}
		ActiveCoveragePublishGroups.Reset();
		if (!bAccepted)
			MarkCoverageDirty();
	}
	const double Now = FPlatformTime::Seconds();
	if (CVarVoxelDebugRepresentation.GetValueOnGameThread() > 0 && Now >= NextTimingLog)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel coverage phase elapsed ms: plan=%.3f boxes=%.3f actors=%.3f commit=%.3f"), PlanMilliseconds, BoxesMilliseconds,
			ActorsMilliseconds, (Now - ResolveStart) * 1000.0 - PlanMilliseconds - BoxesMilliseconds - ActorsMilliseconds);
		NextTimingLog = Now + 5.0;
	}
}

bool FVoxelViewManager::EnqueueViewTask(FVoxelTaskRequest&& InRequest)
{
	if (HasDeferredRepresentation(InRequest.Stamp)) return false;
	// 本地粗层原先走 Module 默认分发；交接必须与显式 Apply 使用同一生命周期。
	if (!InRequest.Apply && (InRequest.Kind == EVoxelTaskKind::BuildVoxelProxy ||
		InRequest.Kind == EVoxelTaskKind::BuildSurface || InRequest.Kind == EVoxelTaskKind::BuildMacro))
	{
		const auto Consumer = InRequest.GenerationConsumer.IsValid() ? InRequest.GenerationConsumer : Scheduler.GetControlConsumer();
		FString BindingError;
		if (!InRequest.BindApply(Scheduler.GetTerrainResources(), Consumer,
			[this](FVoxelTaskResult&& Result) { OnTask(MoveTemp(Result)); }, BindingError))
		{
			Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
				BindingError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelTaskBlockReason::InputMemory : EVoxelTaskBlockReason::Oversized);
			return false;
		}
	}
	FVoxelTerrainServicePriority Priority;
	if (ResolveTaskServicePriority(InRequest.Kind, InRequest.Stamp, InRequest.WorkClass, Priority))
	{
		InRequest.ServiceBand = Priority.Band;
		InRequest.DistanceScore = Priority.DistanceCells;
		InRequest.ForwardScore = Priority.ForwardScore;
		InRequest.SourcePriority = Priority.SourcePriority;
		InRequest.SourceStableKey = Priority.SourceStableKey;
	}
	if (InRequest.Apply)
	{
		const auto Consumer = InRequest.GenerationConsumer.IsValid() ? InRequest.GenerationConsumer : Scheduler.GetControlConsumer();
		const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FGrantedCoverageApply>() +
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTaskResult>() +
			VoxelBoundedAllocation::FunctionBytes<FDeferredGrantedCoverageApply, false>();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		const auto Admission = Scheduler.GetTerrainResources()->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			MetadataBytes, MetadataLease, 0, InRequest.GetBindingBytes() + InRequest.GetRequiredBindingInputBytes(),
			InRequest.GetBindingLeaseCount() + InRequest.GetRequiredBindingInputLeaseCount());
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
		const uint64 PreviousRequiredInputBytes = InRequest.GetRequiredBindingInputBytes();
		const int32 PreviousRequiredLeaseCount = InRequest.GetRequiredBindingInputLeaseCount();
		InRequest.SetRequiredBindingInput(PreviousRequiredInputBytes + MetadataBytes, PreviousRequiredLeaseCount + 1);
		FString BindingError;
		if (!InRequest.WrapApply(Scheduler.GetTerrainResources(), Consumer,
			[this, MetadataLease, Kind = InRequest.Kind, Stage = InRequest.TerrainStage](TFunction<void(FVoxelTaskResult&&)>&& Previous,
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreviousLease)
		{
			const auto Apply = VoxelBoundedAllocation::MakeWorkerShared<FGrantedCoverageApply>(MetadataLease);
			Apply->MetadataLease = MetadataLease;
			Apply->PreviousLease = MoveTemp(PreviousLease);
			Apply->Apply = MoveTemp(Previous);
			Apply->Context = this;
			Apply->BeforeApply = [](void* Context, const FVoxelTaskResult& Result)
			{
				if (!Result.Stamp.bHasGeometryIdentity) return;
				auto& Manager = *static_cast<FVoxelViewManager*>(Context);
				auto* Attempt = Manager.RepresentationAttempts.Find(Result.Stamp.GeometryIdentity.Key);
				if (Attempt && Attempt->Stamp == Result.Stamp && Attempt->DeferredKind == Result.Kind)
					Attempt->DeferredKind = EVoxelTaskKind::None;
			};
			return [this, Apply, Kind, Stage](FVoxelTaskResult&& InResult)
		{
			const bool bLegacyHeightfield = !InResult.Stamp.bHasGeometryIdentity &&
				(Kind == EVoxelTaskKind::BuildViewTransition || (Kind == EVoxelTaskKind::BuildViewCoverage && Stage >= 2));
			const FIntPoint Coordinate(InResult.Stamp.Section.X, InResult.Stamp.Section.Y);
			const auto Rect = Stage == 2 ? SurfaceCoverageRect({Coordinate, InResult.Stamp.ViewKey.Level}) :
				MacroCoverageRect({Coordinate, InResult.Stamp.ViewKey.Level});
			const bool bLegacyVolume = !InResult.Stamp.bHasGeometryIdentity &&
				Kind == EVoxelTaskKind::BuildViewCoverage && Stage == 1;
			const bool bDeferred = bLegacyHeightfield ? ShouldDeferCoverage(Rect) :
				bLegacyVolume ? ShouldDeferCoverage(VoxelProxyCoverageRect(InResult.Stamp.ViewKey)) :
				ShouldDeferCoverage(InResult.Stamp);
			if (bDeferred)
			{
				const auto Result = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTaskResult>(Apply->MetadataLease, MoveTemp(InResult));
				if (Result->bSuccess && !Result->bCanceled && IsRepresentationStampCurrent(Result->Stamp) &&
					(Kind == EVoxelTaskKind::BuildVoxelProxy || Kind == EVoxelTaskKind::BuildSurface || Kind == EVoxelTaskKind::BuildMacro))
				{
					auto& Attempt = RepresentationAttempts.FindChecked(Result->Stamp.GeometryIdentity.Key);
					check(Attempt.DeferredKind == EVoxelTaskKind::None);
					Attempt.DeferredKind = Kind;
				}
				const auto Bounds = bLegacyHeightfield ? Rect : bLegacyVolume ? VoxelProxyCoverageRect(Result->Stamp.ViewKey) : CoverageBounds(Result->Stamp);
				DeferredCoverageCallbacks.Enqueue({Bounds, FDeferredGrantedCoverageApply{Apply, Result}, Apply->MetadataLease});
				return;
			}
			Apply->Consume(MoveTemp(InResult));
		};
		}, BindingError))
		{
			InRequest.SetRequiredBindingInput(PreviousRequiredInputBytes, PreviousRequiredLeaseCount);
			Scheduler.ObserveDemand(InRequest.Kind, InRequest.Stamp,
				BindingError.StartsWith(TEXT("Oversized")) ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			return false;
		}
	}
	return Scheduler.Enqueue(MoveTemp(InRequest));
}

void FVoxelViewManager::RecordTerrainMesh(const FVoxelTerrainSpatialKey& InKey, const FVoxelSectionMeshResult& InMesh, const double InBuildMilliseconds)
{
	auto& Metrics = GeometryMetrics.FindOrAdd(InKey);
	if (const auto* Geometry = GetViewInterest().Geometry.Find(InKey))
		Metrics.Geometry = *Geometry;
	else
		Metrics.Geometry = {InKey, 1, Module.GetManifest().Settings.MinZ, Module.GetManifest().Settings.MaxZ, 0, 0};
	Metrics.Vertices = Metrics.Triangles = 0;
	for (const auto& Batch : InMesh.Batches)
	{
		Metrics.Vertices += Batch.Mesh.Vertices.Num();
		Metrics.Triangles += Batch.Mesh.Triangles.Num() / 3;
	}
	Metrics.MeshBytes = InMesh.Bytes();
	Metrics.BuildMilliseconds = InBuildMilliseconds;
	Metrics.bGenerated = true;
	++TerrainWorkMetrics.Completed;
	TerrainWorkMetrics.BuildMilliseconds += InBuildMilliseconds;
}

FVoxelTerrainWorkMetrics FVoxelViewManager::GetTerrainWorkMetrics() const
{
	return TerrainWorkMetrics;
}

void FVoxelViewManager::LogTerrainGeometryMetrics() const
{
	struct FBucket
	{
		int32 Pages = 0;
		uint64 Vertices = 0;
		uint64 Triangles = 0;
		uint64 Bytes = 0;
		double AreaCells = 0.0;
		double BuildMilliseconds = 0.0;
		TSet<FIntPoint> Footprints;
	};
	const auto Log = [this](const TCHAR* Phase, const TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometryMetrics>& Values)
	{
		TMap<uint64, FBucket> Buckets;
		for (const auto& Pair : Values)
		{
			const auto& Metrics = Pair.Value;
			const auto& Geometry = Metrics.Geometry;
			auto& Bucket =
				Buckets.FindOrAdd((uint64(Geometry.Key.Representation) << 56) | (uint64(Geometry.Key.Level) << 48) | uint32(Geometry.SampleStepCells));
			++Bucket.Pages;
			Bucket.Vertices += Metrics.Vertices;
			Bucket.Triangles += Metrics.Triangles;
			Bucket.Bytes += Metrics.MeshBytes;
			bool bAlreadyCounted = false;
			Bucket.Footprints.Add(FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), &bAlreadyCounted);
			if (!bAlreadyCounted)
			{
				Bucket.AreaCells += double(Geometry.Key.SideCells()) * Geometry.Key.SideCells();
			}
			Bucket.BuildMilliseconds += Metrics.BuildMilliseconds;
		}
		for (const auto& Pair : Buckets)
			UE_LOG(LogTemp, Display,
				TEXT("Voxel geometry actual: phase=%s epoch=%llu rep=%d level=%u step=%u pages=%d ownershipAreaCells=%.0f vertices=%llu triangles=%llu "
					 "bytes=%llu buildMs=%.3f"),
				Phase, WorldEpoch, int32(Pair.Key >> 56), uint8(Pair.Key >> 48), uint32(Pair.Key), Pair.Value.Pages, Pair.Value.AreaCells, Pair.Value.Vertices,
				Pair.Value.Triangles, Pair.Value.Bytes, Pair.Value.BuildMilliseconds);
	};
	Log(TEXT("generated"), GeometryMetrics);
	Log(TEXT("presented"), PresentedGeometryMetrics);
	const auto Work = GetTerrainWorkMetrics();
	UE_LOG(LogTemp, Display,
		TEXT("Voxel terrain work: epoch=%llu completed=%llu failed=%llu canceled=%llu stale=%llu retry=%llu buildMs=%.3f coverageSteps=%llu pendingCoverage=%d "
			 "ageMs=%.3f"),
		WorldEpoch, Work.Completed, Work.Failed, Work.Canceled, Work.Stale, Work.Retried, Work.BuildMilliseconds, CoverageMicrosteps,
		CoverageContinuation.IsValid(), CoverageContinuation.IsValid() ? (FPlatformTime::Seconds() - CoverageStartedSeconds) * 1000.0 : 0.0);
}

FVoxelGameThreadContinuation FVoxelViewManager::IsTerrainVolumeCoveredSteps(
	const FVoxelViewKey InNode, const FCoveragePassSnapshot& InSnapshot, const bool bByFine, bool& OutCovered) const
{
	OutCovered = false;
	const auto& Nodes = bByFine ? InSnapshot.FineNodes : InSnapshot.ProxyNodes;
	const auto& Branches = bByFine ? InSnapshot.FineBranches : InSnapshot.ProxyBranches;
	for (FVoxelViewKey Ancestor = InNode; Ancestor.Level <= 20; Ancestor = Ancestor.GetParent())
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (Nodes.Contains(Ancestor))
		{
			OutCovered = true;
			co_return;
		}
	}
	if (InNode.Level == 0 || !Branches.Contains(InNode))
		co_return;
	TArray<FVoxelViewKey> Children;
	InNode.GetChildren(Children);
	for (const auto& Child : Children)
	{
		bool bChildCovered = false;
		co_await IsTerrainVolumeCoveredSteps(Child, InSnapshot, bByFine, bChildCovered);
		if (!bChildCovered)
			co_return;
	}
	OutCovered = true;
}
