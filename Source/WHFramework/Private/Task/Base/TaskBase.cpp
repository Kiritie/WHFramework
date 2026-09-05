// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Base/TaskBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Task/EventHandle_TaskCompleted.h"
#include "Event/Handle/Task/EventHandle_TaskEntered.h"
#include "Event/Handle/Task/EventHandle_TaskExecuted.h"
#include "Event/Handle/Task/EventHandle_TaskLeaved.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/Handle/Task/EventHandle_TaskStateChanged.h"
#include "Task/TaskModule.h"
#include "Task/TaskModuleStatics.h"
#include "Task/Base/TaskAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"

UTaskBase::UTaskBase()
{
	TaskGUID = FGuid::NewGuid().ToString();
	TaskDisplayName = FText::FromString(TEXT("Task Base"));
	TaskDescription = FText::GetEmpty();

	TaskIndex = 0;
	TaskHierarchy = 0;
	TaskState = ETaskState::None;

	TaskEnterType = ETaskEnterType::Procedure;
	TaskExecuteResult = ETaskExecuteResult::None;

	TaskExecuteType = ETaskExecuteType::Procedure;
	AutoExecuteTaskTime = 0.f;

	TaskLeaveType = ETaskLeaveType::Procedure;
	AutoLeaveTaskTime = 0.f;

	TaskCompleteType = ETaskCompleteType::Procedure;
	AutoCompleteTaskTime = 0.f;
	TaskGuideType = ETaskGuideType::None;
	TaskGuideIntervalTime = 0.f;
	
	bTaskSkipAble = false;

	bMergeSubTask = false;

	SubTasks = TArray<UTaskBase*>();

	RootTask = nullptr;
	ParentTask = nullptr;

	bRuntimeSelected = false;
}

#if WITH_EDITOR
void UTaskBase::OnGenerate()
{
	
}

void UTaskBase::OnUnGenerate()
{
	
}
#endif

void UTaskBase::OnStateChanged(ETaskState InTaskState)
{
	OnTaskStateChanged.Broadcast(InTaskState);
	K2_OnStateChanged(InTaskState);

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskStateChanged::StaticClass(), this, {this});
}

void UTaskBase::OnInitialize()
{
	for (auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->RootTask = IsRootTask() ? this : RootTask;
			Iter->ParentTask = this;
			Iter->OnInitialize();
		}
	}

	K2_OnInitialize();
}

void UTaskBase::OnRestore()
{
	OnSuspend();
	TaskExecuteResult = ETaskExecuteResult::None;
	ObjectiveProgress.Reset();
	bRewardsGranted = false;
	TaskState = ETaskState::None;
	OnStateChanged(TaskState);

	K2_OnRestore();

	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->OnRestore();
		}
	}
}

void UTaskBase::OnEnter()
{
	ClearTaskTimers();
	TaskExecuteResult = ETaskExecuteResult::None;
	TaskState = ETaskState::Entered;
	OnStateChanged(TaskState);

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	TaskExecuteResult = ETaskExecuteResult::None;

	WHDebug(FString::Printf(TEXT("进入任务: %s"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter();
	if (TaskState != ETaskState::Entered) return;

	switch(TaskGuideType)
	{
		case ETaskGuideType::TimerOnce:
		{
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UTaskBase::Guide, TaskGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskEntered::StaticClass(), this, {this});

	if(bMergeSubTask)
	{
		for (auto Iter : SubTasks)
		{
			if(Iter)
			{
				Iter->Enter();
			}
		}
	}

	if(TaskExecuteType == ETaskExecuteType::Automatic && TaskState == ETaskState::Entered)
	{
		if(AutoExecuteTaskTime > 0.f)
		{
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(AutoExecuteTimerHandle, this, &UTaskBase::Execute, AutoExecuteTaskTime, false);
		}
		else
		{
			Execute();
		}
	}
}

void UTaskBase::OnRefresh()
{
	if (bTaskTickEnabled) K2_OnRefresh();

	for (auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Refresh();
		}
	}
}

void UTaskBase::OnGuide()
{
	switch(TaskGuideType)
	{
		case ETaskGuideType::TimerLoop:
		{
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UTaskBase::Guide, TaskGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	K2_OnGuide();
}

void UTaskBase::OnExecute()
{
	TaskState = ETaskState::Executing;
	OnStateChanged(TaskState);

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	K2_OnExecute();
	if (TaskState != ETaskState::Executing) return;

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskExecuted::StaticClass(), this, {this});

	if(TaskState == ETaskState::Executing)
	{
		switch(TaskCompleteType)
		{
			case ETaskCompleteType::Automatic:
			{
				if(!HasSubTask(false) && Objectives.IsEmpty())
				{
					if(AutoCompleteTaskTime > 0.f)
					{
						if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UTaskBase::Complete, ETaskExecuteResult::Succeed), AutoCompleteTaskTime, false);
					}
					else
					{
						Complete();
					}
				}
				break;
			}
			case ETaskCompleteType::Skip:
			{
				Complete(ETaskExecuteResult::Skipped);
			}
			default: break;
		}
	}
}

void UTaskBase::OnComplete(ETaskExecuteResult InTaskExecuteResult)
{
	TaskExecuteResult = InTaskExecuteResult;
	TaskState = ETaskState::Completed;
	ClearTaskTimers();
	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Complete(InTaskExecuteResult);
		}
	}

	OnStateChanged(TaskState);

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
	
	K2_OnComplete(InTaskExecuteResult);

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskCompleted::StaticClass(), this, {this});

	if(InTaskExecuteResult != ETaskExecuteResult::Skipped)
	{
		WHDebug(FString::Printf(TEXT("任务%s: %s"), TaskExecuteResult != ETaskExecuteResult::Skipped ? TEXT("完成") : TEXT("失败"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Green, 5.f);
	}

	if(TaskLeaveType == ETaskLeaveType::Automatic && TaskState == ETaskState::Completed)
	{
		if(TaskExecuteResult != ETaskExecuteResult::Skipped && AutoLeaveTaskTime > 0.f)
		{
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, this, &UTaskBase::Leave, AutoLeaveTaskTime, false);
		}
		else
		{
			Leave();
		}
	}
}

