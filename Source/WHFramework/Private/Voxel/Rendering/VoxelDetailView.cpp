#include "Voxel/Rendering/VoxelDetailView.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Runtime/VoxelRuntimeDefinition.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr int32 DetailRadiusCells = 160;
	constexpr int32 MaxDetailAssets = 64;
	constexpr int32 MaxDetailInstances = 8192;
	constexpr int32 MaxInstancesPerAsset = 2048;
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
	return sizeof(FVoxelDetailPlan) + Placements.GetAllocatedSize();
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
	Observers = TArray<FVector>(InObservers);
	TArray<FIntVector> ObserverSections;
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
		LastObserverSections = ObserverSections;
		bDirty = true;
	}

	if (PendingPlan)
	{
		if (bDirty)
		{
			AbortStaging();
		}
		else if (StagedAssetIndex < Assets.Num())
		{
			FString Error;
			if (!BuildAssetBatch(StagedAssetIndex++, Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Voxel detail staging failed: %s"), *Error);
				AbortStaging();
				bDirty = true;
				LastRequestTime = FPlatformTime::Seconds();
			}
			return;
		}
		else
		{
			DestroyActors(ActiveActors);
			ActiveActors = MoveTemp(StagedActors);
			for (const TWeakObjectPtr<AActor>& Actor : ActiveActors)
			{
				if (Actor.IsValid())
				{
					Actor->SetActorHiddenInGame(false);
					Actor->SetActorEnableCollision(true);
				}
			}
			PendingPlan.Reset();
			StagedAssetIndex = 0;
		}
	}

	const double Now = FPlatformTime::Seconds();
	if (bRunning || !bDirty || ObserverSections.IsEmpty() ||
		ObserverSections.Num() > 8 || Now - LastRequestTime < 0.5)
	{
		return;
	}

	TArray<FIntVector> ObserverCells;
	for (const FVector& Observer : InObservers)
	{
		const FVector Cell = Observer / Module.BlockSize();
		ObserverCells.Add(FIntVector(
			FMath::FloorToInt(Cell.X),
			FMath::FloorToInt(Cell.Y),
			FMath::FloorToInt(Cell.Z)));
	}
	const uint64 Token = NextToken++;
	const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		Module.GetGenerationConfig();
	const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
		Module.GetGenerationCache();
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildDetails;
	Request.WorkClass = EVoxelWorkClass::Background;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Token = Token;
	Request.ReservedBytes = 8ull * 1024ull * 1024ull;
	Request.InputBytes = ObserverCells.GetAllocatedSize();
	Request.Execute = [Config, Cache, ObserverCells = MoveTemp(ObserverCells), Token](const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		if (!Config || !Cache)
		{
			Result.Error = TEXT("Voxel detail generation dependencies are unavailable");
			return Result;
		}
		TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Plan =
			MakeShared<FVoxelDetailPlan, ESPMode::ThreadSafe>();
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
			if (!Query.Prepare(Bounds, Result.Error, &InCancel))
			{
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
						Plan->Placements.Add(Placement);
						if (Plan->Placements.Num() > MaxDetailInstances)
						{
							Result.Error = TEXT("Voxel detail plan exceeds the 8192 instance limit");
							return Result;
						}
					}
				}
			}
		}
		Plan->Placements.Sort([](
			const FVoxelStructureDetailPlacement& InA,
			const FVoxelStructureDetailPlacement& InB)
		{
			if (InA.OwnerId != InB.OwnerId) return InA.OwnerId < InB.OwnerId;
			if (InA.DetailId != InB.DetailId) return InA.DetailId.LexicalLess(InB.DetailId);
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
		InResult.Stamp.Token != WaitingToken)
	{
		return true;
	}
	bRunning = false;
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
	PendingPlan = MoveTemp(InResult.Details);
	return true;
}

void FVoxelDetailView::Reset()
{
	bStopped = true;
	++WaitingToken;
	bRunning = false;
	AbortStaging();
	DestroyActors(ActiveActors);
	Assets.Reset();
	Meshes.Reset();
	AssetIndices.Reset();
	LastObserverSections.Reset();
	Observers.Reset();
}

