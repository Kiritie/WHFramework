#include "Task/TaskObjectiveTypes.h"
#include "Task/TaskPresentationTypes.h"
#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/EventModule.h"
#include "Event/Events/Task/Event_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"

UTaskAsset* UTaskModule::GetAsset(UTaskAsset* InAsset) const
{
	if(Assets.Contains(InAsset)) return InAsset;
	return GetAssetByInstance(InAsset, FGuid());
}

UTaskAsset* UTaskModule::GetAssetByInstance(UTaskAsset* InAsset, FGuid InInstanceID) const
{
	if(!InAsset) return nullptr;
	const UObject* Source = InAsset->SourceObject ? InAsset->SourceObject : InAsset;
	for(UTaskAsset* Asset : Assets)
	{
		if(Asset && (Asset == InAsset || Asset->SourceObject == Source) && Asset->InstanceID == InInstanceID) return Asset;
	}
	return nullptr;
}

TArray<UTaskAsset*> UTaskModule::GetAssetsByAgent(FGuid InAgentID) const
{
	TArray<UTaskAsset*> Result;
	if(!InAgentID.IsValid()) return Result;
	for(UTaskAsset* Asset : Assets)
	{
		if(Asset && Asset->AgentID == InAgentID) Result.Add(Asset);
	}
	return Result;
}

UTaskBase* UTaskModule::ResolveRuntimeTask(UTaskBase* Task) const
{
	if (!Task) return nullptr;
	UTaskAsset* TaskAsset = Task->GetTaskAsset();
	if(Assets.Contains(TaskAsset)) return Task;
	if (UTaskAsset* Asset = GetAsset(TaskAsset)) return Asset->TaskMap.FindRef(Task->TaskGUID);
	return nullptr;
}

UTaskAsset* UTaskModule::AddAsset(UTaskAsset* InAsset)
{
	return AddAssetInternal(InAsset, FGuid(), FGuid());
}

UTaskAsset* UTaskModule::CreateAsset(UTaskAsset* InAsset, FGuid InAgentID, FGuid InOwnerActorID)
{
	if(!InAsset || !InAgentID.IsValid()) return nullptr;
	FGuid InstanceID = FGuid::NewGuid();
	while(GetAssetByInstance(InAsset, InstanceID)) InstanceID = FGuid::NewGuid();
	return AddAssetInternal(InAsset, InstanceID, InAgentID, InOwnerActorID);
}

UTaskAsset* UTaskModule::AddAssetInternal(UTaskAsset* InAsset, FGuid InInstanceID, FGuid InAgentID, FGuid InOwnerActorID, bool bPresentationOnly)
{
	if(bClearingRuntimeAssets || !InAsset) return nullptr;
	UTaskAsset* Source = Cast<UTaskAsset>(InAsset->SourceObject ? InAsset->SourceObject : InAsset);
	if(!Source) return nullptr;
	if(bDefaultAssetsPerOwner && !InOwnerActorID.IsValid() && !bPresentationOnly && DefaultAssets.Contains(Source)) return nullptr;
	if(UTaskAsset* Existing = GetAssetByInstance(Source, InInstanceID)) return Existing;
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	TArray<FText> Errors;
	if(!Source->ValidateTasks(Errors))
	{
		for(const FText& Error : Errors)
		{
			UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
		}
		return nullptr;
	}
	UTaskAsset* RuntimeAsset = DuplicateObject<UTaskAsset>(Source, this);
	if (!RuntimeAsset) return nullptr;
	RuntimeAsset->SourceObject = Source;
	RuntimeAsset->InstanceID = InInstanceID;
	RuntimeAsset->AgentID = InAgentID;
	RuntimeAsset->OwnerActorID = InOwnerActorID;
	RuntimeAsset->bPresentationOnly = bPresentationOnly;
	TArray<FText> ExpansionErrors;
	if(!RuntimeAsset->CompileTasks(ExpansionErrors) || !RuntimeAsset->ExpandTaskAssetReferences(ExpansionErrors))
	{
		for(const FText& Error : ExpansionErrors) UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString());
		return nullptr;
	}
	Assets.Add(RuntimeAsset);
	if(!RuntimeAsset->InitializeRuntimeTasks())
	{
		Assets.Remove(RuntimeAsset);
		return nullptr;
	}
	if(InInstanceID.IsValid())
	{
		for(const auto& Pair : RuntimeAsset->TaskMap)
		{
			UTaskBase* Task = Pair.Value;
			if(!Task) continue;
			for(FTaskObjective& Objective : Task->Objectives)
			{
				if(Objective.RequiredCountRange == FIntPoint::ZeroValue) continue;
				const uint32 Seed = HashCombineFast(GetTypeHash(InInstanceID), HashCombineFast(GetTypeHash(Pair.Key), GetTypeHash(Objective.ObjectiveID)));
				Objective.RequiredCount = FRandomStream(Seed).RandRange(Objective.RequiredCountRange.X, Objective.RequiredCountRange.Y);
			}
		}
	}
	RequestTaskMarkersRefresh();
	return RuntimeAsset;
}

