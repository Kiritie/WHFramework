#include "Voxel/Rendering/VoxelDetailView.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Chaos/SimCallbackObject.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationCapacityWait.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelRuntimeDefinition.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "PBDRigidsSolver.h"
#include "PhysicsEngine/BoxElem.h"
#include "Voxel/VoxelModule.h"

struct FVoxelDetailAssetBatch
{
	enum class EPhase : uint8 { Capture, Create, Instances, Boxes, Cook, PhysicsSync };
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PhysicalLease;
	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> PhysicsSync;
	TArray<FTransform> Transforms;
	TArray<FBox> Boxes;
	TWeakObjectPtr<AActor> Host;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
	TWeakObjectPtr<UVoxelCollisionComponent> Collision;
	FVector Anchor = FVector::ZeroVector;
	FVoxelGenerationConsumerKey Consumer;
	int32 PlacementCursor = 0;
	int32 PlacementEnd = 0;
	int32 ChunkEnd = 0;
	int32 InstanceCursor = 0;
	int32 BoxCursor = 0;
	EPhase Phase = EPhase::Capture;
};

namespace
{
	struct FDetailPhysicsSyncCommand
	{
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> Sync;
		TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> Ticket;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		Chaos::FPBDRigidsSolver* Solver = nullptr;
		void operator()() const
		{
			if (Ticket) Ticket->ConfirmPhysicsCommitted(*Solver);
			Sync->Store(true);
		}
	};

	constexpr int32 DetailRadiusCells = 160;
	constexpr int32 MaxDetailAssets = 64;
	constexpr int32 MaxDetailInstances = 8192;
	constexpr int32 MaxInstancesPerAsset = 2048;
	constexpr int32 InstancesPerHost = 256;
	constexpr int32 DetailMicrostep = 32;
	constexpr double DetailNearRadiusCm = 1200.0;
	constexpr double DetailMiddleRadiusCm = 12000.0;

	enum class EDetailPresentationBand : uint8
	{
		Near,
		Middle,
		Far
	};

	EDetailPresentationBand ResolveDetailBand(
		const FVector& InWorldLocation,
		const TConstArrayView<FVector> InObservers)
	{
		double MinimumDistanceSquared = TNumericLimits<double>::Max();
		for (const FVector& Observer : InObservers)
		{
			MinimumDistanceSquared = FMath::Min(
				MinimumDistanceSquared,
				FVector::DistSquared(Observer, InWorldLocation));
		}
		if (MinimumDistanceSquared <= FMath::Square(DetailNearRadiusCm))
		{
			return EDetailPresentationBand::Near;
		}
		if (MinimumDistanceSquared <= FMath::Square(DetailMiddleRadiusCm))
		{
			return EDetailPresentationBand::Middle;
		}
		return EDetailPresentationBand::Far;
	}

	bool SamePlacement(
		const FVoxelStructureDetailPlacement& InA,
		const FVoxelStructureDetailPlacement& InB)
	{
		return InA.OwnerId == InB.OwnerId &&
			InA.DetailId == InB.DetailId &&
			InA.Position == InB.Position &&
			InA.Yaw == InB.Yaw;
	}
}

uint64 FVoxelDetailPlan::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelDetailPlan>() + Placements.GetAllocatedSize();
}

FVoxelDetailView::FVoxelDetailView(
	UVoxelModule& InModule,
	FVoxelTaskScheduler& InScheduler,
	const uint64 InWorldEpoch)
	: Module(InModule)
	, Scheduler(InScheduler)
	, WorldEpoch(InWorldEpoch)
{
	CommitHandle = Module.OnBlocksCommitted.AddRaw(this, &FVoxelDetailView::MarkDirty);
}

FVoxelDetailView::~FVoxelDetailView()
{
	Reset();
	Module.OnBlocksCommitted.Remove(CommitHandle);
}

