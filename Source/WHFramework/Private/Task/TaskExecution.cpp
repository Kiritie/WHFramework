#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskObjectiveTypes.h"
#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/Events/Task/Event_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"

void UTaskModule::RestoreTask(UTaskBase* InTask)
{
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskAsset()->bPresentationOnly) return;
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		ClearCurrentTasksForSubtree(InTask);
		InTask->OnRestore();
		RequestTaskMarkersRefresh();
	}
}

void UTaskModule::RestoreTaskByGUID(const FString& InTaskGUID)
{
	RestoreTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::EnterTask(UTaskBase* InTask, bool bSetAsCurrent)
{
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask) return;
	UTaskAsset* Asset = InTask->GetTaskAsset();
	if (!Asset || Asset->bPresentationOnly || InTask->TaskEnterType == ETaskEnterType::None || InTask->IsCompleted()) return;
	const ETaskState PreviousState = InTask->TaskState;
	if (InTask->TaskState == ETaskState::None)
	{
		if (InTask->ParentTask)
		{
			if (!InTask->ParentTask->IsEntered()) return;
			if (InTask->ParentTask->bSequentialSubTasks)
			{
				for (UTaskBase* Sibling : InTask->ParentTask->SubTasks)
				{
					if (Sibling == InTask) break;
					if (!Sibling->IsLeaved() || !Sibling->IsSucceed()) return;
				}
			}
		}
		FString FailedInfo;
		if (!InTask->ArePrerequisitesMet() || !InTask->CheckTaskCondition(FailedInfo)) return;
		InTask->OnEnter();
		if (bSetAsCurrent && !InTask->IsLeaved()) SetCurrentTask(Asset->OwnerActorID, InTask);
	}
	if (InTask->IsEntered() && InTask->HasSubTask(false))
	{
		for (UTaskBase* Child : InTask->SubTasks)
		{
			if (Child && Child->TaskEnterType == ETaskEnterType::Automatic) EnterTask(Child, bSetAsCurrent);
		}
	}
	if(InTask->TaskState != PreviousState) RequestTaskMarkersRefresh();
}

void UTaskModule::EnterTaskByGUID(const FString& InTaskGUID, bool bSetAsCurrent)
{
	EnterTask(GetTaskByGUID(InTaskGUID), bSetAsCurrent);
}

void UTaskModule::RefreshTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask) return;
	UTaskAsset* Asset = InTask->GetTaskAsset();
	if (!Asset || Asset->bPresentationOnly) return;
	if (InTask->IsEntered())
	{
		if(InTask->IsExecuting() && InTask->bTaskTickEnabled) InTask->OnRefresh();
		for(UTaskBase* Child : InTask->SubTasks)
		{
			if (Child) RefreshTask(Child);
		}
	}
}

void UTaskModule::RefreshTaskByGUID(const FString& InTaskGUID)
{
	RefreshTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::GuideTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskAsset()->bPresentationOnly) return;
	if(InTask && InTask->IsEntered())
	{
		InTask->OnGuide();
	}
}

void UTaskModule::GuideTaskByGUID(const FString& InTaskGUID)
{
	GuideTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::ExecuteTask(UTaskBase* InTask)
{
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskAsset()->bPresentationOnly) return;
	if(InTask && InTask->GetTaskState() == ETaskState::Entered)
	{
		InTask->OnExecute();
		RequestTaskMarkersRefresh();
	}
}