void UTaskBase::OnLeave()
{
	TaskState = ETaskState::Leaved;
	ClearTaskTimers();
	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Leave();
		}
	}

	GrantRewards();
	OnStateChanged(TaskState);

	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WHDebug(FString::Printf(TEXT("%s任务: %s"), TaskExecuteResult != ETaskExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave();

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskLeaved::StaticClass(), this, {this});
}

UTaskAsset* UTaskBase::GetTaskAsset() const
{
	return GetTypedOuter<UTaskAsset>();
}

bool UTaskBase::IsCurrent() const
{
	return UTaskModuleStatics::GetCurrentTask() == this;
}

bool UTaskBase::IsEntered() const
{
	return TaskState == ETaskState::Entered || TaskState == ETaskState::Executing;
}

bool UTaskBase::IsExecuting() const
{
	return TaskState == ETaskState::Executing;
}

bool UTaskBase::IsCompleted(bool bCheckSubs) const
{
	return (TaskState == ETaskState::Completed || TaskState == ETaskState::Leaved) && (!bCheckSubs || IsAllSubCompleted());
}

bool UTaskBase::IsSucceed(bool bCheckSubs) const
{
	return (TaskExecuteResult == ETaskExecuteResult::Succeed || TaskExecuteResult == ETaskExecuteResult::Skipped) && (!bCheckSubs || IsAllSubSucceed());
}

bool UTaskBase::IsLeaved(bool bCheckSubs) const
{
	return (TaskState == ETaskState::None || TaskState == ETaskState::Leaved) && (!bCheckSubs || IsAllSubLeaved());
}

bool UTaskBase::CheckTaskCondition_Implementation(FString& OutInfo) const
{
	return true;
}

bool UTaskBase::CheckTaskSkipAble_Implementation(FString& OutInfo) const
{
	return bTaskSkipAble;
}

float UTaskBase::CheckTaskProgress_Implementation(FString& OutInfo) const
{
	float Progress = 0.f;
	if (!Objectives.IsEmpty())
	{
		int32 Current = 0;
		int32 Required = 0;
		for (const FTaskObjective& Objective : Objectives)
		{
			if (Objective.bOptional) continue;
			Required += FMath::Max(1, Objective.RequiredCount);
			Current += FMath::Clamp(GetObjectiveProgress(Objective.ObjectiveID), 0, FMath::Max(1, Objective.RequiredCount));
		}
		OutInfo = FString::Printf(TEXT("%d/%d"), Current, Required);
		return Required > 0 ? (float)Current / Required : 1.f;
	}
	if(HasSubTask())
	{
		int32 Num = 0;
		for(auto Iter : SubTasks)
		{
			if(Iter && Iter->IsCompleted())
			{
				Num++;
			}
		}
		Progress = (float)Num / SubTasks.Num();
		OutInfo = FString::Printf(TEXT("%d/%d"), Num, SubTasks.Num());
	}
	return Progress;
}

bool UTaskBase::IsParentOf(UTaskBase* InTask) const
{
	if(InTask && InTask->ParentTask)
	{
		if(InTask->ParentTask == this) return true;
		return IsParentOf(InTask->ParentTask);
	}
	return false;
}

void UTaskBase::Restore()
{
	UTaskModuleStatics::RestoreTask(this);
}

void UTaskBase::Enter(bool bSetAsCurrent)
{
	UTaskModuleStatics::EnterTask(this, bSetAsCurrent);
}