bool FVoxelDetailView::Initialize(FString& OutError)
{
	bStopped = false;
	UVoxelWorldGenerationProfile* Profile = Module.GetWorldGenerationProfile();
	if (!Module.GetWorld() || !Module.GetGenerator() || !Profile)
	{
		OutError = TEXT("Voxel detail view dependencies are unavailable");
		return false;
	}
	if (Profile->Details.Num() > MaxDetailAssets)
	{
		OutError = TEXT("Voxel detail profile exceeds the 64 asset limit");
		return false;
	}
	using namespace VoxelBoundedAllocation;
	const auto StorageConsumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, OwnerIncarnation, {}, 1);
	const uint64 StorageBytes = 3 * ArrayBytes<TWeakObjectPtr<AActor>>(MaxDetailAssets * MaxInstancesPerAsset / InstancesPerHost) +
		ArrayBytes<TStrongObjectPtr<UVoxelDetailData>>(MaxDetailAssets) + ArrayBytes<TStrongObjectPtr<UStaticMesh>>(MaxDetailAssets) +
		MapBytes<FName, int32>(MaxDetailAssets) + 3 * ArrayBytes<FVector>(8) + 5 * ArrayBytes<FIntVector>(8);
	const uint64 FactMetadataBytes = ArrayBytes<FVoxelGenerationConsumerKey>(8);
	if (!Scheduler.GetTerrainResources()->Acquire(StorageConsumer, EVoxelTerrainResourcePool::Resident, StorageBytes + FactMetadataBytes, StorageLease))
	{
		OutError = TEXT("CapacityBlocked: Detail 所有者容器等待驻留容量");
		return false;
	}
	Assets.Reserve(MaxDetailAssets);
	Meshes.Reserve(MaxDetailAssets);
	AssetIndices.Reserve(MaxDetailAssets);
	ActiveActors.Reserve(MaxDetailAssets * MaxInstancesPerAsset / InstancesPerHost);
	StagedActors.Reserve(MaxDetailAssets * MaxInstancesPerAsset / InstancesPerHost);
	RetiringActors.Reserve(MaxDetailAssets * MaxInstancesPerAsset / InstancesPerHost);
	Observers.Reserve(8);
	LastObserverSections.Reserve(8);
	PlannedObserverCells.Reserve(8);
	FactConsumers.Reserve(8);

	const bool bRender = Module.GetWorld()->GetNetMode() != NM_DedicatedServer;
	for (const TSoftObjectPtr<UVoxelDetailData>& Reference : Profile->Details)
	{
		UVoxelDetailData* Asset = Reference.LoadSynchronous();
		if (!Asset || !Asset->Validate(OutError) || AssetIndices.Contains(Asset->StableKey))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Voxel detail profile contains an invalid or duplicate asset");
			}
			Reset();
			return false;
		}
		UStaticMesh* Mesh = bRender ? Asset->Mesh.LoadSynchronous() : nullptr;
		if (bRender && !Mesh)
		{
			OutError = TEXT("Voxel detail mesh is missing from cooked data");
			Reset();
			return false;
		}
		const int32 Index = Assets.Num();
		AssetIndices.Add(Asset->StableKey, Index);
		Assets.Emplace(Asset);
		Meshes.Emplace(Mesh);
	}
	OutError.Reset();
	return true;
}