UTaskAsset* UTaskModule::PrepareRuntimeAsset(UTaskAsset* InAsset, FGuid InInstanceID, FGuid InAgentID, FGuid InOwnerActorID, FString& OutError)
{
	if(!IsInGameThread() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client || bLoadingTasks || bClearingRuntimeAssets ||
		!InAsset || !InInstanceID.IsValid() || !InAgentID.IsValid() || !InOwnerActorID.IsValid() || PreparedRuntimeAssets.Num() >= 64)
	{
		OutError = TEXT("Task.PreparedAssetUnavailable");
		return nullptr;
	}
	UTaskAsset* Source = Cast<UTaskAsset>(InAsset->SourceObject ? InAsset->SourceObject : InAsset);
	if(!Source || GetAssetByInstance(Source, InInstanceID))
	{
		OutError = TEXT("Task.PreparedInstanceAlreadyExists");
		return nullptr;
	}
	TArray<FText> Errors;
	if(!Source->ValidateTasks(Errors))
	{
		OutError = Errors.IsEmpty() ? TEXT("Task.InvalidTemplate") : Errors[0].ToString();
		return nullptr;
	}
	UTaskAsset* Candidate = DuplicateObject<UTaskAsset>(Source, this);
	if(!Candidate)
	{
		OutError = TEXT("Task.CannotCloneTemplate");
		return nullptr;
	}
	Candidate->SourceObject = Source;
	Candidate->InstanceID = InInstanceID;
	Candidate->AgentID = InAgentID;
	Candidate->OwnerActorID = InOwnerActorID;
	Candidate->bPresentationOnly = true;
	if(!Candidate->CompileTasks(Errors) || !Candidate->ExpandTaskAssetReferences(Errors) || !Candidate->RebuildTaskMap(false) ||
		!Candidate->ValidateTasks(Errors) || Candidate->TaskMap.Num() > 256 || Candidate->RootTasks.IsEmpty())
	{
		OutError = Errors.IsEmpty() ? TEXT("Task.InvalidRuntimeHierarchy") : Errors[0].ToString();
		return nullptr;
	}
	for(const auto& Pair : Candidate->TaskMap)
	{
		UTaskBase* Task = Pair.Value;
		FTaskPresentationState Initial;
		Initial.TaskGUID = Pair.Key;
		Initial.Target = Task->Target;
		Task->ApplyPresentation(Initial);
		for(FTaskObjective& Objective : Task->Objectives)
		{
			if(Objective.RequiredCountRange == FIntPoint::ZeroValue) continue;
			const FString Key = InInstanceID.ToString(EGuidFormats::Digits) + TEXT("/") + Pair.Key + TEXT("/") + Objective.ObjectiveID.ToString();
			Objective.RequiredCount = FRandomStream(FCrc::StrCrc32(*Key)).RandRange(Objective.RequiredCountRange.X, Objective.RequiredCountRange.Y);
		}
	}
	Assets.Reserve(Assets.Num() + PreparedRuntimeAssets.Num() + 1);
	PreparedRuntimeAssets.Add(Candidate);
	OutError.Reset();
	return Candidate;
}

bool UTaskModule::InstallPreparedRuntimeAsset(UTaskAsset* InAsset, FString& OutError)
{
	if(!IsInGameThread() || bLoadingTasks || bClearingRuntimeAssets || !InAsset || InAsset->GetOuter() != this || !InAsset->SourceObject ||
		!InAsset->InstanceID.IsValid() || !InAsset->OwnerActorID.IsValid())
	{
		OutError = TEXT("Task.PreparedAssetIdentityChanged");
		return false;
	}
	if(Assets.Contains(InAsset)) return true;
	if(!PreparedRuntimeAssets.Contains(InAsset) || GetAssetByInstance(Cast<UTaskAsset>(InAsset->SourceObject), InAsset->InstanceID))
	{
		OutError = TEXT("Task.PreparedAssetMissingOrReplaced");
		return false;
	}
	Assets.Add(InAsset);
	PreparedRuntimeAssets.Remove(InAsset);
	OutError.Reset();
	return true;
}