void UTaskBase::Refresh()
{
	UTaskModuleStatics::RefreshTask(this);
}

void UTaskBase::Restate()
{
	OnStateChanged(TaskState);
}

void UTaskBase::Guide()
{
	UTaskModuleStatics::GuideTask(this);
}

void UTaskBase::Execute()
{
	UTaskModuleStatics::ExecuteTask(this);
}

void UTaskBase::Complete(ETaskExecuteResult InTaskExecuteResult)
{
	UTaskModuleStatics::CompleteTask(this, InTaskExecuteResult);
}

void UTaskBase::Leave()
{
	UTaskModuleStatics::LeaveTask(this);
}

void UTaskBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << TaskState;
			Ar << TaskExecuteResult;
			Ar << ObjectiveProgress;
			Ar << bRewardsGranted;
		}
		else if(Ar.IsSaving())
		{
			Ar << TaskState;
			Ar << TaskExecuteResult;
			Ar << ObjectiveProgress;
			Ar << bRewardsGranted;
		}
	}
}

void UTaskBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
}

FSaveData* UTaskBase::ToData()
{
	CachedSaveData = FSaveData();
	return &CachedSaveData;
}

void UTaskBase::ClearTaskTimers()
{
	if (UWorld* World = GetWorld())
	{
		auto& Timers = World->GetTimerManager();
		Timers.ClearTimer(AutoExecuteTimerHandle);
		Timers.ClearTimer(AutoCompleteTimerHandle);
		Timers.ClearTimer(AutoLeaveTimerHandle);
		Timers.ClearTimer(StartGuideTimerHandle);
	}
}

void UTaskBase::OnSuspend()
{
	ClearTaskTimers();
	K2_OnSuspend();
}

void UTaskBase::OnResume()
{
	K2_OnResume();
}

void UTaskBase::BeginDestroy()
{
	ClearTaskTimers();
	Super::BeginDestroy();
}

void UTaskBase::SetTaskTimersPaused(bool bPaused)
{
	if (UWorld* World = GetWorld())
	{
		for (const FTimerHandle Handle : {AutoExecuteTimerHandle, AutoCompleteTimerHandle, AutoLeaveTimerHandle, StartGuideTimerHandle})
		{
			if (bPaused) World->GetTimerManager().PauseTimer(Handle);
			else World->GetTimerManager().UnPauseTimer(Handle);
		}
	}
}

FTaskRuntimeSaveData UTaskBase::CaptureRuntimeData()
{
	FTaskRuntimeSaveData Data;
	Data.Archive = GetSaveDataRef<FSaveData>(true);
	if (UWorld* World = GetWorld())
	{
		auto& Timers = World->GetTimerManager();
		Data.ExecuteRemaining = Timers.GetTimerRemaining(AutoExecuteTimerHandle);
		Data.CompleteRemaining = Timers.GetTimerRemaining(AutoCompleteTimerHandle);
		Data.LeaveRemaining = Timers.GetTimerRemaining(AutoLeaveTimerHandle);
		Data.GuideRemaining = Timers.GetTimerRemaining(StartGuideTimerHandle);
	}
	return Data;
}

void UTaskBase::ResumeRuntimeData(const FTaskRuntimeSaveData& Data)
{
	ClearTaskTimers();
	if (UWorld* World = GetWorld())
	{
		auto& Timers = World->GetTimerManager();
		if (TaskState == ETaskState::Entered && Data.ExecuteRemaining >= 0.f)
			Timers.SetTimer(AutoExecuteTimerHandle, this, &UTaskBase::Execute, FMath::Max(Data.ExecuteRemaining, KINDA_SMALL_NUMBER), false);
		if (TaskState == ETaskState::Executing && Data.CompleteRemaining >= 0.f)
			Timers.SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UTaskBase::Complete, ETaskExecuteResult::Succeed), FMath::Max(Data.CompleteRemaining, KINDA_SMALL_NUMBER), false);
		if (TaskState == ETaskState::Completed && Data.LeaveRemaining >= 0.f)
			Timers.SetTimer(AutoLeaveTimerHandle, this, &UTaskBase::Leave, FMath::Max(Data.LeaveRemaining, KINDA_SMALL_NUMBER), false);
		if (IsEntered() && Data.GuideRemaining >= 0.f)
			Timers.SetTimer(StartGuideTimerHandle, this, &UTaskBase::Guide, FMath::Max(Data.GuideRemaining, KINDA_SMALL_NUMBER), false);
	}
	OnResume();
	OnStateChanged(TaskState);
}

bool UTaskBase::HasSubTask(bool bIgnoreMerge) const
{
	return SubTasks.Num() > 0 && (bIgnoreMerge || !bMergeSubTask);
}