void FVoxelDetailView::Tick(const TConstArrayView<FVector> InObservers)
{
	if (bStopped || !Module.IsReady())
	{
		return;
	}
	auto& Budget = Module.GetTerrainFrameContext().GetPublishBudget();
	const double Deadline = Module.GetTerrainFrameContext().Deadline(0.1);
	for (int32 Work = 0; Work < 4 && !RetiringActors.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Work)
	{
		const double Started = FPlatformTime::Seconds();
		if (!Budget.TryStart(EVoxelPublishPhase::Retire, Started, 0.01)) return;
		const auto Actor = RetiringActors.Pop(EAllowShrinking::No);
		if (Actor.IsValid())
		{
			if (auto* Collision = Actor->FindComponentByClass<UVoxelCollisionComponent>()) Collision->DestroyComponent();
			Actor->Destroy();
		}
		Budget.Finish(EVoxelPublishPhase::Retire, Started, FPlatformTime::Seconds());
	}
	const double UpdateStarted = FPlatformTime::Seconds();
	if (!Budget.TryStart(EVoxelPublishPhase::Priority, UpdateStarted, 0.01)) return;
	if (InObservers.Num() > ObserverCapacity)
	{
		using namespace VoxelBoundedAllocation;
		if (InObservers.Num() > MAX_int32 / 2)
		{
			Budget.Finish(EVoxelPublishPhase::Priority, UpdateStarted, FPlatformTime::Seconds());
			return;
		}
		const int32 Capacity = int32(FMath::RoundUpToPowerOfTwo(uint32(InObservers.Num())));
		const uint64 PreviousBytes = 3 * ArrayBytes<FVector>(ObserverCapacity) + 5 * ArrayBytes<FIntVector>(ObserverCapacity) +
			ArrayBytes<FVoxelGenerationConsumerKey>(ObserverCapacity);
		const uint64 NextBytes = 3 * ArrayBytes<FVector>(Capacity) + 5 * ArrayBytes<FIntVector>(Capacity) +
			ArrayBytes<FVoxelGenerationConsumerKey>(Capacity);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Overlap;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, OwnerIncarnation, {}, NextToken);
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Working, PreviousBytes, Overlap) ||
			!StorageLease->Resize(StorageLease->Bytes() - PreviousBytes + NextBytes))
		{
			Budget.Finish(EVoxelPublishPhase::Priority, UpdateStarted, FPlatformTime::Seconds());
			return;
		}
		Observers.Reserve(Capacity);
		LastObserverSections.Reserve(Capacity);
		PlannedObserverCells.Reserve(Capacity);
		FactConsumers.Reserve(Capacity);
		ObserverCapacity = Capacity;
	}
	Observers.Reset();
	Observers.Append(InObservers.GetData(), InObservers.Num());
	TArray<FIntVector> ObserverSections;
	ObserverSections.Reserve(ObserverCapacity);
	for (const FVector& Observer : InObservers)
	{
		const FVector Cell = Observer / Module.BlockSize();
		ObserverSections.AddUnique(FIntVector(
			VoxelGeneration::FloorDivide(FMath::FloorToInt(Cell.X), 16),
			VoxelGeneration::FloorDivide(FMath::FloorToInt(Cell.Y), 16),
			VoxelGeneration::FloorDivide(FMath::FloorToInt(Cell.Z), 16)));
	}
	ObserverSections.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.Z != InB.Z) return InA.Z < InB.Z;
		if (InA.Y != InB.Y) return InA.Y < InB.Y;
		return InA.X < InB.X;
	});
	if (ObserverSections != LastObserverSections)
	{
		CancelPlanning();
		LastObserverSections = ObserverSections;
		bDirty = true;
	}
	Budget.Finish(EVoxelPublishPhase::Priority, UpdateStarted, FPlatformTime::Seconds());
	if (ObserverSections.IsEmpty())
	{
		if (bRunning || WaitingToken || !FactConsumers.IsEmpty()) CancelPlanning();
		if (PendingPlan || AssetBatch || !StagedActors.IsEmpty()) AbortStaging();
		for (int32 Work = 0; Work < 4 && !ActiveActors.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Work)
		{
			const double Started = FPlatformTime::Seconds();
			if (!Budget.TryStart(EVoxelPublishPhase::Retire, Started, 0.01)) return;
			const auto Actor = ActiveActors.Pop(EAllowShrinking::No);
			if (Actor.IsValid())
			{
				if (auto* Collision = Actor->FindComponentByClass<UVoxelCollisionComponent>()) Collision->DestroyComponent();
				Actor->Destroy();
			}
			Budget.Finish(EVoxelPublishPhase::Retire, Started, FPlatformTime::Seconds());
		}
		bDirty = true;
		return;
	}

	if (PendingPlan)
	{
		if (bDirty)
		{
			AbortStaging();
		}
		else if (StagedAssetIndex < Assets.Num())
		{
			if (bCapacityBlocked && Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion <= CapacityVersion) return;
			FString Error;
			bool bComplete = false;
			const double Started = FPlatformTime::Seconds();
			if (!Budget.TryStart(EVoxelPublishPhase::Prepare, Started, 0.01)) return;
			const bool bSuccess = BuildAssetBatch(StagedAssetIndex, bComplete, Error);
			bCapacityBlocked = !bSuccess && Error.StartsWith(TEXT("CapacityBlocked"));
			if (bCapacityBlocked) CapacityVersion = Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion;
			Budget.Finish(EVoxelPublishPhase::Prepare, Started, FPlatformTime::Seconds());
			if (!bSuccess && !Error.StartsWith(TEXT("CapacityBlocked")))
			{
				UE_LOG(LogTemp, Error, TEXT("Voxel detail staging failed: %s"), *Error);
				AbortStaging();
				bDirty = true;
				LastRequestTime = FPlatformTime::Seconds();
			}
			else if (bComplete) ++StagedAssetIndex;
			return;
		}
		else
		{
			for (int32 Work = 0; Work < 4 && StagedActors.IsValidIndex(PresentationCursor) && FPlatformTime::Seconds() < Deadline; ++Work)
			{
				const double Started = FPlatformTime::Seconds();
				if (!Budget.TryStart(EVoxelPublishPhase::Commit, Started, 0.01)) return;
				const auto Actor = StagedActors[PresentationCursor++];
				if (Actor.IsValid())
				{
					Actor->SetActorHiddenInGame(false);
					Actor->SetActorEnableCollision(true);
				}
				Budget.Finish(EVoxelPublishPhase::Commit, Started, FPlatformTime::Seconds());
			}
			if (PresentationCursor != StagedActors.Num()) return;
			check(RetiringActors.IsEmpty());
			Swap(RetiringActors, ActiveActors);
			Swap(ActiveActors, StagedActors);
			if (PendingPlan->WorkReceipt) PendingPlan->WorkReceipt->Resolve(true);
			PendingPlan.Reset();
			StagedAssetIndex = 0;
			PresentationCursor = 0;
		}
	}

	const double Now = FPlatformTime::Seconds();
	if (bRunning || !bDirty || !RetiringActors.IsEmpty() || ObserverSections.IsEmpty() ||
		Now - LastRequestTime < 0.5)
	{
		return;
	}

	TArray<FIntVector> ObserverCells;
	ObserverCells.Reserve(ObserverCapacity);
	for (const FVector& Observer : InObservers)
	{
		const FVector Cell = Observer / Module.BlockSize();
		ObserverCells.Add(FIntVector(
			FMath::FloorToInt(Cell.X),
			FMath::FloorToInt(Cell.Y),
			FMath::FloorToInt(Cell.Z)));
	}
	ObserverCells.Sort([](const FIntVector& A, const FIntVector& B)
	{
		return A.Z != B.Z ? A.Z < B.Z : A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	});
	if (bCapacityBlocked && Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion <= CapacityVersion) return;
	if (WaitingToken == 0)
	{
		WaitingToken = NextToken++;
		PlannedObserverCells = ObserverCells;
	}
	else ObserverCells = PlannedObserverCells;
	const uint64 Token = WaitingToken;
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		Module.GetGenerationConfig();
	const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
		Module.GetGenerationCache();
	bool bFactsReady = true;
	FVoxelTerrainServicePriority FactPriority;
	FactPriority.Band = EVoxelTerrainServiceBand::P2;
	FactPriority.SourceStableKey = OwnerId;
	for (const FIntVector& Center : ObserverCells)
	{
		const FVoxelGenerationBounds Bounds{FIntVector(Center.X - DetailRadiusCells, Center.Y - DetailRadiusCells,
			Module.GetManifest().Settings.MinZ), FIntVector(Center.X + DetailRadiusCells + 1,
			Center.Y + DetailRadiusCells + 1, Module.GetManifest().Settings.MaxZ)};
		const FVoxelGenerationConsumerKey FactConsumer{EVoxelGenerationConsumerKind::WorldFact, WorldEpoch,
			OwnerId, OwnerIncarnation, EVoxelGenerationSpatialKind::Bounds, Bounds.Min, Bounds.Max, Token};
		if (!FactConsumers.Contains(FactConsumer)) FactConsumers.Add(FactConsumer);
		const auto Status = Module.EnsureGenerationFacts(FactConsumer, Bounds, FactPriority);
		if (Status.bFailed) { CancelPlanning(); LastRequestTime = Now; return; }
		bFactsReady &= Status.bReady;
	}
	if (!bFactsReady) return;
	FVoxelTaskRequest Request;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, OwnerIncarnation, {}, Token);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PlanLease;
	const uint64 PlanBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelDetailPlan>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelStructureDetailPlacement>(MaxDetailInstances);
	if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, PlanBytes, PlanLease))
	{
		bCapacityBlocked = true;
		CapacityVersion = Scheduler.GetTerrainResources()->Snapshot().ReleaseVersion;
		return;
	}
	bCapacityBlocked = false;
	Request.Kind = EVoxelTaskKind::BuildDetails;
	Request.WorkClass = EVoxelWorkClass::Background;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.OwnerId = OwnerId;
	Request.Stamp.OwnerIncarnation = OwnerIncarnation;
	Request.Stamp.Token = Token;
	Request.ReservedBytes = 8ull * 1024ull * 1024ull;
	Request.InputBytes = ObserverCells.GetAllocatedSize();
	Request.GenerationConsumer = {EVoxelGenerationConsumerKind::WorldFact, WorldEpoch, OwnerId, OwnerIncarnation,
		EVoxelGenerationSpatialKind::Section, Request.Stamp.Section, Request.Stamp.Section, Token};
	const auto CapacityWait = FVoxelGenerationCapacityWait::Create(Scheduler.GetRuntimeResources(), Request.GenerationConsumer);
	if (!CapacityWait) return;
	Request.bResumable = true;
	Request.GetBlockReason = [CapacityWait] { return CapacityWait->BlockReason(); };
	Request.Execute = [Config, Cache, PlanLease, CapacityWait, ObserverCells = MoveTemp(ObserverCells), Token](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		const uint64 BeforeBuild = CapacityWait->Begin();
		if (!Config || !Cache)
		{
			Result.Error = TEXT("Voxel detail generation dependencies are unavailable");
			return Result;
		}
		TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Plan =
			VoxelBoundedAllocation::MakeWorkerShared<FVoxelDetailPlan>(PlanLease);
		Plan->ResourceLease = PlanLease;
		Plan->Placements.Reserve(MaxDetailInstances);
		Plan->Token = Token;
		for (const FIntVector& Center : ObserverCells)
		{
			if (InCancel.Load())
			{
				return Result;
			}
			FVoxelGenerationQuery Query;
			if (!FVoxelGenerationQuery::Create(
				Config.ToSharedRef(),
				Cache.ToSharedRef(),
				Query,
				Result.Error))
			{
				Result.bCanceled = InCancel.Load();
				CapacityWait->Continue(Result, BeforeBuild);
				return Result;
			}
			const FVoxelGenerationBounds Bounds {
				FIntVector(
					Center.X - DetailRadiusCells,
					Center.Y - DetailRadiusCells,
					Config->Recipe->Settings.MinZ),
				FIntVector(
					Center.X + DetailRadiusCells + 1,
					Center.Y + DetailRadiusCells + 1,
					Config->Recipe->Settings.MaxZ) };
			if (!Query.PrepareStructuresOnly(Bounds, Result.Error, &InCancel))
			{
				Result.bCanceled = InCancel.Load();
				CapacityWait->Continue(Result, BeforeBuild);
				return Result;
			}
			for (const FVoxelStructurePlanPtr& StructurePlan : Query.GetPreparedStructurePlans())
			{
				if (!StructurePlan) continue;
				for (const FVoxelStructureDetailPlacement& Placement : StructurePlan->Details)
				{
					const FIntVector Delta = Placement.Position - Center;
					if (FMath::Abs(Delta.X) <= DetailRadiusCells &&
						FMath::Abs(Delta.Y) <= DetailRadiusCells &&
						FMath::Abs(Delta.Z) <= DetailRadiusCells)
					{
						if (Plan->Placements.Num() == MaxDetailInstances)
						{
							Result.Error = TEXT("Voxel detail plan exceeds the 8192 instance limit");
							return Result;
						}
						Plan->Placements.Add(Placement);
					}
				}
			}
		}
		Plan->Placements.Sort([](
			const FVoxelStructureDetailPlacement& InA,
			const FVoxelStructureDetailPlacement& InB)
		{
			if (InA.DetailId != InB.DetailId) return InA.DetailId.LexicalLess(InB.DetailId);
			if (InA.OwnerId != InB.OwnerId) return InA.OwnerId < InB.OwnerId;
			if (InA.Position.Z != InB.Position.Z) return InA.Position.Z < InB.Position.Z;
			if (InA.Position.Y != InB.Position.Y) return InA.Position.Y < InB.Position.Y;
			if (InA.Position.X != InB.Position.X) return InA.Position.X < InB.Position.X;
			return InA.Yaw < InB.Yaw;
		});
		for (int32 Index = Plan->Placements.Num() - 1; Index > 0; --Index)
		{
			if (SamePlacement(Plan->Placements[Index - 1], Plan->Placements[Index]))
			{
				Plan->Placements.RemoveAt(Index, 1, EAllowShrinking::No);
			}
		}
		Result.Details = MoveTemp(Plan);
		Result.bSuccess = !InCancel.Load();
		return Result;
	};
	if (Scheduler.Enqueue(MoveTemp(Request)))
	{
		WaitingToken = Token;
		bRunning = true;
		bDirty = false;
		LastRequestTime = Now;
	}
}

