#include "Task/TaskExecutionTypes.h"
#include "Task/TaskObjectiveTypes.h"
#include "Task/TaskPresentationTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Task/Event_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"

bool UTaskModule::RemoveTaskMarkers(const UTaskAsset* InAsset)
{
	USceneModule* Scene = USceneModule::GetPtr(GetWorld());
	if(!Scene)
	{
		if(!InAsset) TaskMarkerIDs.Reset();
		return true;
	}
	TSet<FGuid> Removed;
	if(InAsset)
	{
		for(const auto& Pair : InAsset->TaskMap) Removed.Add(MakeTaskMarkerID(InAsset, Pair.Key));
	}
	else
	{
		Removed = TaskMarkerIDs;
	}
	for(const FGuid& MarkerID : Removed)
	{
		TaskMarkerIDs.Remove(MarkerID);
		Scene->RemoveMarker(MarkerID);
	}
	return true;
}

void UTaskModule::RefreshTaskMarkers()
{
	if(bLoadingTasks || bClearingRuntimeAssets || bRefreshingTaskMarkers) return;
	TGuardValue<bool> RefreshGuard(bRefreshingTaskMarkers, true);
	USceneModule* SceneModule = USceneModule::GetPtr(GetWorld());
	if(!SceneModule)
	{
		OnTaskAssetsChanged.Broadcast();
		return;
	}
	USceneModule& Scene = *SceneModule;
	TSet<FGuid> DesiredIDs;
	TArray<FSceneMarker> DesiredMarkers;
	for(UTaskAsset* Asset : Assets)
	{
		if(!Asset) continue;
		if (Asset->OwnerActorID.IsValid() && Asset->AgentID.IsValid())
		{
			for (UTaskBase* Root : Asset->RootTasks)
			{
				if (!Root || Root->GetTaskStage() != ETaskStage::Deliverable || !Root->bRequireExplicitTurnIn) continue;
				FSceneMarker AgentMarker;
				AgentMarker.MarkerID = MakeTaskMarkerID(Asset, TEXT("__deliverable_agent"));
				AgentMarker.MarkerTag = TaskTags::Marker_Deliverable;
				AgentMarker.DisplayName = Root->TaskDisplayName;
				AgentMarker.Icon = Root->Marker.Icon;
				AgentMarker.Color = FLinearColor(1.f, 0.82f, 0.15f);
				AgentMarker.Location = Asset->AgentLocation;
				AgentMarker.Offset = FVector(0.f, 0.f, 120.f);
				AgentMarker.ActorID = Asset->AgentID;
				AgentMarker.OwnerPlayerId = Asset->OwnerActorID;
				AgentMarker.Channels = static_cast<int32>(ESceneMarkerChannel::World);
				AgentMarker.Priority = 200;
				DesiredIDs.Add(AgentMarker.MarkerID);
				DesiredMarkers.Add(MoveTemp(AgentMarker));
				break;
			}
		}
		for(const auto& Pair : Asset->TaskMap)
		{
			UTaskBase* Task = Pair.Value;
			if(!Task || !Task->Marker.bVisible) continue;
			const ETaskStage Stage = Task->GetTaskStage();
			if(Stage != ETaskStage::Active && Stage != ETaskStage::Deliverable) continue;
			FTaskTarget Target = Task->Target;
			if(Stage == ETaskStage::Deliverable && Task->bRequireExplicitTurnIn && Asset->AgentID.IsValid())
			{
				Target.ActorID = Asset->AgentID;
				Target.AreaName = NAME_None;
				Target.Location = Asset->AgentLocation;
			}
			if(!Target.IsValid()) continue;
			FSceneMarker Marker;
			Marker.MarkerID = MakeTaskMarkerID(Asset, Pair.Key);
			Marker.MarkerTag = Stage == ETaskStage::Deliverable ? TaskTags::Marker_Deliverable : TaskTags::Marker_Active;
			Marker.DisplayName = Task->TaskDisplayName;
			Marker.Icon = Task->Marker.Icon;
			Marker.Color = Task->Marker.Color;
			Marker.Location = Target.Location;
			Marker.Offset = Target.Offset;
			Marker.ActorID = Target.ActorID;
			Marker.AreaName = Target.AreaName;
			Marker.Priority = Task->Marker.Priority;
			Marker.Channels = Task->Marker.Channels;
			Marker.OwnerPlayerId = Asset->OwnerActorID;
			Marker.bNavigationOnlyWhenTracked = Task->Marker.bOnlyShowNavigationWhenTracked;
			DesiredIDs.Add(Marker.MarkerID);
			DesiredMarkers.Add(MoveTemp(Marker));
		}
	}
	for (const FSceneMarker& Marker : DesiredMarkers)
	{
		TaskMarkerIDs.Add(Marker.MarkerID);
		const bool bUpdated = Scene.UpdateMarker(Marker);
		if (!bUpdated)
		{
			Scene.AddMarker(Marker);
		}
	}
	const TSet<FGuid> PreviousIDs = TaskMarkerIDs;
	for(const FGuid& MarkerID : PreviousIDs)
	{
		if(!DesiredIDs.Contains(MarkerID))
		{
			TaskMarkerIDs.Remove(MarkerID);
			Scene.RemoveMarker(MarkerID);
		}
	}
	OnTaskAssetsChanged.Broadcast();
}