void UTaskModule::ExecuteTaskByGUID(const FString& InTaskGUID)
{
	ExecuteTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskAsset()->bPresentationOnly) return;
	if(!InTask || InTaskExecuteResult == ETaskExecuteResult::None) return;

	if(InTaskExecuteResult == ETaskExecuteResult::Skipped)
	{
		FString SkipInfo;
		if(!InTask->CheckTaskSkipAble(SkipInfo))
		{
			return;
		}
	}
	if(InTask->IsEntered() && !InTask->IsCompleted())
	{
		InTask->OnComplete(InTaskExecuteResult);
		if(InTask->TaskState == ETaskState::Completed && InTask->bRequireExplicitTurnIn && InTaskExecuteResult == ETaskExecuteResult::Succeed)
		{
			if (InTask->GetTaskAsset()) SetCurrentTask(InTask->GetTaskAsset()->OwnerActorID, InTask);
		}
		if(InTask->ParentTask && InTask->ParentTask->HasSubTask() && InTask->ParentTask->AreSubTasksReadyToComplete())
		{
			CompleteTask(InTask->ParentTask, InTask->ParentTask->IsAllSubSucceed() ? ETaskExecuteResult::Succeed : ETaskExecuteResult::Failed);
		}
		RequestTaskMarkersRefresh();
	}
}

void UTaskModule::CompleteTaskByGUID(const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult)
{
	CompleteTask(GetTaskByGUID(InTaskGUID), InTaskExecuteResult);
}

void UTaskModule::LeaveTask(UTaskBase* InTask)
{
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskAsset()->bPresentationOnly) return;
	if(!InTask) return;

	if(!InTask->IsLeaved())
	{
		if (InTask->bRequireExplicitTurnIn && InTask->TaskExecuteResult != ETaskExecuteResult::Failed && TurningInTask != InTask) return;
		UTaskBase* ParentTask = InTask->ParentTask;
		InTask->OnLeave();
		ClearCurrentTasksForSubtree(InTask);
		if(ParentTask && ParentTask->IsEntered() && ParentTask->HasSubTask() && ParentTask->AreSubTasksReadyToComplete())
		{
			CompleteTask(ParentTask, ParentTask->IsAllSubSucceed() ? ETaskExecuteResult::Succeed : ETaskExecuteResult::Failed);
		}
		if(ParentTask && ParentTask->IsCompleted() && ParentTask->HasSubTask() && ParentTask->IsAllSubLeaved())
		{
			LeaveTask(ParentTask);
		}
		RequestTaskMarkersRefresh();
	}
}

void UTaskModule::LeaveTaskByGUID(const FString& InTaskGUID)
{
	LeaveTask(GetTaskByGUID(InTaskGUID));
}

bool UTaskModule::IsAllTaskCompleted() const
{
	for(auto Iter1 : Assets)
	{
		for(auto Iter2 : Iter1->RootTasks)
		{
			if(!Iter2->IsCompleted(true))
			{
				return false;
			}
		}
	}
	return true;
}

void UTaskModule::SetCurrentTask(FGuid InPlayerId, UTaskBase* InTask)
{
	if (bClearingRuntimeAssets || !InPlayerId.IsValid()) return;
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	if (InTask)
	{
		InTask = ResolveRuntimeTask(InTask);
		if (!InTask) return;
	}
	if(InTask && (!InTask->GetTaskAsset() || InTask->GetTaskAsset()->OwnerActorID != InPlayerId)) return;
	if (GetCurrentTask(InPlayerId) == InTask) return;
	if (InTask) CurrentTasks.Add(InPlayerId, InTask); else CurrentTasks.Remove(InPlayerId);
	RequestTaskMarkersRefresh();
	if (UEventModule* Events = UEventModule::GetPtr(GetWorld())) Events->BroadcastEvent<FEventCurrentTaskChanged>(this, {InPlayerId, InTask});
}

void UTaskModule::ClearCurrentTasksForSubtree(const UTaskBase* InTask)
{
	if (!InTask) return;
	TArray<FGuid> Owners;
	for (const auto& Pair : CurrentTasks)
		if (Pair.Value && (Pair.Value == InTask || InTask->IsParentOf(Pair.Value))) Owners.Add(Pair.Key);
	for (const FGuid& Owner : Owners) SetCurrentTask(Owner, nullptr);
}