bool FVoxelDetailView::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::BuildDetails)
	{
		return false;
	}
	if (bStopped || InResult.Stamp.WorldEpoch != WorldEpoch ||
		InResult.Stamp.Token != WaitingToken || InResult.Stamp.OwnerId != OwnerId ||
		InResult.Stamp.OwnerIncarnation != OwnerIncarnation)
	{
		return true;
	}
	bRunning = false;
	for (const auto& Consumer : FactConsumers) Module.ReleaseGenerationFacts(Consumer);
	FactConsumers.Reset();
	WaitingToken = 0;
	if (!InResult.bSuccess || InResult.bCanceled || !InResult.Details)
	{
		bDirty = true;
		LastRequestTime = FPlatformTime::Seconds();
		if (!InResult.bCanceled)
		{
			UE_LOG(LogTemp, Error, TEXT("Voxel detail plan failed: %s"), *InResult.Error);
		}
		return true;
	}
	AbortStaging();
	InResult.Details->WorkReceipt = InResult.WorkReceipt;
	PendingPlan = MoveTemp(InResult.Details);
	return true;
}

void FVoxelDetailView::Reset()
{
	bStopped = true;
	CancelPlanning();
	AbortStaging();
	DestroyActors(ActiveActors);
	DestroyActors(RetiringActors);
	ActiveActors.Empty(); StagedActors.Empty(); RetiringActors.Empty();
	Assets.Empty(); Meshes.Empty(); AssetIndices.Empty();
	LastObserverSections.Empty(); Observers.Empty();
	PlannedObserverCells.Empty();
	FactConsumers.Empty();
	ObserverCapacity = 8;
	StorageLease.Reset();
}