void UTaskModule::CancelPreparedRuntimeAsset(UTaskAsset* InAsset)
{
	check(IsInGameThread());
	PreparedRuntimeAssets.Remove(InAsset);
}

bool UTaskModule::EnableOwnerDefaultAssets(FString& OutError)
{
	if(!IsInGameThread() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client || bLoadingTasks)
	{
		OutError = TEXT("Task.OwnerDefaultsUnavailable");
		return false;
	}
	bDefaultAssetsPerOwner = true;
	const TArray<UTaskAsset*> Existing = Assets;
	for(UTaskAsset* Asset : Existing)
	{
		if(Asset && !Asset->OwnerActorID.IsValid() && DefaultAssets.Contains(Cast<UTaskAsset>(Asset->SourceObject)))
		{
			Asset->bPresentationOnly = true;
			RemoveAsset(Asset);
		}
	}
	OutError.Reset();
	return true;
}

FGuid UTaskModule::MakeOwnerDefaultInstanceID(const FGuid& InWorldID, const FGuid& InOwnerActorID, const FString& InTemplatePath)
{
	if(!InWorldID.IsValid() || !InOwnerActorID.IsValid() || InTemplatePath.IsEmpty()) return FGuid();
	const FString Key = TEXT("OwnerDefaultTask/") + InWorldID.ToString(EGuidFormats::Digits) + TEXT("/") +
		InOwnerActorID.ToString(EGuidFormats::Digits) + TEXT("/") + InTemplatePath;
	FGuid Result;
	FGuid::ParseExact(FMD5::HashAnsiString(*Key), EGuidFormats::Digits, Result);
	return Result;
}

bool UTaskModule::PrepareOwnerDefaultAssets(const FGuid& InWorldID, const FGuid& InOwnerActorID, TArray<UTaskAsset*>& OutAssets, TSet<FGuid>& OutCreated, FString& OutError)
{
	OutAssets.Reset();
	OutCreated.Reset();
	if(!bDefaultAssetsPerOwner || !InWorldID.IsValid() || !InOwnerActorID.IsValid() || bLoadingTasks ||
		!GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Task.OwnerDefaultsNotReady");
		return false;
	}
	TSet<UTaskAsset*> Seen;
	for(UTaskAsset* Definition : DefaultAssets)
	{
		UTaskAsset* Source = Definition ? Cast<UTaskAsset>(Definition->SourceObject ? Definition->SourceObject : Definition) : nullptr;
		if(!Source || Seen.Contains(Source))
		{
			OutError = TEXT("Task.OwnerDefaultTemplateInvalid");
			return false;
		}
		Seen.Add(Source);
		const FGuid InstanceID = MakeOwnerDefaultInstanceID(InWorldID, InOwnerActorID, Source->GetPathName());
		UTaskAsset* Asset = GetAssetByInstance(Source, InstanceID);
		if(Asset && Asset->OwnerActorID != InOwnerActorID)
		{
			OutError = TEXT("Task.OwnerDefaultIdentityChanged");
			return false;
		}
		if(!Asset)
		{
			Asset = PrepareRuntimeAsset(Source, InstanceID, InOwnerActorID, InOwnerActorID, OutError);
			if(!Asset) return false;
			if(!InstallPreparedRuntimeAsset(Asset, OutError))
			{
				CancelPreparedRuntimeAsset(Asset);
				return false;
			}
			OutCreated.Add(InstanceID);
		}
		OutAssets.Add(Asset);
	}
	OutError.Reset();
	return true;
}

void UTaskModule::RemoveAsset(UTaskAsset* InAsset)
{
    if (bClearingRuntimeAssets) return;
    UTaskAsset* Asset = GetAsset(InAsset);
    if (!Asset) return;
    BeginTaskMutation();
    ON_SCOPE_EXIT { EndTaskMutation(); };
    for (const auto& Pair : Asset->TaskMap)
    {
        PendingResume.Remove(Pair.Value);
        if (Pair.Value) Pair.Value->OnSuspend();
    }
    TArray<FGuid> TrackedOwners;
    for (const auto& Pair : CurrentTasks) if (Pair.Value && Pair.Value->GetTaskAsset() == Asset) TrackedOwners.Add(Pair.Key);
    Assets.Remove(Asset);
    for (const FGuid& Owner : TrackedOwners) CurrentTasks.Remove(Owner);
    RequestTaskMarkersRefresh();
    if (UEventModule* Events = UEventModule::GetPtr(GetWorld()))
        for (const FGuid& Owner : TrackedOwners) Events->BroadcastEvent<FEventCurrentTaskChanged>(this, {Owner, nullptr});
}