UTaskBase* UTaskModule::GetGuidanceTask(FGuid InPlayerId) const
{
	TFunction<UTaskBase*(UTaskBase*)> Resolve = [&](UTaskBase* Task) -> UTaskBase*
	{
		if(!Task || Task->IsLeaved()) return nullptr;
		for(UTaskBase* Child : Task->SubTasks)
		{
			if(UTaskBase* Result = Resolve(Child)) return Result;
		}
		const ETaskStage Stage = Task->GetTaskStage();
		const UTaskAsset* Asset = Task->GetTaskAsset();
		const bool bHasTarget = Task->Target.IsValid() || (Stage == ETaskStage::Deliverable && Task->bRequireExplicitTurnIn && Asset && Asset->AgentID.IsValid());
		return Task->Marker.bVisible && bHasTarget && (Stage == ETaskStage::Active || Stage == ETaskStage::Deliverable) ? Task : nullptr;
	};
	return Resolve(GetCurrentTask(InPlayerId));
}

FGuid UTaskModule::GetGuidanceMarkerId(FGuid InPlayerId) const
{
	const UTaskBase* Task = GetGuidanceTask(InPlayerId);
	const UTaskAsset* Asset = Task ? Task->GetTaskAsset() : nullptr;
	return Asset && Asset->OwnerActorID == InPlayerId ? MakeTaskMarkerID(Asset, Task->TaskGUID) : FGuid();
}

bool UTaskModule::IsExistTaskByGUID(const FString& InTaskGUID) const
{
	return GetTaskByGUID(InTaskGUID) != nullptr;
}

UTaskBase* UTaskModule::GetTaskByGUID(const FString& InTaskGUID) const
{
	UTaskBase* Result = nullptr;
	for (UTaskAsset* Asset : Assets)
	{
		if (UTaskBase* Task = Asset->TaskMap.FindRef(InTaskGUID))
		{
			if (Result) return nullptr;
			Result = Task;
		}
	}
	return Result;
}

UTaskBase* UTaskModule::ResolveTask(const FTaskReference& Reference, UTaskAsset* InContext) const
{
	FGuid InstanceID = Reference.InstanceID;
	if(!InstanceID.IsValid() && InContext)
	{
		const UObject* ContextSource = InContext->SourceObject ? InContext->SourceObject : InContext;
		if(FSoftObjectPath(ContextSource) == Reference.Asset.ToSoftObjectPath()) InstanceID = InContext->InstanceID;
	}
	for (UTaskAsset* Asset : Assets)
	{
		if (Asset->InstanceID == InstanceID && (Asset == Reference.Asset.Get() || (Asset->SourceObject && FSoftObjectPath(Asset->SourceObject) == Reference.Asset.ToSoftObjectPath())))
		{
			return Asset->TaskMap.FindRef(Reference.TaskGUID);
		}
	}
	return nullptr;
}

UTaskBase* UTaskModule::EnsureTask(const FTaskReference& Reference)
{
	if (UTaskBase* Task = ResolveTask(Reference)) return Task;
	if (UTaskAsset* Asset = Reference.Asset.LoadSynchronous()) AddAssetInternal(Asset, Reference.InstanceID, FGuid());
	return ResolveTask(Reference);
}

bool UTaskModule::TurnInTask(UTaskBase* InTask, AActor* InTarget)
{
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask || InTask->GetTaskAsset()->bPresentationOnly || InTask->TaskState != ETaskState::Completed || InTask->TaskExecuteResult != ETaskExecuteResult::Succeed) return false;
	UTaskAsset* Asset = InTask->GetTaskAsset();
	if (InTask->bRequireExplicitTurnIn)
	{
		if(!InTarget || !Asset || !Asset->AgentID.IsValid() || !InTarget->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetActorID(InTarget) != Asset->AgentID) return false;
		FText Reason;
		if(!InTask->CanTurnIn(InTarget, Reason) || !InTask->CommitTurnIn(InTarget)) return false;
	}
	TGuardValue<UTaskBase*> Guard(TurningInTask, InTask);
	LeaveTask(InTask);
	return InTask->TaskState == ETaskState::Leaved;
}