bool UTaskBase::IsSubOf(UTaskBase* InTask) const
{
	if(InTask && ParentTask)
	{
		if(InTask == ParentTask) return true;
		return ParentTask->IsSubOf(InTask);
	}
	return false;
}

bool UTaskBase::IsAllSubEntered() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && !Iter->IsEntered())
		{
			return false;
		}
	}
	return true;
}

bool UTaskBase::IsAllSubCompleted() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && !Iter->IsCompleted())
		{
			return false;
		}
	}
	return true;
}

bool UTaskBase::IsAllSubSucceed() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && !Iter->IsSucceed())
		{
			return false;
		}
	}
	return true;
}

bool UTaskBase::IsAllSubLeaved() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && !Iter->IsLeaved())
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
bool UTaskBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		const FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskExecuteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskCompleteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskGuideType))
		{
			return !HasSubTask(false);
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoExecuteTaskTime))
		{
			return TaskExecuteType == ETaskExecuteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoCompleteTaskTime))
		{
			return TaskCompleteType == ETaskCompleteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoLeaveTaskTime))
		{
			return TaskLeaveType == ETaskLeaveType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskGuideIntervalTime))
		{
			return !HasSubTask(false) && (TaskGuideType == ETaskGuideType::TimerOnce || TaskGuideType == ETaskGuideType::TimerLoop);
		}
	}

	return Super::CanEditChange(InProperty);
}

void UTaskBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, SubTasks) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, bMergeSubTask))
		{
			if(HasSubTask(false))
			{
				TaskExecuteType = ETaskExecuteType::Automatic;
				AutoExecuteTaskTime = 0.f;
				TaskCompleteType = ETaskCompleteType::Procedure;
				AutoCompleteTaskTime = 0.f;
				TaskGuideType = ETaskGuideType::None;
				TaskGuideIntervalTime = 0.f;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

ETaskStage UTaskBase::GetTaskStage() const
{
	switch (TaskState)
	{
		case ETaskState::None:
		{
			FString Info;
			return ArePrerequisitesMet() && CheckTaskCondition(Info) ? ETaskStage::Available : ETaskStage::Locked;
		}
		case ETaskState::Entered:
		case ETaskState::Executing: return ETaskStage::Active;
		case ETaskState::Completed: return TaskExecuteResult == ETaskExecuteResult::Failed ? ETaskStage::Failed : ETaskStage::ReadyToTurnIn;
		case ETaskState::Leaved: return TaskExecuteResult == ETaskExecuteResult::Failed ? ETaskStage::Failed : ETaskStage::Finished;
		default: return ETaskStage::Locked;
	}
}

bool UTaskBase::ArePrerequisitesMet() const
{
	if (Prerequisites.IsEmpty()) return true;
	if (!UTaskModule::IsValid()) return false;
	for (const FTaskReference& Reference : Prerequisites)
	{
		const UTaskBase* Task = UTaskModule::Get().ResolveTask(Reference);
		if (!Task || Task->TaskState != ETaskState::Leaved || !Task->IsSucceed()) return false;
	}
	return true;
}

int32 UTaskBase::GetObjectiveProgress(FName ObjectiveID) const
{
	return ObjectiveProgress.FindRef(ObjectiveID);
}

bool UTaskBase::AreObjectivesCompleted() const
{
	for (const FTaskObjective& Objective : Objectives)
	{
		if (!Objective.bOptional && GetObjectiveProgress(Objective.ObjectiveID) < FMath::Max(1, Objective.RequiredCount)) return false;
	}
	return true;
}

void UTaskBase::ApplyObjectiveEvent(FGameplayTag EventTag, FGameplayTag TargetTag, int32 Count, FPrimaryAssetId TargetAssetID)
{
	if (!IsExecuting() || Count <= 0) return;
	bool bChanged = false;
	for (const FTaskObjective& Objective : Objectives)
	{
		if (Objective.EventTag != EventTag || (Objective.TargetTag.IsValid() && !TargetTag.MatchesTag(Objective.TargetTag))) continue;
		if (Objective.TargetAssetID.IsValid() && Objective.TargetAssetID != TargetAssetID) continue;
		int32& Progress = ObjectiveProgress.FindOrAdd(Objective.ObjectiveID);
		const int32 NewProgress = (int32)FMath::Min<int64>((int64)Progress + Count, FMath::Max(1, Objective.RequiredCount));
		bChanged |= NewProgress != Progress;
		Progress = NewProgress;
	}
	if (bChanged)
	{
		Restate();
		if (AreObjectivesCompleted()) Complete();
	}
}

void UTaskBase::OnReward()
{
	K2_OnReward();
}

void UTaskBase::GrantRewards()
{
	if (bRewardsGranted || TaskState != ETaskState::Leaved || TaskExecuteResult != ETaskExecuteResult::Succeed) return;
	bRewardsGranted = true;
	OnReward();
}
