#include "Voxel/Scene/VoxelSceneRegion.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "GameFramework/Pawn.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/VoxelModule.h"

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
}

void UVoxelSceneRegion::TickSceneActors(float InDeltaSeconds)
{
	(void)InDeltaSeconds;
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

	TArray<AActor*> Created;
	for (const FVoxelSavedSceneActor& Record : Records)
	{
		if (Module->FindSceneActor(Record.Id))
		{
			OutError = TEXT("Duplicate live voxel scene actor GUID");
			for (AActor* Actor : Created)
			{
				Actor->Destroy();
			}
			return false;
		}

		const UScriptStruct* Type = Record.Data.GetStructType();
		AActor* Actor = Type && Type->IsChildOf(FPickUpSaveData::StaticStruct())
			? static_cast<AActor*>(UAbilityModuleStatics::SpawnAbilityPickUp(Record.Data, this))
			: UAbilityModuleStatics::SpawnAbilityActor(Record.Data, this);
		if (Actor && Actor->Implements<USceneActorInterface>())
		{
			RemoveSceneActor(Actor);
			ISceneActorInterface::Execute_SetActorID(Actor, Record.Id.ToString());
		}
		if (!Actor ||
			!Actor->Implements<USceneActorInterface>() ||
			ISceneActorInterface::Execute_GetActorID(Actor) != Record.Id)
		{
			if (Actor)
			{
				Actor->Destroy();
			}
			for (AActor* CreatedActor : Created)
			{
				CreatedActor->Destroy();
			}
			OutError = TEXT("Voxel scene actor factory failed");
			return false;
		}
		Actor->SetReplicates(true);
		Created.Add(Actor);
		AddSceneActor(Actor);
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