bool FVoxelDetailView::BuildAssetBatch(const int32 InAssetIndex, bool& bOutComplete, FString& OutError)
{
	bOutComplete = false;
	if (!PendingPlan || !Assets.IsValidIndex(InAssetIndex) || !Meshes.IsValidIndex(InAssetIndex))
	{
		OutError = TEXT("Detail 分步准备没有完整输入");
		return false;
	}
	UVoxelDetailData* Asset = Assets[InAssetIndex].Get();
	UWorld* World = Module.GetWorld();
	if (!Asset || !World) { OutError = TEXT("Detail 资产或世界已经退出"); return false; }
	const auto& Placements = PendingPlan->Placements;
	if (!AssetBatch)
	{
		// Worker 已按资产和稳定实体身份排序；局部批次用二分查找，不在 GT 重扫整份计划。
		int32 First = 0, Last = Placements.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Placements[Middle].DetailId.LexicalLess(Asset->StableKey)) First = Middle + 1;
			else Last = Middle;
		}
		const int32 Begin = First;
		Last = Placements.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (!Asset->StableKey.LexicalLess(Placements[Middle].DetailId)) First = Middle + 1;
			else Last = Middle;
		}
		if (Begin == First) { bOutComplete = true; return true; }
		if (First - Begin > MaxInstancesPerAsset) { OutError = TEXT("Detail 单资产实例超过 2048"); return false; }
		const FIntVector Position = Placements[Begin].Position;
		const FIntVector Section(VoxelGeneration::FloorDivide(Position.X, 16), VoxelGeneration::FloorDivide(Position.Y, 16),
			VoxelGeneration::FloorDivide(Position.Z, 16));
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, OwnerIncarnation, Section, NextToken++);
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelDetailAssetBatch>() +
			VoxelBoundedAllocation::SharedBytes<TAtomic<bool>>() +
			VoxelBoundedAllocation::ArrayBytes<FTransform>(DetailMicrostep) +
			VoxelBoundedAllocation::ArrayBytes<FTransform>(InstancesPerHost) +
			VoxelBoundedAllocation::ArrayBytes<FBox>(InstancesPerHost * Asset->CollisionBoxesCm.Num());
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Bytes, Lease))
		{ OutError = TEXT("CapacityBlocked: Detail 局部准备等待容量"); return false; }
		AssetBatch = VoxelBoundedAllocation::MakeWorkerShared<FVoxelDetailAssetBatch>(Lease);
		AssetBatch->MetadataLease = MoveTemp(Lease);
		AssetBatch->Consumer = Consumer;
		AssetBatch->PlacementCursor = Begin;
		AssetBatch->PlacementEnd = First;
		AssetBatch->ChunkEnd = FMath::Min(Begin + InstancesPerHost, First);
		AssetBatch->Anchor = FVector(Placements[Begin].Position) * Module.BlockSize();
		AssetBatch->Transforms.Reserve(InstancesPerHost);
		AssetBatch->Boxes.Reserve(InstancesPerHost * Asset->CollisionBoxesCm.Num());
		AssetBatch->PhysicsSync = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	}
	auto& Batch = *AssetBatch;
	if (Batch.Phase == FVoxelDetailAssetBatch::EPhase::Capture)
	{
		const int32 End = FMath::Min(Batch.PlacementCursor + DetailMicrostep, Batch.ChunkEnd);
		for (; Batch.PlacementCursor < End; ++Batch.PlacementCursor)
		{
			const auto& Placement = Placements[Batch.PlacementCursor];
			FVoxelBlockState Support;
			if (Module.GetRuntime()->TryGetBlock(Placement.Position - FIntVector(0, 0, 1), Support))
			{
				const auto* Definition = Module.GetRegistry()->Find(Support.TypeId);
				if (!Definition || !Definition->bSolid) continue;
			}
			const FVector Location = FVector(Placement.Position) * Module.BlockSize();
			const auto Band = ResolveDetailBand(Location, Observers);
			if (Band == EDetailPresentationBand::Far) continue;
			const FTransform Transform(FRotator(0.0, double(Placement.Yaw) * 90.0, 0.0), Location - Batch.Anchor);
			Batch.Transforms.Add(Transform);
			if (Asset->bNearCollision && Band == EDetailPresentationBand::Near)
				for (const FBox& Box : Asset->CollisionBoxesCm) Batch.Boxes.Add(Box.TransformBy(Transform));
		}
		if (Batch.PlacementCursor == Batch.ChunkEnd) Batch.Phase = FVoxelDetailAssetBatch::EPhase::Create;
		return true;
	}
	const auto FinishChunk = [&]
	{
		if (Batch.PlacementCursor == Batch.PlacementEnd) { AssetBatch.Reset(); bOutComplete = true; return; }
		Batch.ChunkEnd = FMath::Min(Batch.PlacementCursor + InstancesPerHost, Batch.PlacementEnd);
		Batch.Anchor = FVector(Placements[Batch.PlacementCursor].Position) * Module.BlockSize();
		Batch.Transforms.Reset(); Batch.Boxes.Reset();
		Batch.Host.Reset(); Batch.Instances.Reset(); Batch.Collision.Reset(); Batch.PhysicalLease.Reset();
		Batch.InstanceCursor = Batch.BoxCursor = 0;
		Batch.PhysicsSync->Store(false);
		Batch.Consumer.RequestSerial = NextToken++;
		Batch.Phase = FVoxelDetailAssetBatch::EPhase::Capture;
	};
	if (Batch.Phase == FVoxelDetailAssetBatch::EPhase::Create)
	{
		if (Batch.Transforms.IsEmpty()) { FinishChunk(); return true; }
		if (!Batch.Boxes.IsEmpty() && !Batch.PhysicalLease)
		{
			// 与精确 Collision 相同的独立物理驻留池；旧 Actor 尚未退休时保持交接峰值。
			const uint64 PhysicalBytes = 4096 + VoxelBoundedAllocation::ArrayBytes<FKBoxElem>(Batch.Boxes.Num()) +
				UVoxelCollisionComponent::PreparedReclaimBytes() +
				uint64(Batch.Boxes.Num()) * 1024;
			if (!Scheduler.GetTerrainResources()->Acquire(Batch.Consumer, EVoxelTerrainResourcePool::Resident,
				PhysicalBytes, Batch.PhysicalLease, 1)) { OutError = TEXT("CapacityBlocked: Detail 碰撞等待交接容量"); return false; }
		}
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Batch.Anchor), Parameters);
		if (!Host) { OutError = TEXT("Detail 局部 Actor 分配失败"); return false; }
		Host->SetReplicates(false);
		Host->SetActorHiddenInGame(true);
		Batch.Host = Host;
		StagedActors.Add(Host);
		USceneComponent* Root = NewObject<USceneComponent>(Host);
		Host->SetRootComponent(Root);
		Root->RegisterComponent();
		if (UStaticMesh* Mesh = Meshes[InAssetIndex].Get())
		{
			auto* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Host);
			Instances->SetupAttachment(Root);
			Instances->SetStaticMesh(Mesh);
			Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instances->SetGenerateOverlapEvents(false);
			Instances->SetCanEverAffectNavigation(false);
			Instances->SetCullDistances(0, FMath::RoundToInt(DetailMiddleRadiusCm));
			Instances->bAutoRebuildTreeOnInstanceChanges = false;
			Instances->RegisterComponent();
			Batch.Instances = Instances;
		}
		if (!Batch.Boxes.IsEmpty())
		{
			auto* Collision = NewObject<UVoxelCollisionComponent>(Host);
			Collision->SetupAttachment(Root);
			Collision->RegisterComponent();
			Batch.Collision = Collision;
			if (!Collision->BeginPrepare(Batch.Boxes.Num(), 1.0, Batch.PhysicalLease))
			{ OutError = TEXT("Detail 碰撞输入准备失败"); return false; }
		}
		Batch.Phase = FVoxelDetailAssetBatch::EPhase::Instances;
		return true;
	}
	if (Batch.Phase == FVoxelDetailAssetBatch::EPhase::Instances)
	{
		const int32 Count = FMath::Min(DetailMicrostep, Batch.Transforms.Num() - Batch.InstanceCursor);
		if (auto* Instances = Batch.Instances.Get())
		{
			TArray<FTransform> Slice;
			Slice.Reserve(DetailMicrostep);
			Slice.Append(Batch.Transforms.GetData() + Batch.InstanceCursor, Count);
			Instances->AddInstances(Slice, false, false, false);
		}
		Batch.InstanceCursor += Count;
		if (Batch.InstanceCursor == Batch.Transforms.Num())
		{
			if (auto* Instances = Batch.Instances.Get()) Instances->BuildTreeIfOutdated(true, true);
			Batch.Phase = FVoxelDetailAssetBatch::EPhase::Boxes;
		}
		return true;
	}
	if (Batch.Phase == FVoxelDetailAssetBatch::EPhase::Boxes)
	{
		if (auto* Collision = Batch.Collision.Get())
		{
			const int32 Count = FMath::Min(DetailMicrostep, Batch.Boxes.Num() - Batch.BoxCursor);
			if (!Collision->AppendPreparedBoxes(MakeArrayView(Batch.Boxes.GetData() + Batch.BoxCursor, Count)))
			{ OutError = TEXT("Detail 碰撞分步写入失败"); return false; }
			Batch.BoxCursor += Count;
			if (Batch.BoxCursor != Batch.Boxes.Num()) return true;
		}
		Batch.Phase = FVoxelDetailAssetBatch::EPhase::Cook;
		return true;
	}
	if (Batch.Phase == FVoxelDetailAssetBatch::EPhase::Cook)
	{
		auto* Collision = Batch.Collision.Get();
		if (!Collision) { FinishChunk(); return true; }
		FVoxelTerrainServicePriority Priority;
		Priority.Band = EVoxelTerrainServiceBand::P2;
		Priority.SourceStableKey = OwnerId;
		if (!Collision->BeginDetachedCook(Scheduler, Batch.Consumer, Batch.PhysicalLease, Priority, LastRequestTime,
			Module.GetPhysicsRetirement()))
		{ OutError = TEXT("CapacityBlocked: Detail 等待后台 Cook 接纳"); return false; }
		if (Collision->IsDetachedCookRunning()) return true;
		if (Collision->HasDetachedCookFailed()) { OutError = TEXT("Detail 后台 Cook 失败"); return false; }
		FPhysScene* Scene = World->GetPhysicsScene();
		if (!Scene || !Scene->GetSolver()) { OutError = TEXT("Detail 物理场景已经退出"); return false; }
		if (!Collision->PrepareNativeRetirement(PendingPlan ? PendingPlan->WorkReceipt : nullptr))
		{
			if (!Collision->IsNativeRetirementCapacityBlocked()) return true;
			OutError = TEXT("CapacityBlocked: Detail 等待物理退休票据接纳");
			return false;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CommandLease;
		const uint64 CommandBytes = FMemory::QuantizeSize(sizeof(Chaos::FSimCallbackCommandObject),
			alignof(Chaos::FSimCallbackCommandObject)) + VoxelBoundedAllocation::FunctionBytes<FDetailPhysicsSyncCommand>();
		if (!Scheduler.GetTerrainResources()->Acquire(Batch.Consumer, EVoxelTerrainResourcePool::Input, CommandBytes, CommandLease))
		{ OutError = TEXT("CapacityBlocked: Detail 等待物理同步命令容量"); return false; }
		bool bInstalled = false;
		FPhysicsCommand::ExecuteWrite(Scene, [&] { bInstalled = Collision->InstallPreparedPhysics(); });
		if (!bInstalled) { OutError = TEXT("Detail 物理批次安装失败"); return false; }
		Scene->GetSolver()->EnqueueCommandImmediate(FDetailPhysicsSyncCommand{
			Batch.PhysicsSync, Collision->CaptureNativeTicket(), MoveTemp(CommandLease), Scene->GetSolver()});
		Batch.Phase = FVoxelDetailAssetBatch::EPhase::PhysicsSync;
		return true;
	}
	if (!Batch.PhysicsSync->Load()) return true;
	if (auto* Collision = Batch.Collision.Get())
	{
		const auto Ticket = Collision->CaptureNativeTicket();
		if (Ticket && !Ticket->bPhysicsCommitted.Load()) { OutError = TEXT("Detail 未取得有效 PT 碰撞提交证明"); return false; }
		Collision->MarkPhysicsCommitted();
	}
	FinishChunk();
	return true;
}
void FVoxelDetailView::DestroyActors(TArray<TWeakObjectPtr<AActor>>& InActors)
{
	for (const TWeakObjectPtr<AActor>& Actor : InActors)
	{
		if (Actor.IsValid())
		{
			if (auto* Collision = Actor->FindComponentByClass<UVoxelCollisionComponent>()) Collision->DestroyComponent();
			Actor->Destroy();
		}
	}
	InActors.Reset();
}