bool FVoxelDetailView::BuildAssetBatch(const int32 InAssetIndex, FString& OutError)
{
	if (!PendingPlan || !Assets.IsValidIndex(InAssetIndex) || !Meshes.IsValidIndex(InAssetIndex))
	{
		OutError = TEXT("Voxel detail staging state is invalid");
		return false;
	}
	UVoxelDetailData* Asset = Assets[InAssetIndex].Get();
	UWorld* World = Module.GetWorld();
	if (!Asset || !World)
	{
		OutError = TEXT("Voxel detail asset or world is unavailable");
		return false;
	}

	TArray<const FVoxelStructureDetailPlacement*> Selected;
	for (const FVoxelStructureDetailPlacement& Placement : PendingPlan->Placements)
	{
		if (Placement.DetailId == Asset->StableKey)
		{
			Selected.Add(&Placement);
		}
	}
	if (Selected.IsEmpty())
	{
		return true;
	}
	if (Selected.Num() > MaxInstancesPerAsset)
	{
		OutError = TEXT("Voxel detail asset exceeds the 2048 instance batch limit");
		return false;
	}

	const double CellSize = Module.BlockSize();
	const FVector Anchor = FVector(Selected[0]->Position) * CellSize;
	TArray<FTransform> Transforms;
	TArray<FBox> CollisionBoxes;
	for (const FVoxelStructureDetailPlacement* Placement : Selected)
	{
		FVoxelBlockState Support;
		const FIntVector SupportCell = Placement->Position - FIntVector(0, 0, 1);
		if (Module.GetRuntime()->TryGetBlock(SupportCell, Support))
		{
			const FVoxelRuntimeDefinition* Definition = Module.GetRegistry()->Find(Support.TypeId);
			if (!Definition || !Definition->bSolid)
			{
				continue;
			}
		}
		const FVector WorldLocation = FVector(Placement->Position) * CellSize;
		const EDetailPresentationBand Band = ResolveDetailBand(WorldLocation, Observers);
		if (Band == EDetailPresentationBand::Far)
		{
			continue;
		}
		const FTransform Transform(
			FRotator(0.0, static_cast<double>(Placement->Yaw) * 90.0, 0.0),
			WorldLocation - Anchor);
		Transforms.Add(Transform);
		if (Asset->bNearCollision && Band == EDetailPresentationBand::Near)
		{
			for (const FBox& Box : Asset->CollisionBoxesCm)
			{
				CollisionBoxes.Add(Box.TransformBy(Transform));
			}
		}
	}
	if (Transforms.IsEmpty())
	{
		return true;
	}

	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Anchor), Parameters);
	if (!Host)
	{
		OutError = TEXT("Voxel detail presenter actor allocation failed");
		return false;
	}
	Host->SetReplicates(false);
	Host->SetActorHiddenInGame(true);
	Host->SetActorEnableCollision(false);
	StagedActors.Add(Host);
	USceneComponent* Root = NewObject<USceneComponent>(Host);
	Host->SetRootComponent(Root);
	Root->RegisterComponent();

	if (UStaticMesh* Mesh = Meshes[InAssetIndex].Get())
	{
		UHierarchicalInstancedStaticMeshComponent* Instances =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(Host);
		Instances->SetupAttachment(Root);
		Instances->SetStaticMesh(Mesh);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetGenerateOverlapEvents(false);
		Instances->SetCanEverAffectNavigation(false);
		Instances->SetCullDistances(0, FMath::RoundToInt(DetailMiddleRadiusCm));
		Instances->RegisterComponent();
		Instances->AddInstances(Transforms, false, false, false);
	}
	if (!CollisionBoxes.IsEmpty())
	{
		UVoxelCollisionComponent* Collision = NewObject<UVoxelCollisionComponent>(Host);
		Collision->SetupAttachment(Root);
		Collision->RegisterComponent();
		if (!Collision->Apply(CollisionBoxes, 1.0))
		{
			OutError = TEXT("Voxel detail simple collision build failed");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

void FVoxelDetailView::DestroyActors(TArray<TWeakObjectPtr<AActor>>& InActors)
{
	for (const TWeakObjectPtr<AActor>& Actor : InActors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	InActors.Reset();
}

void FVoxelDetailView::AbortStaging()
{
	DestroyActors(StagedActors);
	PendingPlan.Reset();
	StagedAssetIndex = 0;
}

void FVoxelDetailView::MarkDirty(const FVoxelEditBatch& InBatch)
{
	if (!InBatch.Sections.IsEmpty())
	{
		bDirty = true;
	}
}
