#include "Task/TaskComponent.h"

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
	Super::BeginPlay();
	ResolveAgentID();
	if(UTaskModule::IsValid()) UTaskModule::Get().OnTaskAssetsChanged.AddUniqueDynamic(this, &UTaskComponent::OnTaskAssetsChanged);
	RefreshTaskMarker();
}

void UTaskComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(UTaskModule::IsValid())
	{
		UTaskModule::Get().OnTaskAssetsChanged.RemoveDynamic(this, &UTaskComponent::OnTaskAssetsChanged);
		for(UTaskAsset* Asset : GetRuntimeAssets())
		{
			if(Asset && GetOwner()) Asset->AgentLocation = GetOwner()->GetActorLocation();
		}
	}
	if(USceneModule::IsValid() && MarkerID.IsValid()) USceneModule::Get().RemoveMarker(MarkerID);
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
	RefreshTaskMarker();
	if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
}

TArray<UTaskAsset*> UTaskComponent::GetRuntimeAssets() const
{
	return UTaskModule::IsValid() && AgentID.IsValid() ? UTaskModule::Get().GetAssetsByAgent(AgentID) : TArray<UTaskAsset*>();
}

bool UTaskComponent::HasRuntimeAsset(UTaskAsset* InSource) const
{
	if(!InSource) return false;
	const FSoftObjectPath SourcePath(InSource);
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(Asset && Asset->SourceObject && FSoftObjectPath(Asset->SourceObject) == SourcePath) return true;
	}
	return false;
}

TArray<UTaskAsset*> UTaskComponent::GetOfferedAssets() const
{
	TArray<UTaskAsset*> Result;
	for(const TSoftObjectPtr<UTaskAsset>& AssetPtr : TaskAssets)
	{
		UTaskAsset* Asset = AssetPtr.Get();
		if(!Asset) Asset = AssetPtr.LoadSynchronous();
		if(Asset && !HasRuntimeAsset(Asset)) Result.Add(Asset);
	}
	return Result;
}

UTaskBase* UTaskComponent::GetTask(ETaskStage InStage) const
{
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(!Asset) continue;
		for(UTaskBase* Task : Asset->RootTasks)
		{
			if(Task && Task->GetTaskStage() == InStage) return Task;
		}
	}
	return nullptr;
}

UTaskBase* UTaskComponent::ResolveTask(const FTaskReference& InReference) const
{
	if(!UTaskModule::IsValid()) return nullptr;
	if(InReference.InstanceID.IsValid()) return UTaskModule::Get().ResolveTask(InReference);
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(!Asset || !Asset->SourceObject) continue;
		if(!InReference.Asset.IsNull() && FSoftObjectPath(Asset->SourceObject) != InReference.Asset.ToSoftObjectPath()) continue;
		if(InReference.TaskGUID.IsEmpty()) return Asset->RootTasks.IsEmpty() ? nullptr : Asset->RootTasks[0];
		if(UTaskBase* Task = Asset->TaskMap.FindRef(InReference.TaskGUID)) return Task;
	}
	return nullptr;
}

UTaskBase* UTaskComponent::AcceptTask(UTaskAsset* InAsset, FString InTaskGUID)
{
	if(!bAgentActive || !UTaskModule::IsValid() || (!AgentID.IsValid() && !ResolveAgentID())) return nullptr;
	if(!InAsset)
	{
		const TArray<UTaskAsset*> Offers = GetOfferedAssets();
		InAsset = Offers.IsEmpty() ? nullptr : Offers[0];
	}
	if(!InAsset || HasRuntimeAsset(InAsset)) return nullptr;
	UTaskAsset* RuntimeAsset = UTaskModule::Get().CreateAsset(InAsset, AgentID);
	if(!RuntimeAsset) return nullptr;
	RuntimeAsset->AgentLocation = GetOwner()->GetActorLocation();
	UTaskBase* Task = InTaskGUID.IsEmpty() ? (RuntimeAsset->RootTasks.IsEmpty() ? nullptr : RuntimeAsset->RootTasks[0]) : RuntimeAsset->TaskMap.FindRef(InTaskGUID);
	if(!Task)
	{
		UTaskModule::Get().RemoveAsset(RuntimeAsset);
		return nullptr;
	}
	UTaskModule::Get().EnterTask(Task, true);
	if(Task->GetTaskState() == ETaskState::None)
	{
		UTaskModule::Get().RemoveAsset(RuntimeAsset);
		return nullptr;
	}
	OnTaskAccepted.Broadcast(Task);
	RefreshTaskMarker();
	return Task;
}

bool UTaskComponent::TurnInTask(UTaskBase* InTask)
{
	if(!bAgentActive || !UTaskModule::IsValid()) return false;
	if(!InTask) InTask = GetTask(ETaskStage::Deliverable);
	const bool bSucceeded = UTaskModule::Get().TurnInTask(InTask, GetOwner());
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
		if(USceneModule::IsValid() && MarkerID.IsValid()) USceneModule::Get().RemoveMarker(MarkerID);
		LastMarkerTag = FGameplayTag();
	}
	else RefreshTaskMarker();
}

void UTaskComponent::OnTaskAssetsChanged()
{
	RefreshTaskMarker();
	if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
}

void UTaskComponent::RefreshTaskMarker()
{
	if(!bAgentActive || !USceneModule::IsValid() || (!AgentID.IsValid() && !ResolveAgentID())) return;
	for(UTaskAsset* Asset : GetRuntimeAssets())
	{
		if(Asset && GetOwner()) Asset->AgentLocation = GetOwner()->GetActorLocation();
	}
	FGameplayTag MarkerTag;
	TSoftObjectPtr<UTexture2D> Icon;
	FLinearColor Color;
	FText DisplayName;
	if(UTaskBase* Task = GetTask(ETaskStage::Deliverable))
	{
		MarkerTag = TaskTags::Marker_Deliverable;
		Icon = DeliverableMarkerIcon;
		Color = FLinearColor(1.f, 0.82f, 0.15f);
		DisplayName = Task->TaskDisplayName;
	}
	else if(!GetOfferedAssets().IsEmpty())
	{
		MarkerTag = TaskTags::Marker_Available;
		Icon = AvailableMarkerIcon;
		Color = FLinearColor(1.f, 0.75f, 0.05f);
		DisplayName = NSLOCTEXT("Task", "AvailableTask", "Available Task");
	}
	else
	{
		if(MarkerID.IsValid()) USceneModule::Get().RemoveMarker(MarkerID);
		if(LastMarkerTag.IsValid())
		{
			if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
		}
		LastMarkerTag = FGameplayTag();
		return;
	}
	if(LastMarkerTag != MarkerTag)
	{
		if(UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->NotifyOptionsChanged();
	}
	FSceneMarker Marker;
	Marker.MarkerID = MarkerID;
	Marker.MarkerTag = MarkerTag;
	Marker.DisplayName = DisplayName;
	Marker.Icon = Icon;
	Marker.Color = Color;
	Marker.Location = GetOwner()->GetActorLocation();
	Marker.Offset = MarkerOffset;
	Marker.ActorID = AgentID;
	Marker.Channels = static_cast<int32>(ESceneMarkerChannel::World);
	Marker.Priority = 200;
	if(!USceneModule::Get().UpdateMarker(Marker)) USceneModule::Get().AddMarker(Marker);
	LastMarkerTag = MarkerTag;
}
