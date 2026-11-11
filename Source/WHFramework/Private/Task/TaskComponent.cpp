#include "Task/TaskComponent.h"
#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Scene/SceneMarkerTypes.h"

#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/TaskModule.h"

UTaskComponent::UTaskComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTaskComponent::BeginPlay()
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	Super::BeginPlay();
	ResolveAgentID();
	ResolveTaskAssets();
	if(TaskModule != nullptr) TaskModule->OnTaskAssetsChanged.AddUniqueDynamic(this, &UTaskComponent::OnTaskAssetsChanged);
	RefreshTaskMarker();
}

void UTaskComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	USceneModule* SceneModule = USceneModule::GetPtr(GetWorld());
	if(TaskModule != nullptr)
	{
		TaskModule->OnTaskAssetsChanged.RemoveDynamic(this, &UTaskComponent::OnTaskAssetsChanged);
		for(UTaskAsset* Asset : GetRuntimeAssets())
		{
			if(Asset && GetOwner()) Asset->AgentLocation = GetOwner()->GetActorLocation();
		}
	}
	if(SceneModule != nullptr && MarkerID.IsValid()) SceneModule->RemoveMarker(MarkerID);
	ResolvedTaskAssets.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UTaskComponent::ResolveAgentID()
{
	AActor* Owner = GetOwner();
	if(!Owner || !Owner->Implements<USceneActorInterface>()) return false;
	AgentID = ISceneActorInterface::Execute_GetActorID(Owner);
	if(AgentID.IsValid())
	{
		MarkerID = FGuid(AgentID.A ^ 0x54A54A31u, AgentID.B ^ 0x4D41524Bu, AgentID.C ^ 0x45524F46u, AgentID.D ^ 0x46455231u);
		return true;
	}
	return false;
}

void UTaskComponent::SetTaskAssets(const TArray<TSoftObjectPtr<UTaskAsset>>& InAssets)
{
	TaskAssets.Reset();
	for(const TSoftObjectPtr<UTaskAsset>& Asset : InAssets)
	{
		if(!Asset.IsNull()) TaskAssets.AddUnique(Asset);
	}
	ResolveTaskAssets();
	RefreshTaskMarker();
	if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
}

TArray<UTaskAsset*> UTaskComponent::GetRuntimeAssets() const
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	return TaskModule != nullptr && AgentID.IsValid() ? TaskModule->GetAssetsByAgent(AgentID) : TArray<UTaskAsset*>();
}

bool UTaskComponent::HasRuntimeAsset(UTaskAsset* InSource, FGuid InPlayerId) const
{
	if(!InSource || !InPlayerId.IsValid()) return false;
	const FSoftObjectPath SourcePath(InSource);
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(Asset && Asset->OwnerActorID == InPlayerId && Asset->SourceObject && FSoftObjectPath(Asset->SourceObject) == SourcePath) return true;
	}
	return false;
}

TArray<UTaskAsset*> UTaskComponent::GetOfferedAssets(AActor* InInteractor) const
{
	TArray<UTaskAsset*> Result;
	const FGuid PlayerId = InInteractor && InInteractor->GetWorld() == GetWorld() && InInteractor->Implements<USceneActorInterface>() ? ISceneActorInterface::Execute_GetActorID(InInteractor) : FGuid();
	if (!PlayerId.IsValid()) return Result;
	for(UTaskAsset* Asset : ResolvedTaskAssets)
	{
		if(Asset && !HasRuntimeAsset(Asset, PlayerId)) Result.Add(Asset);
	}
	return Result;
}

void UTaskComponent::ResolveTaskAssets()
{
	ResolvedTaskAssets.Reset();
	for(const TSoftObjectPtr<UTaskAsset>& AssetPtr : TaskAssets)
	{
		if(UTaskAsset* Asset = AssetPtr.LoadSynchronous())
		{
			ResolvedTaskAssets.AddUnique(Asset);
		}
	}
}

UTaskBase* UTaskComponent::GetTask(AActor* InInteractor, ETaskStage InStage) const
{
	const FGuid PlayerId = InInteractor && InInteractor->GetWorld() == GetWorld() && InInteractor->Implements<USceneActorInterface>() ? ISceneActorInterface::Execute_GetActorID(InInteractor) : FGuid();
	if (!PlayerId.IsValid()) return nullptr;
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(!Asset || Asset->OwnerActorID != PlayerId) continue;
		for(UTaskBase* Task : Asset->RootTasks)
		{
			if(Task && Task->GetTaskStage() == InStage) return Task;
		}
	}
	return nullptr;
}

