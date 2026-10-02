#include "Voxel/Scene/VoxelSceneRegion.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "GameFramework/Pawn.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

UWorld* UVoxelSceneRegion::GetWorld() const
{
	return Module ? Module->GetWorld() : nullptr;
}

void UVoxelSceneRegion::Initialize(
	UVoxelModule* InModule,
	const FIntVector& InRegionKey)
{
	check(IsInGameThread());
	check(InModule);
	check(!Module);
	Module = InModule;
	RegionKey = InRegionKey;
}

void UVoxelSceneRegion::Shutdown()
{
	DestroySceneActors();
	ActiveSections.Reset();
	PendingActors.Reset();
	ResidencyRefreshSeconds = 0.0f;
	Module = nullptr;
	bSceneReady = false;
	bSceneLoading = false;
	bSceneFailed = false;
}

void UVoxelSceneRegion::OnSectionActivated(const FIntVector& InSection)
{
	ActiveSections.Add(InSection);
}

void UVoxelSceneRegion::OnSectionDeactivated(const FIntVector& InSection)
{
	ActiveSections.Remove(InSection);
	FString Error;
	if (!CaptureInactiveActors(Error))
	{
		bSceneFailed = true;
		UE_LOG(LogTemp, Error, TEXT("Simulation actor capture failed: %s"), *Error);
	}
}

void UVoxelSceneRegion::TickSceneActors(float InDeltaSeconds)
{
	if (!Module || !Module->IsAuthority() || !bSceneReady || bSceneFailed)
	{
		return;
	}
	ResidencyRefreshSeconds += InDeltaSeconds;
	if (ResidencyRefreshSeconds < 0.25f)
	{
		return;
	}
	ResidencyRefreshSeconds = 0.0f;
	FString Error;
	if (!CaptureInactiveActors(Error))
	{
		bSceneFailed = true;
		UE_LOG(LogTemp, Error, TEXT("Simulation actor capture failed: %s"), *Error);
		return;
	}
	int32 Remaining = 2;
	TArray<FGuid> PendingIds;
	PendingActors.GenerateKeyArray(PendingIds);
	int32 Attempts = FMath::Min(16, PendingIds.Num());
	while (Remaining > 0 && Attempts-- > 0)
	{
		PendingActorCursor %= PendingIds.Num();
		const FGuid Id = PendingIds[PendingActorCursor++];
		const FParameter* Data = PendingActors.Find(Id);
		if (Data && RestorePendingActor(Id, *Data))
		{
			PendingActors.Remove(Id);
			--Remaining;
		}
	}
}

bool UVoxelSceneRegion::CaptureInactiveActors(FString& OutError)
{
	if (!Module || !Module->IsAuthority())
	{
		return true;
	}
	TArray<AActor*> Retiring;
	TArray<TPair<AActor*, UVoxelSceneRegion*>> Migrating;
	for (const auto& Pair : SceneActorMap)
	{
		AActor* Actor = Pair.Value;
		if (!ShouldPersistActor(Actor))
		{
			continue;
		}
		const FVector Cell = Actor->GetActorLocation() / Module->BlockSize();
		const FVoxelSectionKey Section = VoxelCoord::Section(FIntVector(FMath::FloorToInt(Cell.X), FMath::FloorToInt(Cell.Y), FMath::FloorToInt(Cell.Z)));
		const FIntVector SectionCoordinate(Section.X, Section.Y, Section.Z);
		UVoxelSceneRegion* Destination = Module->GetSceneRegionForActor(Actor, SectionCoordinate);
		if (!Destination)
		{
			OutError = TEXT("Scene actor cannot obtain its destination persistence region");
			return false;
		}
		if (Destination != this)
		{
			Migrating.Add({Actor, Destination});
			continue;
		}
		if (Module->IsSimulationResident(FIntVector(Section.X, Section.Y, Section.Z)))
		{
			continue;
		}
		ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor);
		if (!Agent)
		{
			OutError = TEXT("Scene actor has no save data owner");
			return false;
		}
		PendingActors.Add(Pair.Key, Agent->GetSaveData(true));
		Retiring.Add(Actor);
	}
	for (const auto& Transfer : Migrating)
	{
		if (!Transfer.Value->AddSceneActor(Transfer.Key))
		{
			OutError = TEXT("Scene actor region transfer rejected");
			return false;
		}
		RemoveSceneActor(Transfer.Key);
	}
	for (AActor* Actor : Retiring)
	{
		RemoveSceneActor(Actor);
		Actor->Destroy();
	}
	return true;
}