void FVoxelDetailView::AbortStaging()
{
	if (!StagedActors.IsEmpty())
	{
		check(RetiringActors.IsEmpty());
		Swap(RetiringActors, StagedActors);
	}
	AssetBatch.Reset();
	bCapacityBlocked = false;
	PendingPlan.Reset();
	StagedAssetIndex = 0;
	PresentationCursor = 0;
}

void FVoxelDetailView::MarkDirty(const FVoxelEditBatch& InBatch)
{
	for (const auto& Pair : InBatch.Sections)
	{
		const FIntVector Min = Pair.Section * 16;
		for (const FVector& Observer : Observers)
		{
			const FVector Center = Observer / Module.BlockSize();
			if (Center.X + DetailRadiusCells >= Min.X && Center.X - DetailRadiusCells < Min.X + 16 &&
				Center.Y + DetailRadiusCells >= Min.Y && Center.Y - DetailRadiusCells < Min.Y + 16 &&
				Center.Z + DetailRadiusCells >= Min.Z && Center.Z - DetailRadiusCells < Min.Z + 16)
			{
				CancelPlanning(); bDirty = true; return;
			}
		}
	}
}

void FVoxelDetailView::CancelPlanning()
{
	Scheduler.CancelMatching([Id = OwnerId](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{ return Kind == EVoxelTaskKind::BuildDetails && Stamp.OwnerId == Id; });
	for (const auto& Consumer : FactConsumers) Module.ReleaseGenerationFacts(Consumer);
	FactConsumers.Reset();
	++OwnerIncarnation;
	PlannedObserverCells.Reset();
	WaitingToken = 0;
	bRunning = false;
	bCapacityBlocked = false;
}