UTaskBase* UTaskComponent::ResolveTask(AActor* InInteractor, const FTaskReference& InReference) const
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	const FGuid PlayerId = InInteractor && InInteractor->GetWorld() == GetWorld() && InInteractor->Implements<USceneActorInterface>() ? ISceneActorInterface::Execute_GetActorID(InInteractor) : FGuid();
	if(TaskModule == nullptr || !PlayerId.IsValid()) return nullptr;
	if(InReference.InstanceID.IsValid())
	{
		UTaskBase* Task = TaskModule->ResolveTask(InReference);
		const UTaskAsset* Asset = Task ? Task->GetTaskAsset() : nullptr;
		return Asset && Asset->OwnerActorID == PlayerId && Asset->AgentID == AgentID ? Task : nullptr;
	}
	UTaskBase* Found = nullptr;
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(!Asset || Asset->OwnerActorID != PlayerId || !Asset->SourceObject) continue;
		if(!InReference.Asset.IsNull() && FSoftObjectPath(Asset->SourceObject) != InReference.Asset.ToSoftObjectPath()) continue;
		UTaskBase* Task = InReference.TaskGUID.IsEmpty() ? (Asset->RootTasks.IsEmpty() ? nullptr : Asset->RootTasks[0]) : Asset->TaskMap.FindRef(InReference.TaskGUID);
		if (Task && Found && Found != Task) return nullptr;
		if (Task) Found = Task;
	}
	return Found;
}

UTaskBase* UTaskComponent::AcceptTask(UTaskAsset* InAsset, FString InTaskGUID, AActor* InInteractor)
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	if(!bAgentActive || TaskModule == nullptr || (!AgentID.IsValid() && !ResolveAgentID())) return nullptr;
	FGuid OwnerActorID;
	if(InInteractor)
	{
		if(InInteractor->GetWorld() != GetWorld() || !InInteractor->HasAuthority() ||
			!InInteractor->Implements<USceneActorInterface>()) return nullptr;
		OwnerActorID = ISceneActorInterface::Execute_GetActorID(InInteractor);
		if(!OwnerActorID.IsValid()) return nullptr;
	}
	if (!OwnerActorID.IsValid()) return nullptr;
	if(!InAsset)
	{
		const TArray<UTaskAsset*> Offers = GetOfferedAssets(InInteractor);
		InAsset = Offers.IsEmpty() ? nullptr : Offers[0];
	}
	if(!InAsset || HasRuntimeAsset(InAsset, OwnerActorID)) return nullptr;
	UTaskAsset* RuntimeAsset = TaskModule->CreateAsset(InAsset, AgentID, OwnerActorID);
	if(!RuntimeAsset) return nullptr;
	RuntimeAsset->AgentLocation = GetOwner()->GetActorLocation();
	UTaskBase* Task = InTaskGUID.IsEmpty() ? (RuntimeAsset->RootTasks.IsEmpty() ? nullptr : RuntimeAsset->RootTasks[0]) : RuntimeAsset->TaskMap.FindRef(InTaskGUID);
	if(!Task)
	{
		TaskModule->RemoveAsset(RuntimeAsset);
		return nullptr;
	}
	TaskModule->EnterTask(Task, true);
	if(Task->GetTaskState() == ETaskState::None)
	{
		TaskModule->RemoveAsset(RuntimeAsset);
		return nullptr;
	}
	OnTaskAccepted.Broadcast(Task);
	RefreshTaskMarker();
	return Task;
}

bool UTaskComponent::TurnInTask(AActor* InInteractor, UTaskBase* InTask)
{
	UTaskModule* TaskModule = UTaskModule::GetPtr(GetWorld());
	if(!bAgentActive || TaskModule == nullptr) return false;
	if(!InTask) InTask = GetTask(InInteractor, ETaskStage::Deliverable);
	const FGuid PlayerId = InInteractor && InInteractor->GetWorld() == GetWorld() && InInteractor->Implements<USceneActorInterface>() ? ISceneActorInterface::Execute_GetActorID(InInteractor) : FGuid();
	if (!PlayerId.IsValid() || !InTask || !InTask->GetTaskAsset() || InTask->GetTaskAsset()->OwnerActorID != PlayerId) return false;
	const bool bSucceeded = TaskModule->TurnInTask(InTask, GetOwner());
	RefreshTaskMarker();
	return bSucceeded;
}

void UTaskComponent::SetAgentActive(bool bInActive)
{
	bAgentActive = bInActive;
	if(!bAgentActive)
	{
		for(UTaskAsset* Asset : GetRuntimeAssets())
		{
			if(Asset && GetOwner()) Asset->AgentLocation = GetOwner()->GetActorLocation();
		}
	}
	RefreshTaskMarker();
}

bool UTaskComponent::HasAvailableTask(AActor* InInteractor) const
{
	return bAgentActive && !GetOfferedAssets(InInteractor).IsEmpty();
}

void UTaskComponent::OnTaskAssetsChanged()
{
	RefreshTaskMarker();
	if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
}

void UTaskComponent::RefreshTaskMarker()
{
    // TaskPresentation publishes the owner-scoped deliverable marker; this
    // component only reports that the agent's options may have changed.
    if (USceneModule* Scene = USceneModule::GetPtr(GetWorld()); Scene && MarkerID.IsValid()) Scene->RemoveMarker(MarkerID);
    LastMarkerTag = FGameplayTag();
    for (UTaskAsset* Asset : GetRuntimeAssets())
        if (Asset && GetOwner()) Asset->AgentLocation = GetOwner()->GetActorLocation();
    bHasAvailableTask = bAgentActive && !ResolvedTaskAssets.IsEmpty();
    OnTaskAvailabilityChanged.Broadcast(bHasAvailableTask);
}