bool UVoxelSceneRegion::RestorePendingActor(const FGuid& InId, const FParameter& InData)
{
	FVector Location;
	const FPickUpSaveData* Pickup = InData.GetPtr<FPickUpSaveData>();
	const FSceneActorSaveData* SceneActor = InData.GetPtr<FSceneActorSaveData>();
	if (Pickup)
	{
		Location = Pickup->Location;
	}
	else if (SceneActor)
	{
		Location = SceneActor->SpawnTransform.GetLocation();
	}
	else
	{
		bSceneFailed = true;
		return false;
	}
	const FVector Cell = Location / Module->BlockSize();
	const FVoxelSectionKey Key = VoxelCoord::Section(FIntVector(FMath::FloorToInt(Cell.X), FMath::FloorToInt(Cell.Y), FMath::FloorToInt(Cell.Z)));
	const FIntVector Section(Key.X, Key.Y, Key.Z);
	if (!Module->IsSimulationResident(Section) || !Module->IsCollisionReady(Section) ||
		(InData.GetPtr<FPawnSaveData>() && !Module->IsNavigationReady(Section)))
	{
		return false;
	}
	if (Module->FindSceneActor(InId))
	{
		bSceneFailed = true;
		return false;
	}
	AActor* Actor = Pickup ? static_cast<AActor*>(UAbilityModuleStatics::SpawnAbilityPickUp(InData, this))
		: UAbilityModuleStatics::SpawnAbilityActor(InData, this);
	if (!Actor || !Actor->Implements<USceneActorInterface>())
	{
		if (Actor)
		{
			Actor->Destroy();
		}
		bSceneFailed = true;
		return false;
	}
	RemoveSceneActor(Actor);
	ISceneActorInterface::Execute_SetActorID(Actor, InId.ToString());
	Actor->SetReplicates(true);
	if (!AddSceneActor(Actor))
	{
		Actor->Destroy();
		return false;
	}
	return true;
}

bool UVoxelSceneRegion::CaptureProjectData(FParameter& OutData) const
{
	OutData = FParameter();
	return true;
}