TArray<FTaskAssetPresentation> UTaskModule::CaptureOwnerPresentation(const FGuid& InOwnerActorID) const
{
	TArray<FTaskAssetPresentation> Result;
	if(!InOwnerActorID.IsValid()) return Result;
	for(const UTaskAsset* Asset : Assets)
	{
		if(!Asset || Asset->OwnerActorID != InOwnerActorID || Asset->bPresentationOnly) continue;
		FTaskAssetPresentation& Snapshot = Result.AddDefaulted_GetRef();
		Snapshot.Asset.Asset = Cast<UTaskAsset>(Asset->SourceObject);
		Snapshot.Asset.InstanceID = Asset->InstanceID;
		Snapshot.Asset.AgentID = Asset->AgentID;
		Snapshot.Asset.OwnerActorID = Asset->OwnerActorID;
		Snapshot.Asset.InstanceKey = Asset->InstanceKey;
		Snapshot.Asset.AgentLocation = Asset->AgentLocation;
		for(const auto& Pair : Asset->TaskMap)
		{
			if(Pair.Value) Snapshot.Tasks.Add(Pair.Value->CapturePresentation());
		}
		Snapshot.Tasks.Sort([](const FTaskPresentationState& A, const FTaskPresentationState& B) { return A.TaskGUID < B.TaskGUID; });
	}
	Result.Sort([](const FTaskAssetPresentation& A, const FTaskAssetPresentation& B) { return A.Asset.InstanceID < B.Asset.InstanceID; });
	return Result;
}

void UTaskModule::ApplyOwnerPresentation(const FGuid& InOwnerActorID, const TArray<FTaskAssetPresentation>& InAssets)
{
	if(!GetWorld() || GetWorld()->GetNetMode() != NM_Client || !InOwnerActorID.IsValid()) return;
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	TSet<FGuid> Present;
	for(const FTaskAssetPresentation& Snapshot : InAssets)
	{
		if(Snapshot.Asset.OwnerActorID != InOwnerActorID) continue;
		UTaskAsset* Source = Snapshot.Asset.Asset.LoadSynchronous();
		UTaskAsset* Asset = AddAssetInternal(Source, Snapshot.Asset.InstanceID, Snapshot.Asset.AgentID, InOwnerActorID, true);
		if(!Asset || !Asset->bPresentationOnly) continue;
		Present.Add(Asset->InstanceID);
		Asset->AgentLocation = Snapshot.Asset.AgentLocation;
		Asset->InstanceKey = Snapshot.Asset.InstanceKey;
		for(const FTaskPresentationState& State : Snapshot.Tasks)
		{
			if(UTaskBase* Task = Asset->TaskMap.FindRef(State.TaskGUID)) Task->ApplyPresentation(State);
		}
	}
	const TArray<UTaskAsset*> Previous = Assets;
	for(UTaskAsset* Asset : Previous)
	{
		if(Asset && Asset->bPresentationOnly && Asset->OwnerActorID == InOwnerActorID && !Present.Contains(Asset->InstanceID)) RemoveAsset(Asset);
	}
	RequestTaskMarkersRefresh();
}

bool UTaskModule::IsVisibleToPlayer(const UTaskAsset* InAsset, FGuid InPlayerId) const
{
	return InAsset && InPlayerId.IsValid() && (!InAsset->OwnerActorID.IsValid() || InAsset->OwnerActorID == InPlayerId);
}

TArray<UTaskAsset*> UTaskModule::GetVisibleAssets(FGuid InPlayerId) const
{
	TArray<UTaskAsset*> Result;
	for(UTaskAsset* Asset : Assets)
	{
		if(IsVisibleToPlayer(Asset, InPlayerId)) Result.Add(Asset);
	}
	return Result;
}