void UTaskModule::ReportTaskEvent(FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount, FPrimaryAssetId InTargetAssetID, FName InTargetName, FGuid InOwnerActorID, ETaskEventScope InScope)
{
	if (!InEventTag.IsValid() || InCount <= 0 || bLoadingTasks || ModuleState == EModuleState::Paused) return;
	BeginTaskMutation();
	ON_SCOPE_EXIT { EndTaskMutation(); };
	TArray<UTaskBase*> ExecutingTasks;
	for (UTaskAsset* Asset : Assets)
	{
		if(!Asset || Asset->bPresentationOnly || Asset->OwnerActorID != InOwnerActorID) continue;
		for (const auto& Pair : Asset->TaskMap)
		{
			if (Pair.Value->IsExecuting() && !Pair.Value->Objectives.IsEmpty()) ExecutingTasks.Add(Pair.Value);
		}
	}
	for (UTaskBase* Task : ExecutingTasks)
	{
		if (ResolveRuntimeTask(Task)) Task->ApplyObjectiveEvent(InEventTag, InTargetTag, InCount, InTargetAssetID, InTargetName, InScope);
	}
	RequestTaskMarkersRefresh();
}

void UTaskModule::BeginTaskMutation()
{
	++TaskMutationDepth;
}

void UTaskModule::EndTaskMutation()
{
	if(!ensure(TaskMutationDepth > 0)) return;
	--TaskMutationDepth;
	if(TaskMutationDepth > 0 || bLoadingTasks || bClearingRuntimeAssets || bRefreshingTaskMarkers) return;
	if(bAutomaticTaskEntryDirty && !bResolvingAutomaticTasks)
	{
		bResolvingAutomaticTasks = true;
		++TaskMutationDepth;
		do
		{
			bAutomaticTaskEntryDirty = false;
			EnterEligibleAutomaticTasks();
			if (bLoadingTasks || bClearingRuntimeAssets) break;
		}
		while(bAutomaticTaskEntryDirty);
		--TaskMutationDepth;
		bResolvingAutomaticTasks = false;
	}
	if (bLoadingTasks || bClearingRuntimeAssets) return;

	if(bTaskMarkersDirty)
	{
		bTaskMarkersDirty = false;
		RefreshTaskMarkers();
	}
}

void UTaskModule::RequestTaskMarkersRefresh()
{
	bTaskMarkersDirty = true;
	bAutomaticTaskEntryDirty = true;
	if(TaskMutationDepth == 0 && !bLoadingTasks && !bClearingRuntimeAssets && !bRefreshingTaskMarkers)
	{
		BeginTaskMutation();
		EndTaskMutation();
	}
}

void UTaskModule::EnterEligibleAutomaticTasks()
{
	for(UTaskAsset* Asset : Assets)
	{
		if(!Asset || Asset->bPresentationOnly) continue;
		for(UTaskBase* Task : Asset->RootTasks)
		{
			if (Task) EnterEligibleAutomaticTasks(Task);
		}
	}
}

void UTaskModule::EnterEligibleAutomaticTasks(UTaskBase* InTask)
{
	if(!InTask) return;
	if(InTask->TaskState == ETaskState::None && InTask->TaskEnterType == ETaskEnterType::Automatic)
	{
		const FGuid Owner = InTask->GetTaskAsset() ? InTask->GetTaskAsset()->OwnerActorID : FGuid();
		const UTaskBase* Current = GetCurrentTask(Owner);
		EnterTask(InTask, Owner.IsValid() && (!Current || Current->IsCompleted()));
	}
	if(!InTask->IsEntered()) return;
	for(UTaskBase* Child : InTask->SubTasks)
	{
		if (Child) EnterEligibleAutomaticTasks(Child);
	}
}