bool UVoxelSceneRegion::RestoreProjectData(
	const FParameter& InData,
	FString& OutError)
{
	if (InData.HasValue())
	{
		OutError = TEXT("Unexpected voxel scene region project data");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UVoxelSceneRegion::ShouldPersistActor(AActor* InActor) const
{
	return IsValid(InActor) &&
		!InActor->IsActorBeingDestroyed() &&
		!(Cast<APawn>(InActor) && CastChecked<APawn>(InActor)->IsPlayerControlled());
}

bool UVoxelSceneRegion::CaptureActors(
	TArray<uint8>& OutBytes,
	FString& OutError) const
{
	TArray<FVoxelSavedSceneActor> Actors;
	for (const auto& Pair : PendingActors)
	{
		Actors.Add({Pair.Key, Pair.Value});
	}
	for (const TPair<FGuid, TObjectPtr<AActor>>& Pair : SceneActorMap)
	{
		if (!ShouldPersistActor(Pair.Value))
		{
			continue;
		}
		ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Pair.Value);
		if (!Agent)
		{
			OutError = TEXT("Voxel scene actor does not implement SaveDataAgent");
			return false;
		}
		FVoxelSavedSceneActor Saved;
		Saved.Id = Pair.Key;
		Saved.Data = Agent->GetSaveData(true);
		Actors.Add(MoveTemp(Saved));
	}

	FParameter ProjectData;
	if (!CaptureProjectData(ProjectData))
	{
		OutError = TEXT("Voxel scene region project data capture failed");
		return false;
	}
	return FVoxelSceneColumnCodec::Encode(Actors, ProjectData, OutBytes, OutError);
}

bool UVoxelSceneRegion::RestoreActors(
	TConstArrayView<uint8> InBytes,
	FString& OutError)
{
	if (!Module || !Module->IsAuthority() || bSceneReady)
	{
		OutError = TEXT("Voxel scene region is not ready for restore");
		return false;
	}

	TArray<FVoxelSavedSceneActor> Records;
	FParameter ProjectData;
	if (!FVoxelSceneColumnCodec::Decode(InBytes, Records, ProjectData, OutError) ||
		!RestoreProjectData(ProjectData, OutError))
	{
		return false;
	}

	for (const FVoxelSavedSceneActor& Record : Records)
	{
		if (Module->FindSceneActor(Record.Id) || PendingActors.Contains(Record.Id))
		{
			OutError = TEXT("Duplicate stored voxel scene actor GUID");
			return false;
		}
		PendingActors.Add(Record.Id, Record.Data);
	}

	bSceneReady = true;
	bSceneLoading = false;
	OutError.Reset();
	return true;
}

bool UVoxelSceneRegion::HasSceneActor(
	const FString& InId,
	bool bInEnsured) const
{
	return GetSceneActor(InId, nullptr, bInEnsured) != nullptr;
}

AActor* UVoxelSceneRegion::GetSceneActor(
	const FString& InId,
	TSubclassOf<AActor> InClass,
	bool bInEnsured) const
{
	(void)bInEnsured;
	FGuid Id;
	if (!FGuid::Parse(InId, Id))
	{
		return nullptr;
	}
	AActor* Actor = SceneActorMap.FindRef(Id);
	return IsValid(Actor) && (!InClass || Actor->IsA(InClass)) ? Actor : nullptr;
}

bool UVoxelSceneRegion::AddSceneActor(AActor* InActor)
{
	if (!IsValid(InActor) ||
		!Module ||
		!Module->IsAuthority() ||
		!InActor->Implements<USceneActorInterface>())
	{
		return false;
	}
	const FGuid Id = ISceneActorInterface::Execute_GetActorID(InActor);
	if (!Id.IsValid())
	{
		return false;
	}
	if (AActor* Existing = Module->FindSceneActor(Id))
	{
		if (Existing != InActor)
		{
			return false;
		}
	}
	SceneActorMap.Add(Id, InActor);
	ISceneActorInterface::Execute_SetContainer(InActor, TScriptInterface<ISceneContainerInterface>(this));
	return true;
}

bool UVoxelSceneRegion::RemoveSceneActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}
	for (auto Iterator = SceneActorMap.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value() == InActor)
		{
			Iterator.RemoveCurrent();
			return true;
		}
	}
	return false;
}

void UVoxelSceneRegion::DestroySceneActors()
{
	TArray<TObjectPtr<AActor>> Actors;
	SceneActorMap.GenerateValueArray(Actors);
	SceneActorMap.Reset();
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor) &&
			!(Cast<APawn>(Actor) && CastChecked<APawn>(Actor)->IsPlayerControlled()))
		{
			Actor->Destroy();
		}
	}
}

const FIntVector& UVoxelSceneRegion::GetRegionKey() const
{
	return RegionKey;
}

const TMap<FGuid, TObjectPtr<AActor>>& UVoxelSceneRegion::GetSceneActors() const
{
	return SceneActorMap;
}

bool UVoxelSceneRegion::HasActiveSections() const
{
	return !ActiveSections.IsEmpty();
}

bool UVoxelSceneRegion::HasStoredActor(const FGuid& InId) const
{
	return SceneActorMap.Contains(InId) || PendingActors.Contains(InId);
}

const TMap<FGuid, FParameter>& UVoxelSceneRegion::GetPendingActors() const
{
	return PendingActors;
}
